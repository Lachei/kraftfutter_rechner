#pragma once

#include <iostream>

#include "static_types.h"

struct settings {
	float k_s{.5};

	static settings& Default() {
		static settings s{};
		return s;
	}
	/** @brief writes the settings struct as json to the static strig s */
	template<int N>
	constexpr void dump_to_json(static_string<N> &s) const {
		s.append_formatted(R"({{"k_s":{}}})", k_s);
	}
};

/** @brief prints formatted for monospace output, eg. usb */
std::ostream& operator<<(std::ostream &os, const settings &s) {
	os << "k_s:   " << s.k_s << '\n';
	return os;
}

/** @brief parses a single key, value pair from the istream */
std::istream& operator>>(std::istream &is, settings &s) {
	std::string key;
	is >> key;
	if (key == "k_s")
		is >> s.k_s;
	else
		is.fail();
	return is;
}

