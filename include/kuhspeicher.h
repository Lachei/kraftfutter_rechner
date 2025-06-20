#pragma once

#include "static_types.h"
#include "persistent_storage.h"
#include "ranges"

struct kuhspeicher {
	using iota = std::ranges::iota_view<size_t, size_t>;
	static kuhspeicher& Default() {
		static kuhspeicher speicher{};
		return speicher;
	}

	kuh cow{};
	static_ring_buffer<uint8_t, 64, uint8_t> last_feeds{};

	void clear() {
		LogInfo("Clearing cows");
		persistent_storage_t::Default().write(0, &persistent_storage_layout::cows_size);
	}

	void reload_last_feeds() {
		last_feeds.clear();
		const int N = last_feeds.storage.size();
		std::span<kuh> cows = cows_view();
		const auto last_feed_time = [&](int i){ return cows[i].letzte_fuetterungen.back().timestamp; };
		for (int i: iota(0, cows.size())) {
			if (last_feeds.full && last_feed_time(last_feeds[0]) > last_feed_time(i))
				continue;
			// sort the stuff
			for (int i = (last_feeds.cur_write + N - 2) % N,
				 j = (last_feeds.cur_write + N - 1) % N;
				 j != last_feeds.cur_start;
				 i = (i + N - 1) % N, j = (i + N - 1) % N) {
				if (last_feed_time(last_feeds[i]) < last_feed_time(last_feeds[j]))
					break;
				std::swap(last_feeds[i], last_feeds[j]);
			}
		}
	}

	int cows_size() const { return std::clamp(persistent_storage_t::Default().view(&persistent_storage_layout::cows_size), 0, MAX_COWS); }
	std::span<kuh> cows_view() const { return persistent_storage_t::Default().view(&persistent_storage_layout::cows, 0, cows_size()); }
	bool write_or_create_cow(const kuh &cow) {
		if (cow.name.size() > cow.name.storage.size()) {
			LogError("Invalid cow name string, not adding cow");
			return false;
		}
		auto cows_span = cows_view();
		int dst{-1};
		for (int i: iota{0, cows_span.size()}) {
			if (cows_span[i].name.sv() == cow.name.sv()) {
				dst = i;
				break;
			}
		}
		if (dst < 0) {
			int s = cows_span.size();
			if (s == MAX_COWS)
				return false;
			persistent_storage_t::Default().write(s + 1, &persistent_storage_layout::cows_size);
			dst = s;
		}
		persistent_storage_t::Default().write_array_range(&cow, &persistent_storage_layout::cows, dst, dst + 1);
		LogInfo("Cow {} written", cow.name.sv());
		return true;
	}
	void delete_cow(int i, std::span<kuh> cows) {
		if (size_t(i) != cows.size() - 1) {
			persistent_storage_t::Default().write_array_range(&cows.back(), &persistent_storage_layout::cows, i, i + 1);
		}
		if (cows.size())
			persistent_storage_t::Default().write(cows.size() - 1, &persistent_storage_layout::cows_size);
	}
	void delete_cow(std::string_view name) {
		auto cows = cows_view();
		int dst{-1};
		for (int i: iota{0, cows.size()}) {
			if (cows[i].name.sv() == name) {
				dst = i;
				break;
			}
		}
		if (dst < 0) {
			LogError("Failed to find cow {}", name);
			return;
		}
		delete_cow(dst, cows);
	}
	void sanitize_cows() {
		auto cows = cows_view();
		for (int i: iota{0, cows.size()})
			if (cows[i].name.size() > cows[i].name.storage.size())
				delete_cow(i, cows);
	}
	kuh* parse_cow_from_json(std::string_view json) {
		JSON_ASSERT(json.size() && json[0] == '{', "Invalid json");
		json = json.substr(1);
		for(int i = 0; i < 56 && json.size(); ++i) {
			auto key = parse_remove_json_string(json);
			JSON_ASSERT(key, "Error parsing the key");
			JSON_ASSERT(json.size() && json[0] == ':', "Invalid json, missing ':' after key");
			json = json.substr(1);
			if (key == "name") {
				auto name = parse_remove_json_string(json);
				JSON_ASSERT(name, "Error parsing the cow name");
				cow.name.fill(name.value());
			} else if (key == "ohrenmarke") {
				auto id = parse_remove_json_string(json);
				JSON_ASSERT(id, "Error parsing ohrenmarke");
				JSON_ASSERT(id.value().size() == 12, "Ohrenmarke must have 12 digits");
				id.value().copy(cow.ohrenmarke.data(), 12);
			} else if (key == "halsbandnr") {
				auto halsband = parse_remove_json_double(json);
				JSON_ASSERT(halsband, "Failed parsing halband");
				cow.halsbandnr = static_cast<int>(halsband.value());
			} else if (key == "kraftfuttermenge") {
				auto kraftfutter = parse_remove_json_double(json);
				JSON_ASSERT(kraftfutter, "Failed parsing kraftfutter menge");
				cow.kraftfuttermenge = static_cast<int>(kraftfutter.value());
			} else if (key == "abkalbungstag") {
				auto abkalbungstag = parse_remove_json_double(json);
				JSON_ASSERT(abkalbungstag, "Failed parsing abkalbungstag");
				cow.abkalbungstag = abkalbungstag.value();
			} else {
				LogError("Invalid key {}", key.value());
				return {};
			}
			JSON_ASSERT(json.size(), "Invalid json, missing character after value");
			if (json[0] == '}')
				break;
			JSON_ASSERT(json[0] == ',', "Invalid json, expected ',' after value");
			json = json.substr(1);
		}
		// checking for valid cow info
		JSON_ASSERT(cow.name.size(), "Missing cow name");
		JSON_ASSERT(cow.ohrenmarke[0] != 0, "Ohrenmarke not set");
		JSON_ASSERT(cow.halsbandnr != 0, "Halsbandnr is 0");
		JSON_ASSERT(cow.kraftfuttermenge != 0, "Kraftfuttermenge is 0");
		JSON_ASSERT(cow.abkalbungstag != 0, "Abkalbungstag is 0");
		return &cow;
	}
};

