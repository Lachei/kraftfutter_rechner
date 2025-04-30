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
	constexpr void set_size(int s) { view = std::string_view{storage.begin(), storage.begin() + s}; }
	constexpr void fill(std::string_view d) { 
		size_t s = std::min(d.size(), storage.size());
		std::copy_n(d.begin(), s, storage.begin());
		view = std::string_view{storage.data(), s}; 
	}
	constexpr void append(std::string_view d) { 
		size_t s = std::min<size_t>(d.size(), storage.size() - view.size());
		std::copy_n(d.begin(), s, storage.data() + view.size());
		view = std::string_view{storage.data(), view.size() + s}; 
	}
	constexpr void append(char c) {
		if (view.size() == storage.size())
			return;
		storage[view.size()] = c;
		view = std::string_view{storage.data(), view.size() + 1};
	}
	template<typename... Args>
	constexpr int fill_formatted(std::format_string<Args...> fmt, Args&&... args) { 
		auto info = std::format_to_n(storage.data(), storage.size(), fmt, std::forward<Args>(args)...); 
		view = std::string_view{storage.data(), static_cast<size_t>(info.size)};
		return info.size;
	}
	template<typename... Args>
	constexpr int append_formatted(std::format_string<Args...> fmt, Args&&... args) { 
		auto info = std::format_to_n(storage.data() + view.size(), storage.size() - view.size(), fmt, std::forward<Args>(args)...); 
		view = std::string_view{storage.data(), view.size() + info.size};
		return info.size;
	}
	constexpr char* data() { return storage.data(); }
	constexpr void clear() { view = {}; }
	constexpr bool empty() const { return view.empty(); }
	constexpr void make_c_str_safe() { if (view.size() < storage.size()) storage[view.size()] = '\0'; }
};

template<typename T, int N>
struct static_vector {
	std::array<T, N> storage{};
	int cur_size{};
	constexpr T* begin() { return storage.begin(); }
	constexpr T* end() { return storage.begin() + cur_size; }
	constexpr T* push() { if (cur_size >= N) return {}; return storage.data() + cur_size++; }
	constexpr bool push(const T& e) { if (cur_size == N) return false; storage[cur_size++] = e; return true; }
	constexpr bool push(T&& e) { if (cur_size == N) return false; storage[cur_size++] = std::move(e); return true; }
	template<typename F>
	constexpr void remove_if(F &&f) { for (int i = cur_size - 1; i >= 0; --i) if( f(storage[i]) ) { std::swap(storage[i], storage[cur_size - 1]); --cur_size; } }
	constexpr void clear() { cur_size = 0; }
	constexpr bool empty() const { return cur_size == 0; }
	constexpr int size() const { return cur_size; }
};

template<typename T, int N>
struct static_ring_buffer {
	std::array<T, N> storage{};
	int cur_start{};
	int cur_write{};
	bool full{false};
	constexpr auto begin() { return iterator{*this, cur_start}; }
	constexpr auto end() { return iterator{*this, cur_write}; }
	constexpr auto begin() const { return iterator{*this, cur_start}; }
	constexpr auto end() const { return iterator{*this, cur_write}; }
	constexpr T* push() {T* ret = storage.data() + cur_write; 
		if (cur_start == cur_write && full) cur_start = (cur_start + 1) % N; 
		cur_write = (cur_write + 1) % N; 
		full = cur_start == cur_write; 
		return ret; }
	constexpr bool push(const T& e) { *push() = e; return true; }
	constexpr bool push(T&& e) { *push() = std::move(e); return true; }
	constexpr void clear() { cur_start = 0; cur_write = 0; full = false; }
	constexpr bool empty() const { return cur_start == cur_write  && !full; }
	constexpr int size() const { return full? N: cur_write - cur_start + (cur_start > cur_write ? N: 0); }
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

