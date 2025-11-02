#pragma once

#include <cstddef>
#include <cstdint>
#include <ranges>

namespace parrot
{
template<
	class Value
>
class Source
{
	struct Iterator
	{
		using difference_type = std::ptrdiff_t;
		using value_type = Value;

		constexpr explicit Iterator(const Value& initialValue) : value(initialValue) {}

		constexpr Iterator& operator=(const Iterator& other) { value = other.value; }

		constexpr const value_type& operator*() const { return value; }
		constexpr Iterator& operator++() { return *this; }
		constexpr Iterator operator++(int) { return Iterator{ value }; }

		friend constexpr bool operator==(const Iterator& a, const Iterator& b) { return a.value == b.value; }
		friend constexpr bool operator!=(const Iterator& a, const Iterator& b) { return !(a == b); }

		const Value& value;
	};

	static_assert(std::input_or_output_iterator<Iterator>);

	constexpr explicit Source() {}

	constexpr Iterator begin() const { return Iterator{ value }; }
	constexpr std::unreachable_sentinel_t end() const { return std::unreachable_sentinel; }

	constexpr void Push(Value&& newValue) const
	{
		value = newValue;
	}

	mutable Value value{};
};

static_assert(std::ranges::viewable_range<Source<int>>);
}
