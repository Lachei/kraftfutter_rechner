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
	template<typename N>
	constexpr void dump_to_json(static_string<N> &s) const {

	}
};

/** @brief prints formatted for monospace output */
std::ostream& operator<<(std::ostream &os, const settings &s) {
	os << "k_s:   :" << k_s << '\n';
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

