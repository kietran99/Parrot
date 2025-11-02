#pragma once

#include <type_traits>
#include <utility>

#include "Parrot/Signal.h"

namespace parrot::op
{
template<class T>
class Ref
{
public:
	using ValueType = T;

	constexpr Ref(const Signal<T>& signal)
		: m_signalRef(signal)
	{}

private:
	const Signal<T>& m_signalRef;
};
}
