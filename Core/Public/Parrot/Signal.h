#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

/*
* Inputs:
* Mouse.position = Signal (Int, Int) // Continuous: value always present
* Keyboard.lastPressed = Signal Int // Discrete: value only present at certain timestamps
* 
* Transformations:
* Keyboard.lastPressed | transform(IsConsonant) => ('h' 'e' 'l' 'l' 'o' -> T F T T F)
* 
* State:
* Keyboard.lastPressed | foldp([](key, count) { return count + 1; }, 0u) => ('h' 'e' 'l' 'l' 'o' -> 1 2 3 4 5)
* 
* Merge:
* Signal a -> Signal a -> Signal a
*
* Side-effects:
* auto subscription = Mouse.Clicked()
	| transform(_.clientX)
	| fold_left([](u32 count, u32 clientX) => count + clientX, 0u);
	| myLabel.text;
*/

/*
	class Iterator
	{
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = NotifyEvent;

		Iterator() : m_notifyEvent(nullptr) {}
		Iterator(std::span<char> buffer) : m_notifyEvent(buffer.data()) {}

		const value_type& operator*() const { return m_notifyEvent; }
		const value_type* operator->() const { return &m_notifyEvent; }
		Iterator& operator++();
		Iterator operator++(int);

		friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_notifyEvent.m_data == b.m_notifyEvent.m_data; }
		friend bool operator!=(const Iterator& a, const Iterator& b) { return a.m_notifyEvent.m_data != b.m_notifyEvent.m_data; }

	private:
		NotifyEvent m_notifyEvent;
	};

	static_assert(std::forward_iterator<Iterator>);

	NotifyEventSpan(std::span<char> buffer) : m_buffer(buffer) {}

	Iterator begin() const { return cbegin(); }
	Iterator end() const { return cend(); }
	Iterator cbegin() const { return Iterator{ m_buffer }; }
	Iterator cend() const { return Iterator{ }; }

private:
	std::span<char> m_buffer;
*/

namespace parrot
{
template<class T, std::regular_invocable<T> Fn>
	requires not std::same_as<std::invoke_result_t<Fn, T>, void>
auto MapOrDefault(std::weak_ptr<T> weakPtr, Fn&& fn, std::invoke_result_t<Fn, T> defaultValue) -> std::invoke_result_t<Fn, T>
{
	if (auto strongPtr = weakPtr.lock())
	{
		return std::invoke(std::forward<Fn>(fn), *strongPtr);
	}

	return defaultValue;
}

template<class C>
concept Emittable = requires(C instance)
{
	typename C::ValueType;
	{ instance.Push(std::declval<typename C::ValueType>()) } -> std::same_as<bool>;
};

template<class C, class T>
concept EmittableOf = Emittable<C> and std::same_as<typename C::ValueType, T>;

namespace emit
{
template<class C, class Emitter>
concept Preprocessable = requires(C instance, const Emitter& emitter)
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
}




template<class C>
concept Sinkable = requires(C instance)
{
	typename C::ValueType;
	{ instance.Receive(std::declval<typename C::ValueType>()) };
};

template<class C, class TIn>
concept SinkableOf = requires(C instance)
{
	{ instance.Receive(std::declval<TIn>()) };
};

namespace sink
{
template<class C, class Sink>
concept Postprocessable = requires(C instance, const Sink& sink)
{
	typename C::ValueType;
	{ instance.PostInvoke(std::declval<typename C::ValueType>(), sink) } -> std::same_as<bool>;
};

template<class T>
struct None
{
	using ValueType = T;

	void Receive(T) const {}
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

private:
	mutable T value;
};
}




template<class C>
concept Linkable = requires(C instance)
{
	typename C::ValueType;
	{ instance.Push(std::declval<typename C::ValueType>()) } -> std::same_as<bool>;
};

template<class C, class T>
concept LinkableOf = Linkable<C> and std::same_as<typename C::ValueType, T>;

namespace link
{
namespace edge
{
template<class T>
class Simple
{
public:
	using ValueType = T;
	using NextFn = std::function<bool(ValueType&&)>;

	constexpr Simple(const SinkableOf<ValueType> auto& sink)
		: next(std::bind(&decltype(sink)::Receive, sink, std::placeholders::_1))
		//: next([sink](ValueType&& value) { sink.Receive(std::move(value)); return true; })
	{}

	constexpr bool Push(ValueType&& value) const
	{
		assert(next != nullptr);
		return std::invoke(next, std::move(value));
	}

private:
	NextFn next;
};
}

namespace port
{
namespace in
{
template<class T, LinkableOf<T> Link>
class Unicast
{
public:
	using ValueType = T;
	using LinkType = Link;

