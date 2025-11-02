#pragma once

#include <ranges>

#include "Signal.h"

namespace parrot
{
template<class T, class F>
struct Foldp
{
	constexpr Foldp(F&& func, T&& initialValue) {}
};

template<class T>
struct Merge
{
	constexpr Merge(const Signal<T>& signal) {}
};

template<class T, class T2, class F>
constexpr auto operator|(const Signal<T>& signal, Foldp<T2, F>&& foldp)
{
	return Signal<T2>{ };
}

template<class T>
constexpr auto operator|(const Signal<T>& first, Merge<T>&& second)
{
	return Signal<T>{ };
}
}
