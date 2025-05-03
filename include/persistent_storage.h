#pragma once

#include <mutex>

#include "pico/flash.h"
#include "hardware/flash.h"

#include "log_storage.h"

constexpr uint32_t FLASH_SIZE{2 << 20};

/** 
 * @brief  strcut to easily access/setup permanent storage with a static size and lots of compile time validations.
 * Sets up the storage at the very end of the memory range and acquires as many bytes as needed for the persistent_mem_layout struct
 * to fit
 * @usage
 * The usage is normally as follows:
 *
 * # declare the storage in a typed fashion
 * struct layout {
 *	std::array<char, 200> storage_a;
 *	int storage_b
 *	std::array<int, 400> storage_c;
 * };
 * using persistent_storage_t = persistent_storage<layout>;
 *
 * # reading and writing a value to the storage from memory
 * std::array<char, 200> mem_a;
 * persistent_storage_t::Default().write(mem_a, &layout::storage_a);
 * persistent_storage_t::Default().write(mem_a.data(), &layout::storage_a, 10, 20)
 * persistent_storage_t::Default().write(&layout::storage_a, mem_a);
 *
 * int mem_b;
 * persistent_storage_t::Default().write(mem_b, &layout::storage_b);
 * persistent_storage_t::Default().read(&layout::storage_b, mem_b);
 */

template<template persistent_mem_layout, int MAX_WRITE_SIZE = FLASH_PAGE_SIZE * 8>
struct persistent_storage {
	static constexpr char *storage_end{XIP_BASE + FLASH_SIZE}; // 2MB after flash start is end
	static constexpr char *storage_begin{(storage_end - sizeof(persistent_mem_layout)) / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE}; // round down to multiple of FLASH_PAGE_SIZE

	persistent_storage& Default() {
		static persistent_storage p{};
		return p;
	}

	std::mutex _memory_mutex{};
	std::array<char, MAX_WRITE_SIZE> _write_buffer{};

	template<auto M>
	using mem_t = decltype(std::declval<persistent_mem_layout>().*std::declval<M>());

	/** @brief To be used with member pointers: int Struct:: *member = &Struct::member_a; */
	template<typename M, typename T = mem_t<M>> requires std::islessequal(sizeof(T), MAX_WRITE_SIZE)
	err_t write(const T &data, M member) {
		uint32_t start_idx_data = storage_begin + *reinterpret_cast<uintptr_t*>(&member);
		uint32_t start_idx_paged = start_idx_data / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
		uint32_t end_idx_data = start_idx_data + sizeof(M);
		uint32_t end_idx_paged = (end_idx_data + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
		if (start_idx_data != start_idx_paged))
			std::copy(storage_begin + start_idx_paged, storage_begin + start_idx_data, _write_buffer.data());	
		if (end_idx_data != end_idx_paged))
			std::copy(storage_begin + end_idx_data, storage_begin + end_idx_paged, _write_buffer.data() + end_idx_data - start_idx_paged);	
		_write_data write_data{.src_start = &data, .src_end = &data + sizeof(data), .dst_offset = storage_begin + *reinterpret_cast<uintptr_t*>(member)};
		return flash_safe_execute(_write_impl, reinterpret_cast<void*>(&write_data), UINT32_MAX);
	}
	/** @brief Range based write overload, see write() for usage. Size has to be given in bytes written */
	template<typename M, typename T = mem_t<M>>
	err_t write_array_range(const T *data, M member, uint32_t start_idx, uint32_t end_idx) {
		// as only multiple of FLASH_PAGE_SIZE can be written, padding has to be added to the write and preloaded to contain the previous data
		std::scoped_lock lock{_memory_mutex};
	}
	template<typename M, typename T = mem_t<M>> requires std::islessequal(sizeof(T), MAX_WRITE_SIZE)
	void read(M member, mem_t<member>& out) {
	}
	template<typename M, typename T = mem_t<M>>
	void read_array_range(M member, uint32_t start_idx, uint32_t end_idx, M::value_t* out) {
		std::scoped_lock lock{_memory_mutex};
	}

	/*INTERNAL*/ struct _write_data {const char *src_start, *src_end; uint32_t dst_offset;}; // dst offset is the offset of the flash begin
	/*INTERNAL*/ void _write_impl(void *param) {
		const _write_data &data = reinterpret_cast<_write_data&>(*param);
		const uint32_t write_size = data.src_end - data.src_start;
		if (write_size % FLASH_PAGE_SIZE != 0) {
			LogError("_write_impl(): write range must be a multiple of the flash_page_size. Ignoreing write");
			return;
		}
		if (write_size + data.dst_offset > FLASH_SIZE) {
			LogError("_write_impl(): write range overflows storage. Ignoring write.");
			return;
		}
		flash_range_program(data.dst_offset, data.src_start, write_size);
	}
};

