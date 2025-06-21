#pragma once

#include <iostream>

#include "static_types.h"
#include "json_helper.h"

struct settings {
	int reset_times{1}; // at max 4
	std::array<int, 3> reset_offsets{};
	int rations{4};

	static settings& Default() {
		static settings s{};
		return s;
	}
	/** @brief writes the settings struct as json to the static strig s */
	template<int N>
	constexpr int dump_to_json(static_string<N> &s) const {
		return s.append_formatted(R"({{"reset_times":{},"reset_offsets":[{},{},{}],"rations":{}}})", 
		     reset_times, reset_offsets[0], reset_offsets[1], reset_offsets[2], rations);
	}
	constexpr bool parse_from_json(std::string_view json) {
		JSON_ASSERT(json.size() && json[0] == '{', "Invalid json, missing start of object");
		json = json.substr(1);
		for(int i = 0; i < 64 && json.size(); ++i) {
			auto key = parse_remove_json_string(json);
			JSON_ASSERT(key, "Error parsing the key");
			JSON_ASSERT(json.size() && json[0] == ':', "Invalid json, missing ':' after key");
			json = json.substr(1);
			if (key == "reset_times") {
				auto rt = parse_remove_json_double(json);
				JSON_ASSERT(reset_times, "Error parsing reset_times");
				reset_times = rt.value();
			} else if (key == "reset_offsets") {
				bool parsed = parse_remove_json_double_array(json, reset_offsets);
				JSON_ASSERT(parsed, "Error parsing reset_offsets");
			} else if (key == "rations") {
				auto r = parse_remove_json_double(json);
				JSON_ASSERT(r, "Error parsing rations");
				rations = r.value();
			} else {
				LogError("Invalid key {}", key.value());
				return false;
			}
			JSON_ASSERT(json.size(), "Invalid json, missing character after value");
			if (json[0] == '}')
				break;
			JSON_ASSERT(json[0] == ',', "Invalid json, expected ',' after value");
			json = json.substr(1);
		}
		return true;
	}
};

/** @brief prints formatted for monospace output, eg. usb */
std::ostream& operator<<(std::ostream &os, const settings &s) {
	os << "reset_times " << s.reset_times << '\n';
	os << "reset_offests " << s.reset_offsets[0] << ' ' << s.reset_offsets[1] << ' ' <<  s.reset_offsets[2] << '\n';
	os << "rations " << s.rations << '\n';
	return os;
}

/** @brief parses a single key, value pair from the istream */
std::istream& operator>>(std::istream &is, settings &s) {
	std::string key;
	is >> key;
	if (key != "reset_times")
		goto fail;
	is >> s.reset_times >> key;
	if (key != "reset_offsets")
		goto fail;
	is >> s.reset_offsets[0] >> s.reset_offsets[1] >> s.reset_offsets[2] >> key;
	if (key != "rations")
		goto fail;
	is >> s.rations;
	return is;
fail:
	is.setstate(std::ios_base::failbit);
	return is;

}

