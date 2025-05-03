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
 * persistent_storage_t::Default().write_array_range(mem_a.data(), &layout::storage_a, 10, 20)
 * persistent_storage_t::Default().read(&layout::storage_a, mem_a);
 * persistent_storage_t::Default().read_array_range(&layout::storage_a, 10, 20, mem_a);
 *
 * int mem_b;
 * persistent_storage_t::Default().write(mem_b, &layout::storage_b);
 * persistent_storage_t::Default().read(&layout::storage_b, mem_b);
 */

template<template persistent_mem_layout, int MAX_WRITE_SIZE = FLASH_PAGE_SIZE * 8>
struct persistent_storage {
	static constexpr uint32_t begin_offset{(FLASH_SIZE - sizeof(persistent_mem_layout)) / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE}; // round down to multiple of FLASH_PAGE_SIZE
	static constexpr char *flash_begin{XIP_BASE};
	static constexpr char *storage_begin{flash_begin + begin_offset};
	static constexpr char *storage_end{flash_begin + FLASH_SIZE}; // 2MB after flash start is end

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
		uint32_t start_idx_data = begin_offset + *reinterpret_cast<uintptr_t*>(&member);
		uint32_t start_idx_paged = start_idx_data / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
		uint32_t end_idx_data = start_idx_data + sizeof(M);
		uint32_t end_idx_paged = (end_idx_data + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
		if (end_idx_paged - start_idx_paged > MAX_WRITE_SIZE) {
			LogError("persistent_storage::write() too large data to write, abort.");
			return PICO_ERR;
		}
		std::memcpy(_write_buffer.data() + start_idx_data - start_idx_paged, &data, sizeof(M));
		retur _write_impl(start_idx_paged, start_idx_data, end_idx_data, end_idx_paged);
	}
	/** @brief Range based write overload, see write() for usage. Size has to be given in bytes written */
	template<typename M, typename T = mem_t<M>::value_t>
	err_t write_array_range(const T *data, M member, uint32_t start_idx, uint32_t end_idx) {
		if (start_idx == end_idx)
			return PICO_OK;
		if (end_idx > member.size() || start_idx > member.size() || start_idx > end_idx) {
			LogError("persistent_storage::write() indices out of bounds, abort.");
			return PICO_ERR;
		}
		uint32_t start_idx_data = begin_offset + *reinterpret_cast<uintptr_t*>(&member) + start_idx * sizeof(T);
		uint32_t start_idx_paged = start_idx_data / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
		uint32_t end_idx_data = start_idx_data + sizeof(T) * (end_idx - start_idx);
		uint32_t end_idx_paged = (end_idx_data + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
		if (end_idx_paged - start_idx_paged > MAX_WRITE_SIZE) {
			LogError("persistent_storage::write() too large data to write, abort.");
			return PICO_ERR;
		}
		std::memcpy(_write_buffer.data() + start_idx_data - start_idx_paged, data, end_idx_data - start_idx_data);
		retur _write_impl(start_idx_paged, start_idx_data, end_idx_data, end_idx_paged);
	}
	template<typename M, typename T = mem_t<M>> requires std::islessequal(sizeof(T), MAX_WRITE_SIZE)
	void read(M member, mem_t<member>& out) {
		// read is a simple copy from flash memory
		std::memcpy(&out, storage_begin + *reinterpret_cast<uintptr_t*>(&member), sizeof(M));
	}
	template<typename M, typename T = mem_t<M>::value_t>
	void read_array_range(M member, uint32_t start_idx, uint32_t end_idx, M::value_t* out) {
		// read is a simple copy from flash memory
		std::memcpy(out, storage_begin + *reinterpret_cast<uintptr_t*>(&member) + start_idx * sizeof(T), sizeof(T) * (end_idx - start_idx));
	}

	/*INTERNAL*/ struct _write_data {const char *src_start, *src_end; uint32_t dst_offset;}; // dst offset is the offset of the flash begin
	/*INTERNAL*/ err_t _write_impl(uint32_t start_paged, uint32_t start_data, uint32_t end_data, uint32_t end_paged) {
		std::scoped_lock lock{_memory_mutex};
		if (start_idx_data != start_idx_paged)
			std::copy(flash_begin + start_paged, flash_begin + start_data, _write_buffer.data());	
		if (end_idx_data != end_idx_paged)
			std::copy(flash_begin + end_data, flash_begin + end_paged, _write_buffer.data() + end_data - start_paged);	
		_write_data write_data{.src_start = _write_buffer.data(), 
					.src_end = _write_buffer.data() + end_paged - start_paged, 
					.dst_offset = start_paged};
		// first erase as flash_range_program only allows to change 1s to 0s, but not the other way around
		err_t err = flash_safe_execute(_flash_erase, reinterpret_cast<void*>(&write_data), UINT32_MAX);
		if (err != PICO_OK)
			return err;
		return flash_safe_execute(_write_impl, reinterpret_cast<void*>(&write_data), UINT32_MAX);
	}
	/*INTERNAL*/ static void _flash_erase(void *param) {
		const _write_data &data = reinterpret_cast<_write_data&>(*param);
		const uint32_t write_size = data.src_end - data.src_start;
		if (write_size % FLASH_PAGE_SIZE != 0) {
			LogError("_flash_erase(): write range must be a multiple of the flash_page_size. Ignoreing write");
			return;
		}
		if (write_size + data.dst_offset > FLASH_SIZE) {
			LogError("_flash_erase(): write range overflows storage. Ignoring write.");
			return;
		}
		flash_range_erase(data.dst_offset, write_size);
	}
	/*INTERNAL*/ static void _flash_program(void *param) {
		const _write_data &data = reinterpret_cast<_write_data&>(*param);
		const uint32_t write_size = data.src_end - data.src_start;
		if (write_size % FLASH_PAGE_SIZE != 0) {
			LogError("_flash_program(): write range must be a multiple of the flash_page_size. Ignoreing write");
			return;
		}
		if (write_size + data.dst_offset > FLASH_SIZE) {
			LogError("_flash_program(): write range overflows storage. Ignoring write.");
			return;
		}
		flash_range_program(data.dst_offset, data.src_start, write_size);
	}
};

