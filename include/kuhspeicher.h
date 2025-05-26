#pragma once

#include "static_types.h"
#include "persistent_storage.h"
#include "ranges"

using kuhnamen = static_vector<decltype(kuh::name), 256>;

// storage for in memroy storage of all cow names (used for faster readout)
struct kuhspeicher {
	static kuhspeicher& Default() {
		static kuhspeicher speicher{};
		[[maybe_unused]] static bool inited = [](){
			// loading all names from the storage
			int cows_size{};
			
			persistent_storage_t::Default().read(&persistent_storage_layout::cows_size, cows_size);
			for (int  i: std::ranges::iota_view(0, std::min<int>(cows_size, MAX_COWS))) {
				persistent_storage_t::Default().read_array_range(&persistent_storage_layout::cows, i, i + 1, &speicher.cow);
				speicher.cow_names.push(speicher.cow.name);
			}
			return true;}();
		return speicher;
	}

	kuh cow;
	kuhnamen cow_names; // quick access to all names (needed for responsive web-ui)
};

