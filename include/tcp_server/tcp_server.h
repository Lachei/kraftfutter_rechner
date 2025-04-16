#pragma once

#include <array>
#include <functional>

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pico_fsdata.inc"
#include "log_storage.h"

// ------------------------------------------------------------------------------
// struct declarations
// ------------------------------------------------------------------------------

#define template_args <int get_size, int post_size, int put_size, int delete_size, int max_path_length, int max_headers, int buf_size, int message_buffers>
#define template_args_pure <get_size, post_size, put_size, delete_size, max_path_length, max_headers, buf_size, message_buffers>


template<int get_size, int post_size, int put_size, int delete_size, int max_path_length = 256, int max_headers = 64, int buf_size = 2048, int message_buffers = 4>
struct tcp_server {
	struct header {
		std::string_view key;
		std::string_view value;
	};
	struct headers {
		std::array<header, max_headers> data{};
		uint32_t size{};
		header* begin() { return data.data(); }
		header* end() { return data.data() + size; }
		std::string_view get_header(std::string_view key) {
			for (auto & [k, value]: *this)
				if (key == k)
					return value;
			return {};
		}
	};
	using endpoint_callback = std::function<std::string(std::string_view path_rest, const headers &headers, std::string_view body)>;
	struct endpoint {
		std::array<char, 256> path;
		endpoint_callback callback;
	};
	struct message_buffer{
		uint16_t size;
		std::array<uint8_t, buf_size> buffer;
	};

	struct config {
		int port{80};
		std::array<endpoint, get_size> get_endpoints{};
		std::array<endpoint, post_size> post_endpoints{};
		std::array<endpoint, put_size> put_endpoints{};
		std::array<endpoint, delete_size> delete_endpoints{};
		int poll_time_s{5};
	};

	tcp_server(config &&conf = {});
	~tcp_server() { if(!closed) LogError("Tcp server not closed before destruction!"); };
	err_t close();

	config conf{};
	
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

template template_args
constexpr static err_t tcp_server_result(void *arg, int status) {
	tcp_server template_args_pure& server = reinterpret_cast<tcp_server template_args_pure&>(*(char*)arg);
	if (status == 0) {
		LogInfo("Server success");
	} else {
		LogWarning("Server failed " + std::to_string(status));
	}
	server.complete = true;
	return server.close();
}

template template_args
constexpr static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
	tcp_server template_args_pure& server = reinterpret_cast<tcp_server template_args_pure&>(*(char*)arg);
	for (auto &buffer: server.send_buffers)
		buffer.size = 0;

	return ERR_OK;
}


template template_args
constexpr static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
	tcp_server template_args_pure& server = reinterpret_cast<tcp_server template_args_pure&>(*(char*)arg);
	if (!p) {
		return tcp_server_result template_args_pure(arg, -1);
	}
	if (p->tot_len > buf_size)
		LogError("Message too big, could not recieve");
	else if (p->tot_len > 0) {
		// Receive the buffer
		int recieve_buffer{-1};
		bool recieve_success{};
		for (auto &buffer: server.recieve_buffers) {
			++recieve_buffer;
			if (buffer.size > 0)
				continue;
			buffer.size = pbuf_copy_partial(p, buffer.buffer.data(), p->tot_len, 0);
			server.process_request(recieve_buffer);
			recieve_success = true;
			// TODO: notify rtos recieve task
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
        LogInfo("tcp_client_err_fn " + std::to_string(err));
        tcp_server_result template_args_pure(arg, err);
    }
}

template template_args
constexpr static err_t tcp_server_accept (void *arg, struct tcp_pcb *client_pcb, err_t err) {
	tcp_server template_args_pure& server = reinterpret_cast<tcp_server template_args_pure&>(*(char*)arg);
	
	if (err != ERR_OK || client_pcb == NULL) {
	    LogError("Failure in accept");
	    tcp_server_result template_args_pure(arg, err);
	    return ERR_VAL;
	}
	LogInfo("Client connected");
	
	server.client_pcb = client_pcb;
	tcp_arg(client_pcb, arg);
	tcp_sent(client_pcb, tcp_server_sent template_args_pure);
	tcp_recv(client_pcb, tcp_server_recv template_args_pure);
	tcp_poll(client_pcb, tcp_server_poll template_args_pure, server.conf.poll_time_s * 2);
	tcp_err(client_pcb, tcp_server_err template_args_pure);

	return ERR_OK;
}

} // namespace tcp_server::internal

template template_args
tcp_server template_args_pure::tcp_server(config &&conf) {
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        LogError("failed to create pcb");
        return;
    }

    err_t err = tcp_bind(pcb, NULL, conf.port);
    if (err) {
        LogError("failed to bind to port " + std::to_string(conf.port));
        return;
    }

    server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!server_pcb) {
        LogError("failed to listen");
        if (pcb) {
            tcp_close(pcb);
        }
        return;
    }

    tcp_arg(server_pcb, this);
    tcp_accept(server_pcb, tcp_server_internal::tcp_server_accept template_args_pure);

    this->conf = std::move(conf);
}

template template_args
err_t tcp_server template_args_pure::close() {
    err_t err = ERR_OK;
    if (client_pcb != NULL) {
        tcp_arg(client_pcb, NULL);
        tcp_poll(client_pcb, NULL, 0);
        tcp_sent(client_pcb, NULL);
        tcp_recv(client_pcb, NULL);
        tcp_err(client_pcb, NULL);
        err = tcp_close(client_pcb);
        if (err != ERR_OK) {
            LogError("close failed calling abort: " + std::to_string(err));
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

	// TODO insert message parsing and calling of correct endpoint here

	std::string response{};

	// final send of the data
	int free_send_idx = 0;
	for (; send_buffers[free_send_idx].size != 0 && (uint32_t)free_send_idx < send_buffers.size(); ++free_send_idx);
	if ((uint32_t)free_send_idx >= send_buffers.size())
		LogError("No free buffer for sending found, dropping response");
	else if (response.size() >= send_buffers[free_send_idx].buffer.size())
		LogError("Not enough space in response buffer for response, dropping response");
	else {
		auto &buffer = send_buffers[free_send_idx];
		std::copy(response.begin(), response.end(), buffer.buffer.data());
		buffer.buffer[response.size()] = '\0';
		buffer.size = response.size() + 1;
		send_data(free_send_idx);
	}

	// release the recieve buffer by setting its size to 0
	recieve_buffer.size = 0;
}

template template_args
err_t tcp_server template_args_pure::send_data(uint32_t send_buffer_index) {
	err_t err = tcp_write(client_pcb, send_buffers[send_buffer_index].buffer.data(), send_buffers[send_buffer_index].size, TCP_WRITE_FLAG_COPY);
	if (err != ERR_OK) {
		LogError("Failed to write data " + std::to_string(err));
		return tcp_server_internal::tcp_server_result template_args_pure(this, -1);
	}
	return ERR_OK;
}

