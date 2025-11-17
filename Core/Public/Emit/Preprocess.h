#pragma once

#include "Parrot/Signal.h"

namespace parrot::emit
{
template<class C, class Emitter>
concept Preprocessable = requires(C instance, const Emitter & emitter)
{
	typename C::ValueType;
	requires EmittableOf<Emitter, typename C::ValueType>;
	{ instance.PrePush(std::declval<typename C::ValueType>(), emitter) } -> std::same_as<bool>;
};

namespace preprocess
{
template<class T>
struct Never
{
	using ValueType = T;

	constexpr bool PrePush(ValueType&& value, const Emittable auto& emitter) const
	{
		return false;
	}
};

template<class T>
struct None
{
	using ValueType = T;

	constexpr bool PrePush(ValueType&& value, const Emittable auto& emitter) const
	{
		return emitter.Push(std::move(value));
	}
};
}

//template<class T, template<class> class PortIn, template<class> class Preprocessor>
//	requires Emittable<PortIn<T>>
//		and emit::Preprocessable<Preprocessor<T>, PortIn<T>>
//		and std::same_as<typename Preprocessor<T>::ValueType, typename PortIn<T>::ValueType>
//class Emitter
//{
//public:
//	using ValueType = T;
//
//	constexpr Emitter(PortIn<T>&& port, Preprocessor<T>&& preprocessor)
//		: m_port(port)
//		, m_preprocessor(preprocessor)
//	{}
//
//	constexpr bool Push(ValueType value) const
//	{
//		return m_preprocessor.PrePush(std::move(value), m_port);
//	}
//
//private:
//	PortIn<T> m_port;
//	Preprocessor<T> m_preprocessor;
//};
}
