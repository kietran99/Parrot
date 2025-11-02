#pragma once

#include <cassert>
#include <functional>
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
template<class C, class TIn>
concept SinkableOf = requires(C instance)
{
	{ instance.Receive(std::declval<TIn>()) };
};

namespace sink
{
template<class T>
struct None
{
	void Receive(T value) const {}
};

template<class T>
class Snapshot
{
public:
	Snapshot(T&& initValue)
		: value(std::forward<T>(initValue))
	{
	}

	constexpr const T& Value() const { return value; }

	void Receive(T newValue) const
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
	//{ instance.Link(std::declval<sink::None<typename C::ValueType>>()) };
	//{ instance.Unlink(std::declval<sink::None<typename C::ValueType>>()) };
};

namespace link
{
namespace edge
{
template<class T>
struct Simple
{
	using ValueType = T;
	using NextFn = std::function<bool(ValueType)>;

	constexpr bool Push(ValueType&& value) const
	{
		assert(next != nullptr);
		return std::invoke(next, value);
	}

	//constexpr NextFn Link(const SinkableOf<ValueType> auto& sink) const
	//{
	//	return [&sink](ValueType value) { sink.Receive(std::move(value)); };
	//}

	//constexpr auto Unlink(const SinkableOf<ValueType> auto&) const
	//{
	//	next = nullptr;
	//}

	NextFn next;
};
}

namespace socket
{
template<class C>
concept Emittable = requires(C instance)
{
	typename C::ValueType;
	{ instance.Push(std::declval<typename C::ValueType>()) } -> std::same_as<bool>;
};

template<class T, Linkable Link>
	requires std::same_as<typename Link::ValueType, T>
class Unicast
{
public:
	using ValueType = T;
	using LinkType = Link;

	constexpr bool Push(ValueType&& value) const
	{
		return optLink.transform([&value](const LinkType& link) { return link.Push(std::move(value)); }).value_or(false);
	}

	constexpr void Connect(LinkType&& link) const
	{
		optLink = link;
	}

	constexpr void Disconnect(const LinkType&) const
	{
		optLink = std::nullopt;
	}

private:
	mutable std::optional<LinkType> optLink;
};
}
}

template<class C>
concept EmitValidatable = requires(C instance)
{
	typename C::ValueType;
	{ instance.IsValid(std::declval<typename C::ValueType>()) } -> std::same_as<bool>;
};

namespace emit
{
namespace validator
{
template<class T>
struct Never
{
	using ValueType = T;

	constexpr bool IsValid(const ValueType&) const { return false; }
};

template<class T>
struct Always
{
	using ValueType = T;

	constexpr bool IsValid(const ValueType&) const { return true; }
};
}

//template<class T, template<class> class SocketIn, template<class> class EmitValidator>
//	requires link::socket::Emittable<SocketIn<T>>
//		and EmitValidatable<EmitValidator<T>>
//		and std::same_as<typename EmitValidator<T>::ValueType, typename SocketIn<T>::ValueType>
//struct WithLink
//{
//	using ValueType = T;
//
//	constexpr WithLink(SocketIn<T>&& socketIn, EmitValidator<T>&& emitValidator)
//		: socketIn(socketIn)
//		, emitValidator(emitValidator)
//	{
//	}
//
//	constexpr bool Push(ValueType value) const
//	{
//		return Emit(std::move(value), socketIn, emitValidator);
//	}
//
//	SocketIn<T> socketIn{};
//	EmitValidator<T> emitValidator{};
//};

template<class T, link::socket::Emittable SocketIn, EmitValidatable EmitValidator>
	requires std::same_as<typename SocketIn::ValueType, T>
		and std::same_as<typename EmitValidator::ValueType, T>
		and std::same_as<typename EmitValidator::ValueType, typename SocketIn::ValueType>
struct WithLink
{
	using ValueType = T;

	constexpr WithLink(SocketIn&& socketIn, EmitValidator&& emitValidator)
		: socketIn(socketIn)
		, emitValidator(emitValidator)
	{}

	constexpr bool Push(ValueType value) const
	{
		return emitValidator.IsValid(value) ? socketIn.Push(std::forward<T>(value)) : false;
	}

	SocketIn socketIn{};
	EmitValidator emitValidator{};
};
}

template<class C>
concept Emittable = requires(C instance)
{
	typename C::SourceType;
	{ instance.Push(std::declval<typename C::SourceType>()) };
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
struct UnicastSignal
{
	using SourceType = T;

	UnicastSignal() = default;

	template<Emittable Emitter, std::regular_invocable<typename Emitter::SourceType> Operation>
	UnicastSignal(SignalClosure<Emitter, Operation>&& signalClosure)
	{
		signalClosure.emitter.Link(signalClosure.NewSinkRefProxy<UnicastSignal<SourceType>, SourceType>(*this));
	}

	constexpr void Push(SourceType value) const
	{
		Receive(value);
	}

	constexpr void Receive(SourceType value) const
	{
		if (next)
		{
			std::invoke(next, std::forward<SourceType>(value));
		}
	}

	template<SinkableOf<SourceType> Sink>
	constexpr void Link(const Sink& sink) const
	{
		next = [&sink](SourceType value) { sink.Receive(std::move(value)); };
	}

	mutable std::function<void(SourceType)> next{ nullptr };
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
