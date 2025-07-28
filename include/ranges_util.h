#pragma once

#include <algorithm>

template<typename T, class P = std::identity>
struct contains { T e; P p = {}; };
template<std::ranges::input_range R, typename T, typename P>
constexpr bool operator|(R&& r, const contains<T, P>& c) { return std::ranges::contains(std::forward<R>(r), c.e, c.p); };

template<typename T, class P = std::identity>
struct find { T e; P p = {}; };
template<std::ranges::input_range R, typename T, 
	typename P = std::identity, typename Re = decltype(&*std::declval<R>().begin())>
constexpr Re operator|(R&& r, const find<T, P>& f) { 
	Re e = &*std::ranges::find(std::forward<R>(r), f.e, f.p); 
	return e != &*r.end() ? e: nullptr;
};

