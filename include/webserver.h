#pragma once

#include <span>

#include "static_types.h"
#include "tcp_server/tcp_server.h"
#include "dcdc-converter-html.h"
#include "wifi_storage.h"
#include "access_point.h"
#include "persistent_storage.h"
#include "crypto_storage.h"
#include "ntp_client.h"
#include "settings.h"
#include "kuhspeicher.h"

using tcp_server_typed = tcp_server<16, 6, 4, 1>;

tcp_server_typed& Webserver() {
	// default endpoints from upstream
	const auto static_page_callback = [] (std::string_view page, std::string_view status, std::string_view type = "text/html") {
		return [page, status, type](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res){
			res.res_set_status_line(HTTP_VERSION, status);
			res.res_add_header("Server", DEFAULT_SERVER);
			res.res_add_header("Content-Type", type);
			res.res_add_header("Content-Length", static_format<8>("{}", page.size()));
			res.res_write_body(page);
		};
	};
	const auto fill_unauthorized = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_UNAUTHORIZED);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("WWW-Authenticate", static_format<128>(R"(Digest algorithm="{}",nonce="{:x}",realm="{}",qop="{}")", crypto_storage::algorithm, time_us_64(), crypto_storage::realm, crypto_storage::qop));
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
	};
	const auto post_login = [&fill_unauthorized] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		std::string_view auth_header = req.headers_view.get_header("Authorization");
		if (auth_header.empty() || crypto_storage::Default().check_authorization(req.method, auth_header).empty()) {
			fill_unauthorized(req, res);
			return;
		}
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
	};
	const auto get_user = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		std::string_view user{};
		std::string_view auth_header = req.headers_view.get_header("Authorization");
		if (auth_header.size()) {
			user = crypto_storage::Default().check_authorization(req.method, auth_header);
		}
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Length", static_format<8>("{}", user.size()));
		res.res_write_body(user);
	};
	const auto get_time = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		if (ntp_client::Default().ntp_time == 0) {
			res.res_set_status_line(HTTP_VERSION, STATUS_INTERNAL_SERVER_ERROR);
			res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
			res.res_add_header("Content-Length", "0");
			res.res_write_body();
			return;
		}
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		auto length_hdr = res.res_add_header("Content-Length", "    ").value;
		res.res_write_body();
		int size = res.buffer.append_formatted("{}", ntp_client::Default().get_time_since_epoch());
		if (0 == format_to_sv(length_hdr, "{}", size))
			LogError("Failed to write header length");
	};
	const auto set_time = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		ntp_client::Default().set_time_since_epoch(strtoul(req.body.data(), nullptr, 10));
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
	};
	const auto get_logs = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Type", CONTENT_TEXT);
		auto length_hdr = res.res_add_header("Content-Length", "        ").value; // at max 8 chars for size
		res.res_write_body(); // add header end sequence
		int body_size = log_storage::Default().print_errors(res.buffer);
		if (0 == format_to_sv(length_hdr, "{}", body_size))
			LogError("Failed to write header length");
	};
	const auto set_log_level = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		static constexpr std::string_view json_success{R"({"status":"success"})"};
		static constexpr std::string_view json_fail{R"({"status":"error"})"};
		LogInfo("Change log level to {}", req.body);
		// try to match version
		std::string_view status{json_success};
		if (req.body == "Info")
			log_storage::Default().cur_severity = log_severity::Info;
		else if (req.body == "Warning")
			log_storage::Default().cur_severity = log_severity::Warning;
		else if (req.body == "Error")
			log_storage::Default().cur_severity = log_severity::Error;
		else if (req.body == "Fatal")
			log_storage::Default().cur_severity = log_severity::Fatal;
		else
			status = json_fail;
		res.res_set_status_line(HTTP_VERSION, status == json_success ? STATUS_OK: STATUS_BAD_REQUEST);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Type", CONTENT_JSON);
		res.res_add_header("Content-Length", static_format<8>("{}", status.size()));
		res.res_write_body(status);
	};
	const auto get_discovered_wifis = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Type", CONTENT_JSON);
		auto length_hdr = res.res_add_header("Content-Length", "        ").value; // at max 8 chars for size
		res.res_write_body("["); // add header end sequence and json object start
		bool first_iter{true};
		for (const auto& wifi: wifi_storage::Default().wifis) {
			bool connected = wifi_storage::Default().wifi_connected && wifi_storage::Default().ssid_wifi.sv() == wifi.ssid.sv();
			res.buffer.append_formatted("{}{{\"ssid\":\"{}\",\"rssi\":{},\"connected\":{} }}\n", (first_iter? ' ': ','), 
			       wifi.ssid.sv(), wifi.rssi, connected ? "true": "false");
			first_iter = false;
		}
		res.res_write_body("]");
		if (0 == format_to_sv(length_hdr, "{}", res.body.size()))
			LogError("Failed to write header length");
	};
	const auto get_hostname = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Type", CONTENT_TEXT);
		res.res_add_header("Content-Length", static_format<8>("{}", wifi_storage::Default().hostname.size()));
		res.res_write_body(wifi_storage::Default().hostname.sv());
	};
	const auto set_hostname = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
		wifi_storage::Default().hostname.fill(req.body);
		wifi_storage::Default().hostname.make_c_str_safe();
		wifi_storage::Default().hostname_changed = true;
		if (PICO_OK != persistent_storage_t::Default().write(wifi_storage::Default().hostname, &persistent_storage_layout::hostname))
			LogError("Failed to store hostname");
	};
	const auto get_ap_active = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		std::string_view response = access_point::Default().active ? "true": "false";
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Type", CONTENT_TEXT);
		res.res_add_header("Content-Length", static_format<8>("{}", response.size()));
		res.res_write_body(response);
	};
	const auto set_ap_active = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
		if (req.body == "true")
			access_point::Default().init();
		else
			access_point::Default().deinit();
	};
	const auto connect_to_wifi = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);;
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
		// should be in format: ${ssid} ${password}
		auto t = req.body;
		std::string_view ssid = extract_word(t);
		std::string_view password = extract_word(t);
		if (ssid.empty() || password.empty()) {
			LogError("Missing ssid or password for setting wifi connection");
			return;
		}
		auto &wifi = wifi_storage::Default();
		wifi.ssid_wifi.fill(ssid);
		wifi.ssid_wifi.make_c_str_safe();
		wifi.pwd_wifi.fill(password);
		wifi.pwd_wifi.make_c_str_safe();
		wifi.wifi_connected = false;
		wifi.wifi_changed = true;
		if (PICO_OK != persistent_storage_t::Default().write(wifi.ssid_wifi, &persistent_storage_layout::ssid_wifi))
			LogError("Failed to store ssid_wifi");
		if (PICO_OK != persistent_storage_t::Default().write(wifi.pwd_wifi, &persistent_storage_layout::pwd_wifi))
			LogError("Failed to store pwd_wifi");
	};
	const auto set_password = [&fill_unauthorized] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		std::string_view auth_header = req.headers_view.get_header("Authorization");
		if (auth_header.empty() || crypto_storage::Default().check_authorization(req.method, auth_header).empty()) {
			fill_unauthorized(req, res);
			return;
		}
		crypto_storage::Default().set_password(req.body);
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
	};

	// custom enpoints for kraftfutter application
	const auto get_cow_names = [&fill_unauthorized](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		constexpr int MAX_COWS_PER_RES{(tcp_server_typed::message_buffer{}.buffer.size() - 256) / kuh{}.name.storage.size()};
		std::string_view auth_header = req.headers_view.get_header("Authorization");
		if (auth_header.empty() || crypto_storage::Default().check_authorization(req.method, auth_header).empty()) {
			fill_unauthorized(req, res);
			return;
		}

		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Type", CONTENT_JSON);
		auto length_hdr = res.res_add_header("Content-Length", "        ").value; // at max 8 chars for size
	
		// adding the cows as a list in a json: {'cows_size':100,'cow_names':['name1','name2']}
		uint32_t offset = std::clamp<uint32_t>(strtoul(req.body.data(), nullptr, 10), 0, MAX_COWS_PER_RES - 1);
		auto cows = kuhspeicher::Default().cows_view();
		res.res_write_body("{");
		res.buffer.append_formatted(R"("cows_size":{},"cow_names":[)", cows.size());
		cows = cows.subspan(offset, std::min<int>(cows.size() - offset, MAX_COWS_PER_RES));
		for (const auto &cow: cows) {
			if (&cow != cows.data())
				res.buffer.append(',');
			res.buffer.append('"');
			res.buffer.append(cow.name.sv());
			res.buffer.append('"');
		}
		res.res_write_body("]}");
		
		if (0 == format_to_sv(length_hdr, "{}", res.body.size()))
			LogError("Failed to write header length");
	};
	const auto get_cow = [&fill_unauthorized](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		std::string_view auth_header = req.headers_view.get_header("Authorization");
		if (auth_header.empty() || crypto_storage::Default().check_authorization(req.method, auth_header).empty()) {
			fill_unauthorized(req, res);
			return;
		}

		std::string_view req_cow = req.path.substr(req.path.find_last_of('/') + 1);
		const kuh *cow{};
		for (const auto& c: kuhspeicher::Default().cows_view()) {
			if (c.name.sv() == req_cow) {
				cow = &c;
				break;
			}
		}
		
		if (!cow) {
			res.res_set_status_line(HTTP_VERSION, STATUS_BAD_REQUEST);
			res.res_add_header("Server", DEFAULT_SERVER);
			res.res_add_header("Content-Length", "0");
			res.res_write_body();
			return;
		}
		
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Type", CONTENT_JSON);
		auto length_hdr = res.res_add_header("Content-Length", "        ").value; // at max 8 chars for size
		res.res_write_body();
		auto content_length = res.buffer.append_formatted(
			R"({{"name":"{}","ohrenmarke":"{}","halsbandnr":{},"kraftfuttermenge":{},"abkalbungstag":{}}})", 
			cow->name.sv(), array_to_sv(cow->ohrenmarke), cow->halsbandnr, cow->kraftfuttermenge, cow->abkalbungstag
		);
		if (0 == format_to_sv(length_hdr, "{}", content_length))
			LogError("Failed to write header length");
	};
	const auto put_cow = [&fill_unauthorized](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		std::string_view auth_header = req.headers_view.get_header("Authorization");
		if (auth_header.empty() || crypto_storage::Default().check_authorization(req.method, auth_header).empty()) {
			fill_unauthorized(req, res);
			return;
		}

		// parsing data of the type: {'name':'a_name',}
		std::string_view status{STATUS_OK};
		kuh* cow = kuhspeicher::Default().parse_cow_from_json(req.body);
		if (!cow)
			goto failure;
		if (!kuhspeicher::Default().write_or_create_cow(*cow))
			goto failure;
			
		if (false) {
		failure: status = STATUS_BAD_REQUEST;
		}

		res.res_set_status_line(HTTP_VERSION, status);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
	};
	const auto delete_cow = [&fill_unauthorized](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		std::string_view auth_header = req.headers_view.get_header("Authorization");
		if (auth_header.empty() || crypto_storage::Default().check_authorization(req.method, auth_header).empty()) {
			fill_unauthorized(req, res);
			return;
		}

		kuhspeicher::Default().delete_cow(req.body);

		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
	};
	const auto post_reboot = [&fill_unauthorized](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		std::string_view auth_header = req.headers_view.get_header("Authorization");
		if (auth_header.empty() || crypto_storage::Default().check_authorization(req.method, auth_header).empty()) {
			fill_unauthorized(req, res);
			return;
		}

		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
		wifi_storage::Default().request_reboot = true;
	};
	const auto get_settings = [&fill_unauthorized](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		std::string_view auth_header = req.headers_view.get_header("Authorization");
		if (auth_header.empty() || crypto_storage::Default().check_authorization(req.method, auth_header).empty()) {
			fill_unauthorized(req, res);
			return;
		}
		
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		auto length_hdr = res.res_add_header("Content-Length", "        ").value; // at max 8 chars for size
		res.res_write_body();
		int content_length = settings::Default().dump_to_json(res.buffer);
		if (0 == format_to_sv(length_hdr, "{}", content_length))
			LogError("Failed to write header length");
	};
	const auto set_settings = [&fill_unauthorized](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		std::string_view auth_header = req.headers_view.get_header("Authorization");
		if (auth_header.empty() || crypto_storage::Default().check_authorization(req.method, auth_header).empty()) {
			fill_unauthorized(req, res);
			return;
		}

		settings::Default().parse_from_json(req.body);
		persistent_storage_t::Default().write(settings::Default(), &persistent_storage_layout::setting);
		
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", DEFAULT_SERVER);
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
	};

	static tcp_server_typed webserver{
		.port = 80,
		.default_endpoint_cb = static_page_callback(_404_HTML, STATUS_NOT_FOUND),
		.get_endpoints = {
			// kraftfutter-specific code
			tcp_server_typed::endpoint{{.path_match = true}, "/cow_names", get_cow_names},
			tcp_server_typed::endpoint{{.path_match = false}, "/cow_entry/", get_cow},
			tcp_server_typed::endpoint{{.path_match = true}, "/setting", get_settings},
			// interactive endpoints
			tcp_server_typed::endpoint{{.path_match = true}, "/logs", get_logs},
			tcp_server_typed::endpoint{{.path_match = true}, "/discovered_wifis", get_discovered_wifis},
			tcp_server_typed::endpoint{{.path_match = true}, "/host_name", get_hostname},
			tcp_server_typed::endpoint{{.path_match = true}, "/ap_active", get_ap_active},
			// auth endpoints
			tcp_server_typed::endpoint{{.path_match = true}, "/user", get_user},
			// time endpoint
			tcp_server_typed::endpoint{{.path_match = true}, "/time", get_time},
			// static file serve endpoints
			tcp_server_typed::endpoint{{.path_match = true}, "/", static_page_callback(INDEX_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/index.html", static_page_callback(INDEX_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/style.css", static_page_callback(STYLE_CSS, STATUS_OK, "text/css")},
			tcp_server_typed::endpoint{{.path_match = true}, "/internet.html", static_page_callback(INTERNET_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/overview.html", static_page_callback(OVERVIEW_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/settings.html", static_page_callback(SETTINGS_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/cow.svg", static_page_callback(COW_SVG, STATUS_OK, "image/svg+xml")},
		},
		.post_endpoints = {
			tcp_server_typed::endpoint{{.path_match = true}, "/set_log_level", set_log_level},
			tcp_server_typed::endpoint{{.path_match = true}, "/host_name", set_hostname},
			tcp_server_typed::endpoint{{.path_match = true}, "/ap_active", set_ap_active},
			tcp_server_typed::endpoint{{.path_match = true}, "/wifi_connect", connect_to_wifi},
			tcp_server_typed::endpoint{{.path_match = true}, "/login", post_login},
			tcp_server_typed::endpoint{{.path_match = true}, "/reboot", post_reboot},
		},
		.put_endpoints = {
			tcp_server_typed::endpoint{{.path_match = true}, "/set_password", set_password},
			tcp_server_typed::endpoint{{.path_match = true}, "/time", set_time},
			tcp_server_typed::endpoint{{.path_match = true}, "/cow_entry", put_cow},
			tcp_server_typed::endpoint{{.path_match = true}, "/setting", set_settings},
		},
		.delete_endpoints = {
			tcp_server_typed::endpoint{{.path_match = true}, "/cow_entry", delete_cow},
		},
	};
	return webserver;
}

