#pragma once

#include <functional>
#include <atomic>

#include "static_types.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "log_storage.h"


// ------------------------------------------------------------------------------
// struct declarations
// ------------------------------------------------------------------------------

#define template_args <int get_size, int post_size, int put_size, int delete_size, int max_path_length, int max_headers, int buf_size, int message_buffers>
#define template_args_pure <get_size, post_size, put_size, delete_size, max_path_length, max_headers, buf_size, message_buffers>

constexpr std::string_view HTTP_VERSION{"HTTP/1.1"};

constexpr std::string_view STATUS_OK{"200 OK"};
constexpr std::string_view STATUS_BAD_REQUEST{"400 Bad Request"};
constexpr std::string_view STATUS_UAUTHORIZED{"401 Unauthorized"};
constexpr std::string_view STATUS_NOT_FOUND{"404 Not Found"};
constexpr std::string_view STATUS_INTERNAL_SERVER_ERROR{"500 Internal Server Error"};

struct EndpointFlags{
	bool path_match: 1 {true}; // path for endpoint has to match, not only 
};

struct header {
	std::string_view key;
	std::string_view value;
};
template<int max_headers>
struct headers {
	static_vector<header, max_headers> headers{};
	header* begin() { return headers.begin(); }
	header* end() { return headers.end(); }
	std::string_view get_header(std::string_view key) {
		for (auto & [k, value]: headers)
		if (key == k)
			return value;
		return {};
	}
};

/** @brief Tcp server that serves text data according to path specification.
  * The returned content can be freely configured via callbacks via callbacks 
  * @note The tcp server instantly discards any connection after the response was sent.*/
template<int get_size, int post_size, int put_size = 0, int delete_size = 0, int max_path_length = 256, int max_headers = 32, int buf_size = 2048, int message_buffers = 2>
struct tcp_server {
	/**
	 * @brief Struct with a full http frame for both sending and recieving.
	 * The struct has only 1 member, the `buffer` which really holds information,
	 * all other members only give a more structured view to this backing buffer.
	 */
	struct message_buffer{
		std::atomic<bool> used{};
		static_string<buf_size> buffer{};
		std::string_view method{}; // set to the method for a request http frame, else is empty and cannot be written
		std::string_view path{}; // set to the path of a request http frame, else is empty and can not be written
		std::string_view http_version{}; // version of the http protocol, normally HTTP/1.1
		std::string_view status{}; // status code followed by a space and a possibly empty reason string
		headers<max_headers> headers_view{}; // actually only contains std::string views to underlying buffer
		std::string_view body{};

		// ------------------------------------------------------
		// request functions
		// ------------------------------------------------------
		/** @brief update the headers_view and body view from this message_buffer 
		 * @note used for reading/parsing a package*/
		void req_update_structured_views();

		// ------------------------------------------------------
		// response functions
		// ------------------------------------------------------
		/** @brief Write the status line of the response message to the buffer
		  * @note Resets previously set header and body values, but will give a warning
		  * if done so */
		void res_set_status_line(std::string_view http_version, std::string_view status);
		/** @brief add a header to the headers_view (also written to the backing buffer)
		  * @note erases the body view as it becomes invalid and has to be rewritten
		  * @returns header view with the key and value at their place in  the backing buffer, if failed
		  * the string views in the header view are empty
		  * @logs Warning if add_header is called when body is not empty*/
		header res_add_header(std::string_view key, std::string_view value);
		/** @brief writes the string_view the end of the backing buffer directly after the header section
		  * and sets the internal body variable to exactly this string */
		void res_write_body(std::string_view body = {});
		void clear() { used = {}; buffer.clear(); method = {}; path = {}; http_version = {}; status = {}; headers_view.headers.clear(); body = {}; }
	};
	using endpoint_callback = std::function<void(const message_buffer &request, message_buffer& response)>;
	struct endpoint {
		EndpointFlags flags;
		std::array<char, 256> path;
		endpoint_callback callback;
	};

	int port{80};
	endpoint_callback default_endpoint_cb{};
	std::array<endpoint, get_size> get_endpoints{};
	std::array<endpoint, post_size> post_endpoints{};
	std::array<endpoint, put_size> put_endpoints{};
	std::array<endpoint, delete_size> delete_endpoints{};
	int poll_time_s{5};

	~tcp_server() { if(!closed) LogError("Tcp server not closed before destruction!"); };
	err_t start();
	err_t stop();
	
