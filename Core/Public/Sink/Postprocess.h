#pragma once

#include "Parrot/Signal.h"

namespace parrot::sink
{
template<class C, class Sink>
concept Postprocessable = requires(C instance, const Sink & sink)
{
	typename C::ValueType;
	requires SinkableOf<Sink, typename C::ValueType>;
	{ instance.PostInvoke(std::declval<typename C::ValueType>(), sink) } -> std::same_as<bool>;
};

namespace postprocess
{
template<class T>
struct None
{
	using ValueType = T;

	void Receive(T) const {}

	constexpr bool PostInvoke(ValueType&& value, const Sinkable auto& sinker) const
	{
		return sinker.Receive(std::move(value));
	}
};

template<class T>
class Snapshot
{
public:
	using ValueType = T;

	Snapshot(T&& initValue)
		: value(std::move(initValue))
	{
	}

	constexpr const T& Value() const { return value; }

	constexpr void Receive(T newValue) const
	{
		value = std::move(newValue);
	}

	constexpr bool PostInvoke(ValueType&& value, const Sinkable auto& sinker) const
	{
		return sinker.Receive(std::move(value));
	}

private:
	mutable T value;
};
}

//template<class T, template<class> class PortOut, template<class> class Postprocessor>
//	requires Sinkable<PortOut<T>>
//		and sink::Postprocessable<Postprocessor<T>, PortOut<T>>
//		and std::same_as<typename PortOut<T>::ValueType, typename Postprocessor<T>::ValueType>
//class Sink
//{
//public:
//	using ValueType = T;
//
//	Sink(PortOut<T>&& port, Postprocessor<T>&& postprocessor)
//		: m_port(port)
//		, m_postprocessor(postprocessor)
//	{}
//
//	constexpr bool operator()(ValueType&& value) const
//	{
//		//return m_postprocessor.PostInvoke(std::move(value), m_port);
//		return false;
//	}
//
//private:
//	PortOut<T> m_port;
//	Postprocessor<T> m_postprocessor;
//};
}
