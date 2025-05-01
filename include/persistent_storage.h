#pragma once

#include "pico/flash.h"
#include "hardware/flash.h"

template<template storage_struct>
struct persistent_storage {
	uint8_t *storage_end{XIP_BASE + 2 << 20}; // 2MB after flash start is end
	uint8_t *storage_begin{storage_end - sizeof(storage_struct)};

	persistent_storage& Default() {
		static persistent_storage p{};
		return p;
	}

	/** @brief To be used with member pointers: int Struct:: *member = &Struct::member_a; */
	template<typename T>
	void write(const T member);
	/** @brief Range based write overload, see write() for usage */
	template<typename T>
	void write(const T begin, const T end)
	template<typename T>
	void read(T member, decltype(*member)& out);
	template<typename T>
	void read(T begin, T end, decltype(*member)* out);
};