	constexpr bool Push(ValueType&& value) const
	{
		return MapOrDefault(m_optLink, [&value](const LinkType& link) { return link.Push(std::move(value)); }, false);
	}

	constexpr void Connect(std::weak_ptr<LinkType> link) const
	{
		assert(!link.expired());
		m_optLink = link;
	}

private:
	mutable std::weak_ptr<LinkType> m_optLink;
};

template<class T>
using UnicastSimple = Unicast<T, edge::Simple<T>>;
}

namespace out
{
template<class T, LinkableOf<T> Link>
class Unicast
{
public:
	using ValueType = T;
	using LinkType = Link;

	constexpr bool Receive(ValueType&& value) const
	{
		return std::invoke(m_handler, std::move(value));
	}

	constexpr void Connect(std::shared_ptr<LinkType> link) const
	{
		assert(link != nullptr);
		m_link = link;
	}

private:
	mutable std::shared_ptr<Link> m_link;
	std::function<bool(ValueType&&)> m_handler;
};

template<class T>
using UnicastSimple = Unicast<T, edge::Simple<T>>;
}
}
}




//template<class T, template<class> class PortIn, template<class> class Preprocessor>
//	requires Emittable<PortIn<T>>
//		and emit::Preprocessable<Preprocessor<T>>
//		and std::same_as<typename Preprocessor<T>::ValueType, typename PortIn<T>::ValueType>
//struct Emitter
//{
//	using ValueType = T;
//
//	constexpr Emitter(PortIn<T>&& port, Preprocessor<T>&& preprocessor)
//		: m_port(port)
//		, m_preprocessor(preprocessor)
//	{
//	}
//
//	constexpr bool Push(ValueType value) const
//	{
//		return Emit(std::move(value), m_port, m_preprocessor);
//	}
//
//	PortIn<T> m_port;
//	Preprocessor<T> m_preprocessor;
//};

template<class T, Emittable PortIn, emit::Preprocessable<PortIn> Preprocessor>
	requires std::same_as<typename PortIn::ValueType, T>
		and std::same_as<typename Preprocessor::ValueType, T>
		and std::same_as<typename Preprocessor::ValueType, typename PortIn::ValueType>
class Emitter
{
public:
	using ValueType = T;

	constexpr Emitter(PortIn&& port, Preprocessor&& preprocessor)
		: m_port(port)
		, m_preprocessor(preprocessor)
	{
	}

	constexpr bool Push(ValueType value) const
	{
		return m_preprocessor.PrePush(std::move(value), m_port);
	}

private:
	PortIn m_port;
	Preprocessor m_preprocessor;
};




template<class T, template<class> class Postprocessor, template<class> class PortOut>
	requires sink::Postprocessable<Postprocessor<T>, PortOut>
		and Sinkable<PortOut<T>>
		and std::same_as<typename PortOut<T>::ValueType, typename Postprocessor<T>::ValueType>
class Sink
{
public:
	using ValueType = T;

	Sink(PortOut<T>&& port, Postprocessor<T>&& postprocessor)
		: m_port(port)
		, m_postprocessor(postprocessor)
	{}

	constexpr bool operator()(ValueType&& value) const
	{
		//return m_postprocessor.PostInvoke(std::move(value), m_port);
	}

private:
	PortOut<T> m_port;
	Postprocessor<T> m_postprocessor;
};



template<Emittable Emitter, std::regular_invocable<typename Emitter::SourceType> Operation>
struct SignalClosure
{
	using EmitType = typename Emitter::SourceType;

	SignalClosure(const Emitter& emitter, Operation&& operation)
		: emitter(emitter)
		, operation(std::forward<Operation>(operation))
	{
	}

	template<class Sink, class TIn>
		requires SinkableOf<Sink, TIn>
	struct SinkRefProxy
	{
		SinkRefProxy(const Sink& sink, Operation&& operation)
			: sink(sink)
			, operation(std::forward<Operation>(operation))
		{
		}

		constexpr void Receive(EmitType value) const
		{
			sink.Receive(std::invoke(operation, std::move(value)));
		}

		const Sink& sink;
		Operation operation;
	};

	template<class Sink, class TIn>
		requires SinkableOf<Sink, TIn>
	constexpr auto NewSinkRefProxy(const Sink& sink)
	{
		return SinkRefProxy<Sink, TIn>{ sink, std::move(operation) };
	}

	const Emitter& emitter;
	Operation operation;
};

template<class T>
struct Signal
{
	using SourceType = T;
};

template<Emittable Emitter, std::regular_invocable<typename Emitter::SourceType> Operation>
[[nodiscard]] constexpr auto operator|(const Emitter& emitter, Operation&& operation)
{
	return SignalClosure<Emitter, Operation>{ emitter, std::forward<Operation>(operation) };
}
}
