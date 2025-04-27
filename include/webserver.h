#pragma once

#include "static_types.h"
#include "tcp_server/tcp_server.h"
#include "dcdc-converter-html.h"
#include "wifi_search.h"

using tcp_server_typed = tcp_server<8, 1, 0, 0>;
tcp_server_typed& Webserver() {
	const auto static_page_callback = [] (std::string_view page, std::string_view status, std::string_view type = "text/html") {
		return [page, status, type](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res){
			res.res_set_status_line(HTTP_VERSION, status);
			res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
			res.res_add_header("Content-Type", type);
			res.res_add_header("Content-Length", static_format<8>("{}", page.size()));
			res.res_write_body(page);
		};
	};
	const auto get_logs = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		res.res_add_header("Content-Type", "text/html");
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
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		res.res_add_header("Content-Type", "application/json");
		res.res_add_header("Content-Length", static_format<8>("{}", status.size()));
		res.res_write_body(status);
	};
	const auto get_discovered_wifis = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		res.res_add_header("Content-Type", "application/json");
		auto length_hdr = res.res_add_header("Content-Length", "        ").value; // at max 8 chars for size
		res.res_write_body("["); // add header end sequence and json object start
		bool first_iter{true};
		for (const auto& wifi: wifi_storage::Default().wifis) {
			res.buffer.append_formatted("{}{{\"ssid\":\"{}\",\"rssi\":{},\"connected\":false}}\n", (first_iter? ' ': ','), wifi.ssid.view, wifi.rssi);
			first_iter = false;
		}
		res.res_write_body("]");
		if (0 == format_to_sv(length_hdr, "{}", res.body.size()))
			LogError("Failed to write header length");
	};
	static tcp_server_typed webserver{
		.port = 80,
		.default_endpoint_cb = static_page_callback(_404_HTML, STATUS_NOT_FOUND),
		.get_endpoints = {
			tcp_server_typed::endpoint{{.path_match = true}, "/logs", get_logs},
			tcp_server_typed::endpoint{{.path_match = true}, "/discovered_wifis", get_discovered_wifis},
			tcp_server_typed::endpoint{{.path_match = true}, "/", static_page_callback(INDEX_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/index", static_page_callback(INDEX_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/style", static_page_callback(STYLE, STATUS_OK, "text/css")},
			tcp_server_typed::endpoint{{.path_match = true}, "/internet", static_page_callback(INTERNET, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/overview", static_page_callback(OVERVIEW, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/settings", static_page_callback(SETTINGS, STATUS_OK)},
		},
		.post_endpoints = {
			tcp_server_typed::endpoint{{.path_match = true}, "/set_log_level", set_log_level},
		}
	};
	return webserver;
}

