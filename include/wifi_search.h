#pragma once

#include <iostream>

#include "log_storage.h"
#include "static_types.h"

struct wifi_storage{
	struct wifi_info {
		static_string<256> ssid{};
		int rssi{};
	};
	static_vector<wifi_info, 8> wifis{};
	static wifi_storage& Default() {
		static wifi_storage storage{};
		return storage;
	}
	bool wifi_changed{false};
	bool wifi_connected{false};
	static_string<64> ssid_wifi{WIFI_SSID};
	static_string<64> pwd_wifi{WIFI_PASSWORD};
	bool hostname_changed{false};
	static_string<64> hostname{"DcDcConverter"};
	static_string<64> mdns_service_name{"lachei_tcp_server"};

	static int scan_result(void *, const cyw43_ev_scan_result_t *result) {
		if (!result)
			return 0;
		// check if already added
		std::string_view result_ssid{reinterpret_cast<const char *>(result->ssid)};
		for (auto &wifi: wifi_storage::Default().wifis) {
			if (wifi.ssid.view == result_ssid) {
				// LogInfo("Wifi already found");
				wifi.rssi = (.8 * wifi.rssi) + (.2 * result->rssi);
				return 0;
			}
		} auto* wifi = wifi_storage::Default().wifis.push();
		if (!wifi) {
			LogError("Wifi storage overflow");
			return 0;
		}
		
		wifi->ssid.fill(result_ssid);
		wifi->rssi = result->rssi;
		return 0;
	}
	
	void update_scanned() {
		wifis.clear();
		cyw43_wifi_scan_options_t scan_options = {0};
		int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
		if (0 != err) {
			LogError("Failed wifi scan: {}", err);
			return;
		}

		if (cyw43_wifi_scan_active(&cyw43_state)) {
			cyw43_arch_poll();
			vTaskDelay(1000);
		}
	}
};

std::ostream& operator<<(std::ostream &os, const wifi_storage &w) {
	os << "Wifi connected: " << (w.wifi_connected ? "true": "false") << '\n';
	os << "Stored wifi ssid: " << w.ssid_wifi << '\n';
	os << "ap_active: ?" << w.ssid_wifi << '\n';
	os << "hostname: " << w.hostname << '\n';
	os << "mdns_service_name: " << w.mdns_service_name << '\n';
	os << "Amount of discovered wifis: " << w.wifis.size() << '\n';
}