	struct tcp_pcb *server_pcb{};
	struct tcp_pcb *client_pcb{};
	bool complete{};
	bool closed{};
	std::array<message_buffer, message_buffers> send_buffers{};
	std::array<message_buffer, message_buffers> recieve_buffers{};
	int sent_len{};
	int recv_len{};
	int run_count{};

	void process_request(uint32_t recieve_buffer_idx);
	err_t send_data(uint32_t send_buffer_idx);
};

// ------------------------------------------------------------------------------
// implementations
// ------------------------------------------------------------------------------

/** @brief Contains all implementations regarding tcp server connections */
namespace tcp_server_internal {

/** @brief Extract a word from the beginning of content, never reading over newlines.
 * Also removes any whitespace in the returned word and in the changed content. */
std::string_view extract_word(std::string_view &content) {
	if (content.size() >= 2 && content[0] == '\r' && content[1] == '\n')
		return {};
	auto start_word = content.find_first_not_of(' ');
	if (start_word == std::string_view::npos) start_word = 0;
	auto end_word = content.find_first_of(" \r\n", start_word);
	auto ret = content.substr(start_word, end_word - start_word);
	auto s = content.find_first_not_of(' ', std::min(end_word, content.size() - 1));
	if (s == std::string_view::npos)
		content = {};
	else
		content = content.substr(s);
	return ret;
}
/** @brief Extract until newline */
std::string_view extract_until_newline(std::string_view &content) {
	if (content.size() >= 2 && content[0] == '\r' && content[1] == '\n')
		return {};
	auto start_word = content.find_first_not_of(' ');
	if (start_word == std::string_view::npos) start_word = 0;
	auto end_word = content.find_first_of("\r\n", start_word);
	auto ret = content.substr(start_word, end_word - start_word);
	auto s = end_word;
	if (s == std::string_view::npos)
		content = {};
	else
		content = content.substr(s);
	return ret;
}
/** @brief Extract a newline including carriage return.
 *  @return bool with false if no newline sequence was found at the beginning of content,
 *  in which case content was not modified*/
bool extract_newline(std::string_view &content) {
	if (content.size() < 2)
		return false;
	if (content[0] != '\r' || content[1] != '\n')
		return false;
	content = content.substr(2);
	return true;
}

template template_args
constexpr static err_t tcp_server_result(void *arg, int status) {
	tcp_server template_args_pure& server = reinterpret_cast<tcp_server template_args_pure&>(*(char*)arg);
	if (status == 0) {
		LogInfo("Server success");
		return ERR_OK;
	}
	LogWarning("Server failed {}", status);
	server.complete = true;
	err_t err = ERR_OK;
	if (server.client_pcb != NULL) {
		tcp_arg(server.client_pcb, NULL);
		tcp_poll(server.client_pcb, NULL, 0);
		tcp_sent(server.client_pcb, NULL);
		tcp_recv(server.client_pcb, NULL);
		tcp_err(server.client_pcb, NULL);
		err = tcp_close(server.client_pcb);
		if (err != ERR_OK) {
			LogError("close failed calling abort: {}", err);
			tcp_abort(server.client_pcb);
			err = ERR_ABRT;
		}
		server.client_pcb = NULL;
	}
	return err;
}

template template_args
constexpr static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
	return ERR_OK;
}


template template_args
constexpr static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
	if (!p || !arg) {
		LogError("tcp_server_recv() failed");
		return tcp_server_result template_args_pure(arg, -1);
	}
	LogInfo("Message recieved");
	tcp_server template_args_pure& server = reinterpret_cast<tcp_server template_args_pure&>(*(char*)arg);
	if (p->tot_len > buf_size)
		LogError("Message too big, could not recieve");
	else if (p->tot_len > 0) {
		// Receive the buffer
		int recieve_buffer{-1};
		bool recieve_success{};
		for (auto &buffer: server.recieve_buffers) {
			++recieve_buffer;
			if (buffer.used.exchange(true))
				continue;
			buffer.buffer.set_size(pbuf_copy_partial(p, buffer.buffer.data(), p->tot_len, 0));
			server.process_request(recieve_buffer);
			recieve_success = true;
			break;
		}
		if (!recieve_success)
			LogError("Could not recieve message, no free recieve buffer");
	}
	pbuf_free(p);
	return ERR_OK;
}

template template_args
constexpr static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb) {
	LogInfo("tcp_server_poll_fn");
	return ERR_OK;
}

