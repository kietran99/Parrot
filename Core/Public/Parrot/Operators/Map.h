#pragma once

#include <concepts>
#include <utility>

#include "Parrot/Operators/Common.h"
#include "Parrot/Operators/Ref.h"

namespace parrot::op
{
template<Operable Op, class Func>
	requires std::regular_invocable<Func, OperableValueType<Op>>
class MapImpl
{
public:
	using ValueType = std::invoke_result_t<Func, OperableValueType<Op>>;
	using SourceType = ValueType;

	constexpr MapImpl(Op&& op, Func&& func)
		: m_op(std::forward<Op>(op))
		, m_func(std::forward<Func>(func))
	{}

	constexpr ValueType operator()(OperableValueType<Op>&& value) const
	{
		return std::invoke(m_func, value);
	}

	constexpr bool Emittable() const
	{
		return true;
	}

private:
	Op m_op;
	Func m_func;
};

template<class Func>
struct MapClosure
{
	constexpr MapClosure(Func&& func) : func(std::forward<Func>(func)) {}

	void operator()(auto&&) & = delete;
	void operator()(auto&&) const& = delete;
	void operator()(auto&&) && = delete;
	void operator()(auto&&) const&& = delete;

	template<class T>
	requires std::invocable<Func, T>
	[[nodiscard]] constexpr decltype(auto) operator()(T&& value) & noexcept(
		noexcept(func(std::forward<T>(value))))
	{
		return func(std::forward<T>(value));
	}

	template<class T>
		requires std::invocable<Func, T>
	[[nodiscard]] constexpr decltype(auto) operator()(T&& value) const& noexcept(
		noexcept(func(std::forward<T>(value))))
	{
		return func(std::forward<T>(value));
	}

	template<class T>
		requires std::invocable<Func, T>
	[[nodiscard]] constexpr decltype(auto) operator()(T&& value) && noexcept(
		noexcept(std::move(func)(std::forward<T>(value))))
	{
		return std::move(func)(std::forward<T>(value));
	}

	template<class T>
		requires std::invocable<Func, T>
	[[nodiscard]] constexpr decltype(auto) operator()(T&& value) const&& noexcept(
		noexcept(std::move(func)(std::forward<T>(value))))
	{
		return std::move(func)(std::forward<T>(value));
	}

	Func func;
};

//template<class T, class Func>
//	requires std::regular_invocable<Func, T>
//[[nodiscard]] constexpr decltype(auto) operator|(const Signal<T>& signal, MapClosure<Func>&& closure)
//{
//	return MapImpl<Ref<T>, MapClosure<Func>>{ Ref{ signal }, std::forward<MapClosure<Func>>(closure) };
//}

template<class Left, class Right>
class ClosureChain
{
public:
	constexpr ClosureChain(Left&& left, Right&& right) : left(std::forward<Left>(left)), right(std::forward<Right>(right)) {}

	void operator()(auto&&) & = delete;
	void operator()(auto&&) const& = delete;
	void operator()(auto&&) && = delete;
	void operator()(auto&&) const&& = delete;

	template <class T>
	[[nodiscard]] constexpr decltype(auto) operator()(T&& value) & noexcept(
		noexcept(right(left(std::forward<T>(value)))))
		requires requires { right(left(std::forward<T>(value))); }
	{
		return right(left(std::forward<T>(value)));
	}

	template <class T>
	[[nodiscard]] constexpr decltype(auto) operator()(T&& value) const& noexcept(
		noexcept(right(left(std::forward<T>(value)))))
		requires requires { right(left(std::forward<T>(value))); }
	{
		return right(left(std::forward<T>(value)));
	}

	template <class T>
	[[nodiscard]] constexpr decltype(auto) operator()(T&& value) && noexcept(
		noexcept(std::move(right)(std::move(left)(std::forward<T>(value)))))
		requires requires { std::move(this->right)(std::move(this->left)(std::forward<T>(value))); }
	{
		return std::move(right)(std::move(left)(std::forward<T>(value)));
	}

	template <class T>
	[[nodiscard]] constexpr decltype(auto) operator()(T&& value) const&& noexcept(
		noexcept(std::move(right)(std::move(left)(std::forward<T>(value)))))
		requires requires { std::move(this->right)(std::move(this->left)(std::forward<T>(value))); }
	{
		return std::move(right)(std::move(left)(std::forward<T>(value)));
	}

private:
	Left left;
	Right right;
};

//template<class Left, class Right>
//[[nodiscard]] constexpr decltype(auto) operator|(Left&& left, Right&& right) noexcept
//{
//	return ClosureChain(left, right);
//}

struct MapCallable
{
	template<class Func>
	requires std::constructible_from<std::decay_t<Func>, Func>
	[[nodiscard]] static constexpr auto operator()(Func&& func)
	{
		return MapClosure<Func>{ std::forward<Func>(func) };
	}
};

inline constexpr MapCallable Map;
}
