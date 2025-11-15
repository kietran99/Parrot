#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <concepts>
#include <functional>
#include <iostream>
#include <memory>
#include <print>
#include <ranges>
#include <string>
#include <type_traits>

#include "Parrot/Signal.h"
#include "Parrot/Operators.h"

static void Test()
{
	//const parrot::Signal<uint32_t> totalKeyCount = lastPressed | parrot::Foldp([](uint64_t key, uint32_t count) { return count + 1; }, 0u);

	//const parrot::Signal<uint64_t> lastPressed2 = parrot::Signal<uint64_t>{};
	//const auto sdf = lastPressed | parrot::Merge(lastPressed2);

	constexpr std::array arr{ 1u, 2u, 3u };
	constexpr auto transformView = std::views::transform([](uint64_t key) { return std::ranges::contains(std::array{ 'a', 'e', 'i', 'o', 'u' }, key); });
	constexpr auto filterView = std::views::filter([](uint64_t val) { return val < 10u; });
	constexpr auto takeView = std::views::take(8u);
	constexpr auto iotaView = std::views::iota(1u);

	auto x = arr | transformView;
	auto y = filterView | transformView;
	auto wer = y(arr);
	auto z = arr | y;
	
	//auto x = std::ranges::fold_left(arr, 0, [](int count, uint64_t key) { return count + 1; });
	//auto foldpState = parrot::Foldp([](uint64_t key, uint32_t count) { return count + 1; }, 0u);
}

struct CustomType {};

namespace parrot::emit::mock
{
// An emitter that satisfies EmittableOf and allows us to check its state
template<class T>
struct Emitter
{
	using ValueType = T;

	bool Push(T value) const
	{
		pushCallCount++;
		return pushReturnValue;
	}

	mutable int pushCallCount = 0;
	mutable bool pushReturnValue = true;
};

// An emitter for move-only types
template<class T>
struct MoveEmitter
{
	using ValueType = T;

	bool Push(T&& value) const
	{
		pushCallCount++;
		lastPushedValue = std::move(value);
		return true;
	}

	mutable int pushCallCount = 0;
	mutable T lastPushedValue;
};
}

TEST_CASE("Sinkable concept & built-in types")
{
	using namespace parrot;

	struct Custom {};
	
	SUBCASE("Sinkable concept is validated correctly")
	{
		struct SampleCorrectReceiveI32
		{
			void Receive(int32_t&&) const;
		};

		struct SampleCorrectReceiveCustom
		{
			bool Receive(Custom&&) const;
		};

		struct SampleMissingReceiveFunc {};

		static_assert(SinkableOf<SampleCorrectReceiveI32, int32_t>);
		static_assert(not SinkableOf<SampleCorrectReceiveI32, Custom>);
		static_assert(SinkableOf<SampleCorrectReceiveCustom, Custom>);
		static_assert(not SinkableOf<SampleMissingReceiveFunc, float>);
		
		struct SampleReceiveByValue 
		{
			void Receive(int32_t) const;
		};

		struct SampleReceiveByConstLRef 
		{
			void Receive(const int32_t&) const;
		};

		struct SampleReceiveNonConst 
		{
			void Receive(int32_t&&);
		};

		struct SamplePrivateReceive 
		{
		private:
			void Receive(int32_t&&) const;
		};

		static_assert(SinkableOf<SampleReceiveByValue, int32_t>);
		static_assert(SinkableOf<SampleReceiveByConstLRef, int32_t>);
		static_assert(SinkableOf<SampleReceiveNonConst, int32_t>);
		static_assert(not SinkableOf<SamplePrivateReceive, int32_t>);

		static_assert(SinkableOf<sink::None<float>, float>);
		static_assert(not SinkableOf<sink::None<float>, Custom>);
		static_assert(SinkableOf<sink::Snapshot<double>, double>);
		static_assert(not SinkableOf<sink::Snapshot<Custom>, double>);
	}

	SUBCASE("Snapshot captures the last received value")
	{
		SUBCASE("Snapshot with primitive value")
		{
			sink::Snapshot<float> snapshotFloat{ 3.14f };

			REQUIRE(snapshotFloat.Value() == 3.14f);

			snapshotFloat.Receive(79.8f);
			REQUIRE(snapshotFloat.Value() == 79.8f);

			snapshotFloat.Receive(-34.2f);
			REQUIRE(snapshotFloat.Value() == -34.2f);
		}

		SUBCASE("Snapshot with user-defined type")
		{
			struct IntWrapper { int32_t value; };
			sink::Snapshot<IntWrapper> snapshotIntWrapper{ { 45 } };

			REQUIRE(snapshotIntWrapper.Value().value == 45);

			snapshotIntWrapper.Receive({ -69 });
			REQUIRE(snapshotIntWrapper.Value().value == -69);
		}

		SUBCASE("Snapshot copies from lvalues")
		{
			sink::Snapshot<std::string> snapshotStr{ "initial" };
			std::string new_value = "lvalue_test";

			snapshotStr.Receive(new_value);

			REQUIRE(snapshotStr.Value() == "lvalue_test");
			REQUIRE(new_value == "lvalue_test");
		}

		SUBCASE("Snapshot moves from rvalues with move-only types")
		{
			sink::Snapshot<std::unique_ptr<int32_t>> snapshotPtr{ std::make_unique<int32_t>(100) };
			REQUIRE(*snapshotPtr.Value() == 100);

			auto new_ptr = std::make_unique<int32_t>(200);

			snapshotPtr.Receive(std::move(new_ptr));

			REQUIRE(*snapshotPtr.Value() == 200);
			REQUIRE(new_ptr == nullptr);
		}
	}
}

