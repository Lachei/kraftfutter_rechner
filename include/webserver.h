#pragma once

#include "tcp_server/tcp_server.h"
#include "dcdc-converter-html.h"

using tcp_server_typed = tcp_server<6, 0, 0, 0>;
tcp_server_typed& Webserver() {
	const auto static_page_callback = [] (std::string_view page, std::string_view status) {
		return [page, status](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res){
			LogInfo("Recieved package");
			res.res_set_status_line(HTTP_VERSION, status);
			res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
			res.res_add_header("Content-Type", "text/html");
			res.res_add_header("Content-Length", std::to_string(page.size()));
			res.res_write_body(page);
		};
	};
	const auto get_logs = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		LogInfo("Recieved package");
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		res.res_add_header("Content-Type", "text/html");
		const auto log=log_storage<>::Default().print_errors();
		res.res_add_header("Content-Length", std::to_string(log.size()));
		res.res_write_body(log);
	};
	static tcp_server_typed webserver{
		.port = 80,
		.default_endpoint_cb = static_page_callback(_404_HTML, STATUS_NOT_FOUND),
		.get_endpoints = {
			tcp_server_typed::endpoint{{.path_match = true}, "/logs", get_logs},
			tcp_server_typed::endpoint{{.path_match = true}, "/", static_page_callback(INDEX_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/index", static_page_callback(INDEX_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/internet", static_page_callback(INTERNET, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/overview", static_page_callback(OVERVIEW, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/settings", static_page_callback(SETTINGS, STATUS_OK)},
		},
	};
	return webserver;
}

