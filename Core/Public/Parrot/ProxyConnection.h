#pragma once

#include <ranges>

namespace parrot::detail
{
template<std::ranges::view View>
struct ProxyConnection
{
	//constexpr Signal<std::ranges::range_value_t<View>> MakeSignal()
	//{
	//	return Signal<std::ranges::range_value_t<View>>{};
	//}
};
}
