#pragma once

#include "static_types.h"
#include "persistent_storage.h"
#include "ranges"
#include "ntp_client.h"
#include "settings.h"

struct kuhspeicher {
	using iota = std::ranges::iota_view<size_t, size_t>;
	static kuhspeicher& Default() {
		static kuhspeicher speicher{};
		return speicher;
	}

	kuh cow{};
	struct last_feed { uint8_t cow_idx{}, feed_idx{}; };
	static_ring_buffer<last_feed, 64, uint8_t> last_feeds{};

	int cows_size() const { return std::clamp(persistent_storage_t::Default().view(&persistent_storage_layout::cows_size), 0, MAX_COWS); }
	std::span<kuh> cows_view() const { return persistent_storage_t::Default().view(&persistent_storage_layout::cows, 0, cows_size()); }

	void clear() {
		LogInfo("Clearing cows");
		persistent_storage_t::Default().write(0, &persistent_storage_layout::cows_size);
	}

	void reload_last_feeds() {
		last_feeds.clear();
		std::span<kuh> cows = cows_view();
		const auto last_feed_time = [&](last_feed f){ return cows[f.cow_idx].letzte_fuetterungen.storage[f.feed_idx].timestamp; };
		for (int i: iota(0, cows.size())) {
			for (int j: iota(0, cows[i].letzte_fuetterungen.size())) {
				last_feed lf{uint8_t(i), uint8_t(j)};
				if (last_feeds.full && last_feed_time(last_feeds[0]) > last_feed_time(lf))
					continue;
				last_feeds.push(lf);
				// sort the stuff
				for (int i = last_feeds.size() - 2, j = last_feeds.size() - 1;
				     i >= 0; --i, --j) {
					if (last_feed_time(last_feeds[i]) < last_feed_time(last_feeds[j]))
						break;
					std::swap(last_feeds[i], last_feeds[j]);
				}
			}
		}
	}

	template<int N>
	int print_last_feeds(static_string<N> &out) {
		int write_size{2}; // 2 for opening and closing bracket
		out.append('[');
		std::span<kuh> cows{cows_view()};
		for (auto c: last_feeds) {
			const auto &cow = cows[c.cow_idx];
			if (write_size > 2) {
				out.append(',');
				++write_size;
			}
			feed_entry e = cow.letzte_fuetterungen.storage[c.feed_idx];
			write_size += out.append_formatted(R"({{"n":"{}","s":{},"t":{}}})", 
							     cow.name.sv(), int(e.station), uint32_t(e.timestamp));
		}
		out.append(']');
		return write_size;
	}

	// returns the fed kilogram of kraftfutter, returns 0 if nothing was fed, -1 if cow was not found, -2 if time issues arose
	float feed_cow(int necklace_number, int station) {
		std::span<kuh> cows{cows_view()};
		for (kuh &c: cows) {
			if (c.halsbandnr != necklace_number)
				continue;

			LogInfo("Cow {} wanting some kraftfutter, s {}", c.name.sv(), c.letzte_fuetterungen.size());

			cow = c; // copy over to ram memory
			// calculating the minutes for ration start
			time_t secs = ntp_client::Default().get_time_since_epoch();
			struct tm* time = gmtime(&secs);
			if (!time) {
				LogError("Failed to convert seconds to tm");
				return -2;
			}
			const auto &s = settings::Default();
			int reset_hour = s.reset_offsets[0];
			int reset_hour_after = s.reset_times > 1? s.reset_offsets[1]: reset_hour;
			for (int i = 1; i < s.reset_times && s.reset_offsets[i] < time->tm_hour; ++i) {
				reset_hour = s.reset_offsets[i];
				reset_hour_after = s.reset_offsets[(i + 1) % s.reset_times];
			}
			int res_del = reset_hour_after - reset_hour;
			if (res_del <= 0)
				res_del += 24;
			time->tm_hour = reset_hour;
			time->tm_min = time->tm_sec = 0;
			time_t start_time = mktime(time) / 60;
			time_t mins = secs / 60;
			int expected_feeds = int(((mins - start_time) / 60.f) / (float(res_del) / float(s.rations))) + 1;
			if (expected_feeds < 0) {
				LogError("Expected feeds is negative");
				return -2;
			}
			auto& f = cow.letzte_fuetterungen;
			int feeds{};
			for (int i = f.size() - 1; i >= 0 && f[i].timestamp >= start_time; --i)
				++feeds;
			if (feeds < expected_feeds) {
				int cow_idx = &c - cows.data();
				last_feeds.push(last_feed{uint8_t(cow_idx), uint8_t(f.cur_write)});
				f.push(feed_entry{.station = uint8_t(station), .timestamp = uint32_t(mins)});
				write_or_create_cow(cow, cow_idx);
				return cow.kraftfuttermenge / s.rations;
			} else {
				LogInfo("Hungry cow wanted more but has all its rations already {}/{} s {}", feeds, expected_feeds, f.size());
				return 0;
			}
		}
		LogError("Could not find cow with number {}", necklace_number);
		return -1;
	}

	bool write_or_create_cow(const kuh &cow, int dst = -1) {
		if (cow.name.size() > cow.name.storage.size()) {
			LogError("Invalid cow name string, not adding cow");
			return false;
		}
		auto cows_span = cows_view();
		for (int i: iota{0, cows_span.size()}) {
			if (dst != -1)
				break;
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

	void set_cow_kraftfutter(std::string_view cow_name, float kraftfutter) {
		std::span<kuh> cows{cows_view()};
		for (int i: iota{0, cows.size()}) {
			const auto &c = cows[i];
			if (c.name.sv() != cow_name)
				continue;
			cow = c;
			cow.kraftfuttermenge = kraftfutter;
			write_or_create_cow(cow, i);
			return;
		}
		LogError("Could not find the cow to set kraftfutter");
	}

	void sanitize_cows() {
		auto cows = cows_view();
		for (int i: iota{0, cows.size()})
			if (cows[i].name.size() > cows[i].name.storage.size())
				delete_cow(i, cows);
	}

	kuh* parse_cow_from_json(std::string_view json) {
		cow = {};
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

