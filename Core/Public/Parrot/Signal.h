#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
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
template<class Fn, class Ret, class... Args>
concept SignatureMatchInvocable = std::regular_invocable<Fn, Args...> and std::same_as<std::invoke_result_t<Fn, Args...>, Ret>;




template<class C>
concept Linkable = /*SignatureMatchInvocable<bool, typename C::ValueType&&> and */requires(C instance)
{
	typename C::ValueType;
	{ instance.Push(std::declval<typename C::ValueType>()) } -> std::same_as<bool>;
};

template<class C, class T>
concept LinkableOf = Linkable<C> and std::same_as<typename C::ValueType, T>;

namespace link
{
template<class T>
class Simple
{
public:
	using ValueType = T;
	using NextFn = std::function<bool(ValueType&&)>;

	constexpr Simple(SignatureMatchInvocable<bool, ValueType&&> auto&& fn)
		: m_next(std::forward<decltype(fn)>(fn))
	{}

	constexpr bool Push(ValueType&& value) const
	{
		assert(m_next != nullptr);
		return std::invoke(m_next, std::move(value));
	}

	constexpr bool operator()(ValueType&& value) const
	{
		assert(m_next != nullptr);
		return std::invoke(m_next, std::move(value));
	}

private:
	NextFn m_next;
};
}




template<class C>
concept Emittable = std::ranges::forward_range<C> and std::ranges::sized_range<C> and requires(C instance)
{
	typename C::ValueType;
	typename C::LinkType;
	requires LinkableOf<typename C::LinkType, typename C::ValueType>;
	requires LinkableOf<typename C::iterator::value_type, typename C::ValueType>;
};

template<class C, class T>
concept EmittableOf = Emittable<C> and std::same_as<typename C::ValueType, T>;




template<class C>
concept Sinkable = std::constructible_from<C, std::shared_ptr<typename C::LinkType>> and requires(C instance)
{
	typename C::ValueType;
	typename C::LinkType;
	requires LinkableOf<typename C::LinkType, typename C::ValueType>;
};

template<class C, class T>
concept SinkableOf = Sinkable<C> and std::same_as<typename C::ValueType, T>;




template<Emittable Emitter, SignatureMatchInvocable<bool, typename Emitter::ValueType&&> Operation>
[[nodiscard]] constexpr auto NewLink(const Emitter& emitter, Operation&& operation)
{
	auto link = std::make_shared<typename Emitter::LinkType>(std::forward<Operation>(operation));
	emitter.Connect(link);
	return link;
}

namespace emit
{
template<class T, template<class> class Link>
	requires Linkable<Link<T>>
class Unicast
{
public:
	using ValueType = T;
	using LinkType = Link<T>;

	class Iterator
	{
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = LinkType;

		Iterator() : m_weakLink() {}
		Iterator(std::weak_ptr<LinkType> weakLink) : m_weakLink(weakLink) {}

		const value_type& operator*() const { return *m_weakLink.lock(); }
		const value_type* operator->() const { return m_weakLink.lock().get(); }

		Iterator& operator++()
		{
			m_weakLink.reset();
			return *this;
		}

		Iterator operator++(int)
		{
			Iterator temp = *this;
			m_weakLink.reset();
			return temp;
		}

		friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_weakLink.lock() == b.m_weakLink.lock(); }
		friend bool operator!=(const Iterator& a, const Iterator& b) { return a.m_weakLink.lock() != b.m_weakLink.lock(); }

	private:
		std::weak_ptr<LinkType> m_weakLink;
	};

	static_assert(std::forward_iterator<Iterator>);

	using iterator = Iterator;

	Unicast()
		: m_optWeakLink()
	{}

	Iterator begin() const { return cbegin(); }
	Iterator end() const { return cend(); }
	Iterator cbegin() const { return Iterator{ m_optWeakLink }; }
	Iterator cend() const { return Iterator{ }; }

	constexpr size_t size() const { return m_optWeakLink.expired() ? 0 : 1; }
	constexpr bool empty() const { return m_optWeakLink.expired(); }

	constexpr void Connect(std::weak_ptr<LinkType> link) const
	{
		assert(!link.expired());
		m_optWeakLink = link;
	}

private:
	mutable std::weak_ptr<LinkType> m_optWeakLink;
};

template<class T>
using UnicastSimple = Unicast<T, link::Simple>;
}

template<class T, EmittableOf<T> Emitter>
constexpr bool operator<<(const Emitter& emitter, T&& value)
{
	return emitter.empty() ? false : std::ranges::fold_left(emitter, true, [&value](bool acc, const auto& link)
	{
		return acc && link.Push(std::remove_cvref_t<T>(value));
	});
}





namespace sink
{
template<class T, template<class> class Link>
	requires Linkable<Link<T>>
class Unicast
{
public:
	using ValueType = T;
	using LinkType = Link<T>;

	Unicast(std::shared_ptr<LinkType> link)
		: m_link(link)
	{
	}

	constexpr bool Receive(ValueType&& value) const
	{
		//return std::invoke(m_handler, std::move(value));
		return false;
	}

private:
	std::shared_ptr<LinkType> m_link;
};

template<class T>
using UnicastSimple = Unicast<T, link::Simple>;
}

namespace op
{
template<class Fn>
struct Effect
{
	constexpr explicit Effect(Fn&& fn)
		: func(std::forward<Fn>(fn))
	{}

	constexpr bool operator()(auto&& value) const
	{
		return std::invoke(func, std::forward<decltype(value)>(value));
	}

	Fn func;
};
}

template<Emittable Emitter, std::regular_invocable<typename Emitter::ValueType> Operation>
[[nodiscard]] constexpr auto operator|(const Emitter& emitter, op::Effect<Operation>&& operation)
{
	return sink::UnicastSimple<typename Emitter::ValueType>{ NewLink(emitter, std::forward<op::Effect<Operation>>(operation)) };
}









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
