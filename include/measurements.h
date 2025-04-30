#pragma once

#include <iostream>

#include "static_types.h"

struct measurements {
	float i_low{};

	static measurements& Default() {
		static measurements m{};
		return m;
	}
	/** @brief writes the measurements struct as json to the static string */
	template<int N>
	constexpr void dump_to_json(static_string<N> &s) const {
		s.append_formatted(R"({{"i_low":{}}})", i_low);
	}
};

/** @brief prints formatted for monospace output, eg. usb */
std::ostream& operator<<(std::ostream &os, const measurements &m) {
	os << "i_low:    " << m.i_low << '\n';
	return os;
}