TEST_CASE("Emittable concept & built-in types")
{
	using namespace parrot;
	using namespace emit;

	// Local types for checking concept validation
	struct CustomType
	{
	};
	using EmitterInt = mock::Emitter<int32_t>;
	using EmitterCustom = mock::Emitter<CustomType>;

	SUBCASE("Preprocessable concept validation")
	{
		// Class 1: Correct signature (matches Never/None)
		struct SampleCorrectRRef
		{
			using ValueType = int32_t;
			bool PrePush(ValueType&&, const EmitterInt&) const
			{
				return true;
			}
		};

		// Class 2: Missing requirement (Missing ValueType)
		struct SampleMissingValueType
		{
			bool PrePush(int32_t&&, const EmitterInt&) const
			{
				return true;
			}
		};

		// Class 3: Incorrect PrePush signature (wrong return type 'void')
		struct SampleWrongReturn
		{
			using ValueType = int32_t;
			void PrePush(ValueType&&, const EmitterInt&) const
			{
			}
		};

		// Class 4: Incorrect Emitter constraint (Emitter can't handle float)
		struct SampleWrongEmitter
		{
			using ValueType = float; // EmitterInt cannot emit a float
			bool PrePush(ValueType&&, const EmitterInt&) const
			{
				return true;
			}
		};

		// --- Static Assertions ---
		static_assert(Preprocessable<SampleCorrectRRef, EmitterInt>);
		static_assert(not Preprocessable<SampleMissingValueType, EmitterInt>);
		static_assert(not Preprocessable<SampleWrongReturn, EmitterInt>);
		static_assert(not Preprocessable<SampleWrongEmitter, EmitterInt>);

		// Check built-in types
		static_assert(Preprocessable<preprocess::Never<int32_t>, EmitterInt>);
		static_assert(Preprocessable<preprocess::None<CustomType>, EmitterCustom>);
	}

	SUBCASE("Concept validation (Looseness)")
	{
		// These tests demonstrate that the concept as-written is *looser* // than the implementations. The concept allows lvalues (via const ref) 
		// and pass-by-value, while the implementations only allow rvalues (&&).
		// This is a potential design mismatch.

		struct SampleTakesConstRef
		{
			using ValueType = int32_t;
			bool PrePush(const ValueType&, const EmitterInt&) const
			{
				return true;
			}
		};
		// This passes, because std::declval<int32_t>() (an rvalue) can bind to const&
		static_assert(Preprocessable<SampleTakesConstRef, EmitterInt>);

		struct SampleTakesByValue
		{
			using ValueType = int32_t;
			bool PrePush(ValueType, const EmitterInt&) const
			{
				return true;
			}
		};
		// This passes, because std::declval<int32_t>() (an rvalue) can move-construct the value parameter
		static_assert(Preprocessable<SampleTakesByValue, EmitterInt>);
	}

	SUBCASE("preprocess::Never always returns false and does not emit")
	{
		const preprocess::Never<int32_t> preprocessor{};
		mock::Emitter<int32_t> mockEmitter{}; // Uses Emitter from emit::mock

		bool result = preprocessor.PrePush(123, mockEmitter);

		REQUIRE(result == false);
		REQUIRE(mockEmitter.pushCallCount == 0); // Verify it never called the emitter
	}

	SUBCASE("preprocess::None forwards value and returns emitter's result")
	{
		const preprocess::None<int32_t> preprocessor{};
		mock::Emitter<int32_t> mockEmitter{}; // Uses Emitter from emit::mock

		SUBCASE("Emitter returns true")
		{
			mockEmitter.pushReturnValue = true;
			bool result = preprocessor.PrePush(42, mockEmitter);

			REQUIRE(result == true); // Check forwarded return value
			REQUIRE(mockEmitter.pushCallCount == 1);
		}

		SUBCASE("Emitter returns false")
		{
			mockEmitter.pushReturnValue = false;
			bool result = preprocessor.PrePush(99, mockEmitter);

			REQUIRE(result == false); // Check forwarded return value
			REQUIRE(mockEmitter.pushCallCount == 1);
		}
	}

	SUBCASE("preprocess::None correctly moves move-only types")
	{
		using Ptr = std::unique_ptr<int32_t>;

		const preprocess::None<Ptr> preprocessor{};
		mock::MoveEmitter<Ptr> mockEmitter{}; // Uses MoveEmitter from emit::mock

		auto myPayload = std::make_unique<int32_t>(789);

		// Pass the rvalue to PrePush
		bool result = preprocessor.PrePush(std::move(myPayload), mockEmitter);

		REQUIRE(result == true);
		REQUIRE(mockEmitter.pushCallCount == 1);

		// Verify the original payload was moved from
		REQUIRE(myPayload == nullptr);

		// Verify the emitter received the correct value
		REQUIRE(mockEmitter.lastPushedValue != nullptr);
		REQUIRE(*mockEmitter.lastPushedValue == 789);
	}
	
	Emitter<float, link::port::in::UnicastSimple<float>, emit::preprocess::None<float>> emitter{ link::port::in::UnicastSimple<float>{}, emit::preprocess::None<float>{} };
	//Sink<float, sink::Snapshot, link::port::out::UnicastSimple> sink{};
	emitter.Push(67.0f);
}

