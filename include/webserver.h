#pragma once

#include "static_types.h"
#include "tcp_server/tcp_server.h"
#include "dcdc-converter-html.h"
#include "wifi_storage.h"
#include "access_point.h"
#include "persistent_storage.h"
#include "mbedtls/sha256.h"

constexpr std::string_view qop{"auth"};
constexpr std::string_view realm{"webui"};
constexpr std::string_view hex_map{"0123456789abcdef"};
constexpr int SHA_SIZE{32};

using tcp_server_typed = tcp_server<10, 4, 0, 0>;
tcp_server_typed& Webserver() {
	// Utility methods ----------------------------------------------------------
	/** @brief checks the validity of the authorization header and returns the username if successfull. If not successfull returns an empty string_view */
	const auto check_authorization_header = [] (std::string_view method, std::string_view auth_header_content) -> std::string_view {
		constexpr char colon{';'};
		std::string_view username;
		std::string_view response;
		std::string_view nonce;
		std::string_view cnonce;
		std::string_view nc; // nonce count, must not be quoted
		std::string_view uri;

		std::string_view cur = extract_word(auth_header_content);
		if (cur != "Digest") {
			LogError("check_authorization_header(): Missing Digest key word at the beginning");
			return {};
		}
		while (cur = extract_word(auth_header_content, ','), cur.size()) {
			std::string_view key = extract_word(cur, '='); // now cur holds the value
			if (cur.size() && cur.front() == '"') // unquoting
				cur.remove_prefix(1);
			if (cur.size() && cur.back() == '"')
				cur.remove_suffix(1);
			if (key == "username")
				username = cur;
			else if (key == "realm" && cur != realm) {
				LogError("check_authorization_header(): bad realm {}, should be webui", cur);
				return {};
			}
			else if (key == "qop" && cur != qop) {
				LogError("check_authorization_header(): bad qop {}, should be auth", cur);
				return {};
			}
			else if (key == "response")
				response = cur;
			else if (key == "nonce")
				nonce = cur;
			else if (key == "cnonce")
				cnonce = cur;
			else if (key == "nc")
				nc = cur;
			else if (key == "uri")
				uri = cur;
			else
				LogWarning("check_authorization_header(): unkwnown key {}", key);
		}
		if (response.size() != SHA_SIZE * 2) {
			LogError("check_authorization_header(): response sha has the wrong length {}", response.size());
			return {};
		}
		
		std::array<uint8_t, SHA_SIZE * 2> sha_storage;
		// H(A1) calculation
		mbedtls_sha256_context ctx;
		mbedtls_sha256_init(&ctx);
		mbedtls_sha256_starts_ret(&ctx, 0); // 0 = SHA-256 (not SHA-224)
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)username.data(), username.size());
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)realm.data(), realm.size());
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)wifi_storage::Default().user_pwd.data(), wifi_storage::Default().user_pwd.size());
		mbedtls_sha256_finish_ret(&ctx, sha_storage.data());
		// H(A2) calculation
		mbedtls_sha256_starts_ret(&ctx, 0); // 0 = SHA-256 (not SHA-224)
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)method.data(), method.size());
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)uri.data(), uri.size());
		mbedtls_sha256_finish_ret(&ctx, sha_storage.data() + SHA_SIZE);
		// final hash calc
		mbedtls_sha256_starts_ret(&ctx, 0); // 0 = SHA-256 (not SHA-224)
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)sha_storage.data(), SHA_SIZE);
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)nonce.data(), nonce.size());
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)nc.data(), nc.size());
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)cnonce.data(), cnonce.size());
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)qop.data(), qop.size());
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update_ret(&ctx, (const uint8_t*)sha_storage.data() + SHA_SIZE, SHA_SIZE);
		mbedtls_sha256_finish_ret(&ctx, sha_storage.data());

		mbedtls_sha256_free(&ctx);

		// compare the result
		for (const uint8_t *e = (const uint8_t*)sha_storage.data(), *r = (const uint8_t*)response.data(); e < sha_storage.data() + 1; ++e, r += 2) {
			if (hex_map[(*e) >> 4] != *r++) {
				LogError("check_authorization_header(): Hex mismatch");
				return {};
			}
			if (hex_map[(*e) & 0xf] != *r++) {
				LogError("check_authorization_header(): Hex mismatch");
				return {};
			}
		}
		return username;
	};

	// methods for returning an http response -----------------------------------

	const auto static_page_callback = [] (std::string_view page, std::string_view status, std::string_view type = "text/html") {
		return [page, status, type](const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res){
			res.res_set_status_line(HTTP_VERSION, status);
			res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
			res.res_add_header("Content-Type", type);
			res.res_add_header("Content-Length", static_format<8>("{}", page.size()));
			res.res_write_body(page);
		};
	};
	const auto return_unauthorized = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_UNAUTHORIZED);
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		res.res_add_header("WWW-Authenticate", static_format<128>(R"(Digest algorithm="SHA-256",nonce="{:x}",realm="{}",qop="{}")", time_us_64(), realm, qop));
	};
	const auto get_logs = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		res.res_add_header("Content-Type", "text/plain");
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
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		res.res_add_header("Content-Type", "text/plain");
		res.res_add_header("Content-Length", static_format<8>("{}", wifi_storage::Default().hostname.size()));
		res.res_write_body(wifi_storage::Default().hostname.sv());
	};
	const auto set_hostname = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
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
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		res.res_add_header("Content-Type", "text/plain");
		res.res_add_header("Content-Length", static_format<8>("{}", response.size()));
		res.res_write_body(response);
	};
	const auto set_ap_active = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");
		res.res_add_header("Content-Length", "0");
		res.res_write_body();
		if (req.body == "true")
			access_point::Default().init();
		else
			access_point::Default().deinit();
	};
	const auto connect_to_wifi = [] (const tcp_server_typed::message_buffer &req, tcp_server_typed::message_buffer &res) {
		res.res_set_status_line(HTTP_VERSION, STATUS_OK);
		res.res_add_header("Server", "LacheiEmbed(josefstumpfegger@outlook.de)");;
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
	static tcp_server_typed webserver{
		.port = 80,
		.default_endpoint_cb = static_page_callback(_404_HTML, STATUS_NOT_FOUND),
		.get_endpoints = {
			// interactive endpoints
			tcp_server_typed::endpoint{{.path_match = true}, "/logs", get_logs},
			tcp_server_typed::endpoint{{.path_match = true}, "/discovered_wifis", get_discovered_wifis},
			tcp_server_typed::endpoint{{.path_match = true}, "/host_name", get_hostname},
			tcp_server_typed::endpoint{{.path_match = true}, "/ap_active", get_ap_active},
			// static file serve enspoints
			tcp_server_typed::endpoint{{.path_match = true}, "/", static_page_callback(INDEX_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/index", static_page_callback(INDEX_HTML, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/style", static_page_callback(STYLE, STATUS_OK, "text/css")},
			tcp_server_typed::endpoint{{.path_match = true}, "/internet", static_page_callback(INTERNET, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/overview", static_page_callback(OVERVIEW, STATUS_OK)},
			tcp_server_typed::endpoint{{.path_match = true}, "/settings", static_page_callback(SETTINGS, STATUS_OK)},
		},
		.post_endpoints = {
			tcp_server_typed::endpoint{{.path_match = true}, "/set_log_level", set_log_level},
			tcp_server_typed::endpoint{{.path_match = true}, "/host_name", set_hostname},
			tcp_server_typed::endpoint{{.path_match = true}, "/ap_active", set_ap_active},
			tcp_server_typed::endpoint{{.path_match = true}, "/wifi_connect", connect_to_wifi},
		}
	};
	return webserver;
}