template template_args
constexpr static void tcp_server_err(void *arg, err_t err) {
	if (err != ERR_ABRT) {
		LogInfo("tcp_client_err_fn {}", err);
		tcp_server_result template_args_pure(arg, err);
	}
}

template template_args
constexpr static err_t tcp_server_accept (void *arg, struct tcp_pcb *client_pcb, err_t err) {
	if (err != ERR_OK || client_pcb == NULL || arg == NULL) {
		LogError("Failure in accept");
		tcp_server_result template_args_pure(arg, err);
		return ERR_VAL;
	}

	tcp_server template_args_pure& server = reinterpret_cast<tcp_server template_args_pure&>(*(char*)arg);
	
	LogInfo("Client connected, setting up callbacks");

	if (server.client_pcb) {
		err = tcp_close(server.client_pcb);
		if (err != ERR_OK) {
			LogError("close failed calling abort: {}", err);
			tcp_abort(server.client_pcb);
			err = ERR_ABRT;
		}
	}
	
	server.client_pcb = client_pcb;
	tcp_arg(client_pcb, arg);
	tcp_sent(client_pcb, tcp_server_sent template_args_pure);
	tcp_recv(client_pcb, tcp_server_recv template_args_pure);
	tcp_poll(client_pcb, tcp_server_poll template_args_pure, server.poll_time_s * 2);
	tcp_err(client_pcb, tcp_server_err template_args_pure);

	LogInfo("Client connected, setup done");
	return ERR_OK;
}


} // namespace tcp_server::internal

template template_args
void tcp_server template_args_pure::message_buffer::req_update_structured_views() {
	// request line
	std::string_view buffer_view{buffer.view};
	method = tcp_server_internal::extract_word(buffer_view);
	path = tcp_server_internal::extract_word(buffer_view);
	http_version = tcp_server_internal::extract_word(buffer_view);
	if (!tcp_server_internal::extract_newline(buffer_view))
		LogWarning("req_update_structured_views() did not find newline sequence after the request line");
	// headers
	for (std::string_view key = tcp_server_internal::extract_word(buffer_view), value = tcp_server_internal::extract_until_newline(buffer_view);
		!key.empty(); key = tcp_server_internal::extract_word(buffer_view), value = tcp_server_internal::extract_until_newline(buffer_view)) {

		key.remove_suffix(key.empty() ? 0: 1);
		if (!headers_view.headers.push(header{key, value}))
			LogWarning("req_update_structured_views() Failed to add the following header:");

		if (!tcp_server_internal::extract_newline(buffer_view))
			LogWarning("req_update_structured_views() did not find newline sequence after header");
	}
	// body (is simply the rest without the first newline)
	if (!tcp_server_internal::extract_newline(buffer_view))
		LogWarning("req_update_structured_views() did not find a newline for body info");
	body = buffer_view;
}

template template_args
void tcp_server template_args_pure::message_buffer::res_set_status_line(std::string_view http_version, std::string_view status) {
	// sanity checks
	if (!buffer.empty()) {
		buffer.clear();
		LogWarning("res_set_status_line() size != 0, is reset");
	}
	if (!headers_view.headers.empty()) {
		headers_view.headers.clear();
		LogWarning("res_set_status_line() headers_view.size != 0, is reset");
	}
	if (!body.empty()) {
		body = {};
		LogWarning("res_set_status_line() body.size() != 0, is reset");
	}
	method = {};
	path = {};

	buffer.append_formatted("{} {}\r\n", http_version, status);
	this->http_version = buffer.view;
	this->status = buffer.view;
}

template template_args
header tcp_server template_args_pure::message_buffer::res_add_header(std::string_view key, std::string_view value) {
	// sanity checks
	if (!body.empty()) {
		body = {};
		LogWarning("res_add_header() body.size() != 0, is reset");
	}

	int s = buffer.view.size();
	buffer.append_formatted("{}: {}\r\n", key, value);
	if (!this->headers_view.headers.push(header{buffer.view.substr(s), buffer.view.substr(s + key.size() + 2)})) {
		LogWarning("Reached header limit {}", max_headers);
		return {};
	}
	return *(this->headers_view.end() - 1);
}

template template_args
void tcp_server template_args_pure::message_buffer::res_write_body(std::string_view body) {
	if (this->body.empty())
		buffer.append("\r\n");
	const char *s = this->body.empty() ? buffer.view.end(): this->body.begin();

	buffer.append(body);
	this->body = std::string_view{s, buffer.view.end()};
}