TEST_CASE("Ref operator")
{
	namespace op = parrot::op;

	static_assert(op::Operable<op::Ref<int>>);
	static_assert(op::Operable<op::Ref<CustomType>>);
	static_assert(std::is_same_v<op::OperableValueType<op::Ref<float>>, float>);
	static_assert(std::is_same_v<op::OperableValueType<op::Ref<CustomType>>, CustomType>);

	const parrot::Signal<float> signalFloat{};
	static_assert(std::is_same_v<op::OperableValueType<decltype(op::Ref(signalFloat))>, float>);

	const parrot::Signal<CustomType> signalCustom{};
	static_assert(std::is_same_v<op::OperableValueType<decltype(op::Ref(signalCustom))>, CustomType>);

	REQUIRE(true);
}

TEST_CASE("Map operator")
{
	namespace op = parrot::op;

	static_assert(op::Operable<op::MapImpl<op::Ref<int>, decltype([](int) { return true; })>>);
	static_assert(op::Operable<op::MapImpl<op::Ref<CustomType>, decltype([](CustomType) { return true; })>>);
	static_assert(op::Operable<op::MapImpl<op::Ref<CustomType>, decltype([](const CustomType&) { return 23; })>>);
	static_assert(op::Operable<op::MapImpl<op::Ref<CustomType>, decltype([](CustomType&&) { return 1.f; })>>);
	static_assert(std::is_same_v<op::OperableValueType<op::MapImpl<op::Ref<int>, decltype([](int) { return true; })>>, bool>);
	static_assert(std::is_same_v<op::OperableValueType<op::MapImpl<op::Ref<int>, decltype([](int) { return 4.f; })>>, float>);
	static_assert(std::is_same_v<op::OperableValueType<op::MapImpl<op::Ref<CustomType>, decltype([](CustomType) { return true; })>>, bool>);
	static_assert(std::is_same_v<op::OperableValueType<op::MapImpl<op::Ref<CustomType>, decltype([](CustomType) { return 1u; })>>, uint32_t>);
	static_assert(std::is_same_v<op::OperableValueType<op::MapImpl<op::Ref<CustomType>, decltype([](const CustomType&) { return 1u; })>>, uint32_t>);
	static_assert(std::is_same_v<op::OperableValueType<op::MapImpl<op::Ref<CustomType>, decltype([](CustomType&&) { return 1u; })>>, uint32_t>);

	//const parrot::Signal<uint32_t> signalU32{};
	//static_assert(std::is_same_v<op::OperableValueType<decltype(signalU32 | op::Map([](uint32_t) { return true; }))>, bool>);

	//const parrot::Signal<CustomType> signalCustom{};
	//static_assert(std::is_same_v < op::OperableValueType<decltype(signalCustom | op::Map([](CustomType) { return 1.0f; }))>, float>);

	//static_assert(parrot::Emittable<parrot::UnicastSignal<uint64_t>>);

	//{
	//	parrot::UnicastSignal<uint64_t> lastPressed{};
	//	parrot::UnicastSignal<bool> isConsonant = lastPressed
	//		| parrot::op::Map([](uint64_t key) { return std::ranges::contains(std::array{ 'a', 'e', 'i', 'o', 'u' }, key); })
	//		;
	//	parrot::sink::Snapshot<bool> snapshotSink{ true };
	//	isConsonant.Link(parrot::sink::Ref<parrot::sink::Snapshot<bool>, bool>(snapshotSink));

	//	lastPressed.Push(12u);

	//	REQUIRE(snapshotSink.Value() == false);

	//	lastPressed.Push(111u);

	//	REQUIRE(snapshotSink.Value() == true);
	//}
}
