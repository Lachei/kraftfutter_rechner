#pragma once

#include <iostream>

#include "log_storage.h"
#include "static_types.h"
#include "persistent_storage.h"

struct wifi_storage{
	static constexpr uint32_t DISCOVER_TIMEOUT_US = 1e7; // 10 seconds

	struct wifi_info {
		static_string<256> ssid{};
		int rssi{};
		uint64_t last_seen_us{};
	};
	static_vector<wifi_info, 8> wifis{};
	static wifi_storage& Default() {
		static wifi_storage storage{};
		static bool inited = [&storage](){ storage.load_from_persistent_storage(); return true; }();
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
				wifi.rssi = (.8 * wifi.rssi) + (.2 * result->rssi);
				wifi.last_seen_us = time_us_64();
				return 0;
			}
		} 

		auto* wifi = wifi_storage::Default().wifis.push();
		if (!wifi) {
			LogError("Wifi storage overflow");
			return 0;
		}
		wifi->ssid.fill(result_ssid);
		wifi->rssi = result->rssi;
		wifi->last_seen_us = time_us_64();
		return 0;
	}
	
	void update_scanned() {
		// scan new wifis
		cyw43_wifi_scan_options_t scan_options = {0};
		if (0 != cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result)) {
			LogError("Failed wifi scan");
			return;
		}

		// remove old wifis
		uint64_t cur_us = time_us_64();
		wifis.remove_if([cur_us](const auto &e){ return cur_us - e.last_seen_us > DISCOVER_TIMEOUT_US; });
	}

	void write_to_persistent_storage() {
		persistent_storage_t::Default().write(hostname.storage, &persistent_storage_layout::hostname);
		persistent_storage_t::Default().write(ssid_wifi.storage, &persistent_storage_layout::ssid_wifi);
		persistent_storage_t::Default().write(pwd_wifi.storage, &persistent_storage_layout::pwd_wifi);
	}

	void load_from_persitent_storage() {
		persistent_storage_t::Default().read(&persistent_storage_layout::hostname, hostname.storage);
		persistent_storage_t::Default().read(&persistent_storage_layout::ssid_wifi, ssid_wifi.storage);
		persistent_storage_t::Default().read(&persistent_storage_layout::pwd_wifi, pwd_wifi.storage);
	}
};

std::ostream& operator<<(std::ostream &os, const wifi_storage &w) {
	os << "Wifi connected: " << (w.wifi_connected ? "true": "false") << '\n';
	os << "Stored wifi ssid: " << w.ssid_wifi.view << '\n';
	os << "hostname: " << w.hostname.view << '\n';
	os << "mdns_service_name: " << w.mdns_service_name.view << '\n';
	os << "Amount of discovered wifis: " << w.wifis.size() << '\n';
	return os;
}


