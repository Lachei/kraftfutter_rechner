#pragma once

#include "static_types.h"
#include "persistent_storage.h"
#include "ranges"

using kuhnamen = static_vector<decltype(kuh::name), MAX_COWS>;

// storage for in memroy storage of all cow names (used for faster readout)
struct kuhspeicher {
	static kuhspeicher& Default() {
		static kuhspeicher speicher{};
		return speicher;
	}

	void clear() {
		LogInfo("Clearing cows");
		persistent_storage_t::Default().write(0, &persistent_storage_layout::cows_size);
	}

	int cows_size() const { return std::clamp(persistent_storage_t::Default().view(&persistent_storage_layout::cows_size), 0, MAX_COWS); }
	std::span<kuh> cows_view() const { return persistent_storage_t::Default().view(&persistent_storage_layout::cows, 0, cows_size()); }
};

