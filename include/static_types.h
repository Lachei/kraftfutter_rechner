#pragma once

#include <array>
#include <string_view>
#include <format>

template<int N>
struct static_string {
	std::array<char, N> storage{};
	std::string_view view{};
	constexpr static_string() = default;
	constexpr static_string(std::string_view d) {
		size_t s = std::min(d.size(), storage.size());
		std::copy_n(d.begin(), s, storage.begin());
		view = std::string_view{storage.data(), s};
	}
	void set_size(int s) { view = std::string_view{storage.begin(), storage.begin() + s}; }
	void fill(std::string_view d) { 
		size_t s = std::min(d.size(), storage.size());
		std::copy_n(d.begin(), s, storage.begin());
		view = std::string_view{storage.data(), s}; 
	}
	void append(std::string_view d) { 
		size_t s = std::min<size_t>(d.size(), storage.size() - view.size());
		std::copy_n(d.begin(), s, storage.data() + view.size());
		view = std::string_view{storage.data(), view.size() + s}; 
	}
	void append(char c) {
		if (view.size() == storage.size())
			return;
		storage[view.size()] = c;
		view = std::string_view{storage.data(), view.size() + 1};
	}
	template<typename... Args>
	int fill_formatted(std::format_string<Args...> fmt, Args&&... args) { 
		auto info = std::format_to_n(storage.data(), storage.size(), fmt, std::forward<Args>(args)...); 
		view = std::string_view{storage.data(), static_cast<size_t>(info.size)};
		return info.size;
	}
	template<typename... Args>
	int append_formatted(std::format_string<Args...> fmt, Args&&... args) { 
		auto info = std::format_to_n(storage.data() + view.size(), storage.size() - view.size(), fmt, std::forward<Args>(args)...); 
		view = std::string_view{storage.data(), view.size() + info.size};
		return info.size;
	}
	char* data() { return storage.data(); }
	void clear() { view = {}; }
	bool empty() { return view.empty(); }
};

template<typename T, int N>
struct static_vector {
	std::array<T, N> storage{};
	int cur_size{};
	T* begin() { return storage.begin(); }
	T* end() { return storage.begin() + cur_size; }
	T* push() { if (cur_size >= N) return {}; return storage.data() + cur_size++; }
	bool push(const T& e) { if (cur_size == N) return false; storage[cur_size++] = e; return true; }
	bool push(T&& e) { if (cur_size == N) return false; storage[cur_size++] = std::move(e); return true; }
	void clear() { cur_size = 0; }
	bool empty() { return cur_size == 0; }
	int size() { return cur_size; }
};

template<typename T, int N>
struct static_ring_buffer {
	std::array<T, N> storage{};
	int cur_start{};
	int cur_write{};
	bool full{false};
	auto begin() { return iterator{*this, cur_start}; }
	auto end() { return iterator{*this, cur_write}; }
	auto begin() const { return iterator{*this, cur_start}; }
	auto end() const { return iterator{*this, cur_write}; }
	T* push() {T* ret = storage.data() + cur_write; 
		if (cur_start == cur_write && full) cur_start = (cur_start + 1) % N; 
		cur_write = (cur_write + 1) % N; 
		full = cur_start == cur_write; 
		return ret; }
	bool push(const T& e) { *push() = e; return true; }
	bool push(T&& e) { *push() = std::move(e); return true; }
	void clear() { cur_start = 0; cur_write = 0; full = false; }
	bool empty() { return cur_start == cur_write  && !full; }
	int size() { return full? N: cur_write - cur_start + (cur_start > cur_write ? N: 0); }
	template <typename SR>
	struct iterator {
		SR &_p;
		int _cur;
		bool _start{true};
		iterator& operator++() { _cur = (_cur + 1) % N; _start = false; return *this; }
		iterator operator++(int) const { iterator r{*this}; ++(*this); return r; }
		bool operator==(const iterator &o) const { return (!_p.full || !_start) && _cur == o._cur && &_p == &o._p; }
		bool operator!=(const iterator &o) const { return !(*this == o); }
		auto& operator*() const { return _p.storage[_cur]; }
	};
};

template<int N, typename... Args>
static std::string_view static_format(std::format_string<Args...> fmt, Args&&... args) {
	static static_string<N> string{};
	string.fill_formatted(fmt, std::forward<Args>(args)...);
	return string.view;
}

template<typename... Args>
static int format_to_sv(std::string_view dest, std::format_string<Args...> fmt, Args&&... args) {
	if (!dest.data())
		return 0;
	return std::format_to_n(const_cast<char*>(dest.data()), dest.size(), fmt, std::forward<Args>(args)...).size;
}