template template_args
err_t tcp_server template_args_pure::start() {
	LogInfo("Starting webserver");
	struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		LogError("failed to create pcb");
		return ERR_ABRT;
	}
	
	tcp_setprio(pcb, 10);
	err_t err = tcp_bind(pcb, IP_ANY_TYPE, port);
	if (err) {
		LogError("failed to bind to port {}", port);
		return ERR_ABRT;
	}
	
	server_pcb = tcp_listen(pcb);
	if (!server_pcb) {
		LogError("failed to listen");
		if (pcb) {
			tcp_close(pcb);
		}
		return ERR_ABRT;
	}
	
	tcp_arg(server_pcb, this);
	tcp_accept(server_pcb, tcp_server_internal::tcp_server_accept template_args_pure);

	LogInfo("Webserver started");
	
	return ERR_OK;
}

template template_args
err_t tcp_server template_args_pure::stop() {
	err_t err = ERR_OK;
	if (client_pcb != NULL) {
		tcp_arg(client_pcb, NULL);
		tcp_poll(client_pcb, NULL, 0);
		tcp_sent(client_pcb, NULL);
		tcp_recv(client_pcb, NULL);
		tcp_err(client_pcb, NULL);
		err = tcp_close(client_pcb);
		if (err != ERR_OK) {
			LogError("close failed calling abort: {}", err);
			tcp_abort(client_pcb);
			err = ERR_ABRT;
		}
		client_pcb = NULL;
	}
	if (server_pcb) {
		tcp_arg(server_pcb, NULL);
		tcp_close(server_pcb);
		server_pcb = NULL;
	}
	closed = true;
	return err;
}


template template_args
void tcp_server template_args_pure::process_request(uint32_t recieve_buffer_idx) {
	if (recieve_buffer_idx >= recieve_buffers.size()) {
		LogError("Impossible recieve buffer idx");
		return;
	}
	auto &recieve_buffer = recieve_buffers[recieve_buffer_idx];

	int free_send_idx = 0;
	// the following also atomically reservers a buffer
	for (; (uint32_t)free_send_idx < send_buffers.size() && send_buffers[free_send_idx].used.exchange(true) ; ++free_send_idx);
	if ((uint32_t)free_send_idx >= send_buffers.size()) {
		LogError("No free buffer for sending found, dropping request");
		recieve_buffer.clear();
		return;
	}

	auto &send_buffer = send_buffers[free_send_idx];

	recieve_buffer.req_update_structured_views(); // parsing the recieve buffer

	LogInfo("Processing request frame and generating result {} {}", recieve_buffer.method, recieve_buffer.path);
	const auto prefixed_callback_call = [this, &recieve_buffer, &send_buffer](const auto &endpoints) {
		for (const auto &[flags, prefix, callback]: endpoints) {
			if ((flags.path_match && recieve_buffer.path == prefix.data()) ||
			    (!flags.path_match && recieve_buffer.path.starts_with(prefix.data()))) {
				callback(recieve_buffer, send_buffer);
				return;
			}
		}
		default_endpoint_cb(recieve_buffer, send_buffer);
	};
	if (recieve_buffer.method == "GET") 
		prefixed_callback_call(get_endpoints);
	else if (recieve_buffer.method == "POST") 
		prefixed_callback_call(post_endpoints);
	else if (recieve_buffer.method == "PUT")
		prefixed_callback_call(post_endpoints);
	else if (recieve_buffer.method == "DELETE")
		prefixed_callback_call(post_endpoints);
	else
		default_endpoint_cb(recieve_buffer, send_buffer);

	send_data(free_send_idx);
	recieve_buffer.clear();
	send_buffer.clear();
}

template template_args
err_t tcp_server template_args_pure::send_data(uint32_t send_buffer_index) {
	auto &buffer = send_buffers[send_buffer_index].buffer;
	err_t err = tcp_write(client_pcb, buffer.view.data(), buffer.view.size(), 0);
	if (err != ERR_OK) {
		LogError("Failed to write data {}", err);
		return tcp_server_internal::tcp_server_result template_args_pure(this, -1);
	}
	err = tcp_output(client_pcb);
	if (err != ERR_OK) {
		LogError("Failed to output data {}", err);
		return tcp_server_internal::tcp_server_result template_args_pure(this, -1);
	}
	return ERR_OK;
}

