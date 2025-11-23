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
	auto y = arr | transformView | filterView;
	
	//auto x = std::ranges::fold_left(arr, 0, [](int count, uint64_t key) { return count + 1; });
	//auto foldpState = parrot::Foldp([](uint64_t key, uint32_t count) { return count + 1; }, 0u);
}

struct CustomType {};

TEST_CASE("Pipeable functionality")
{
	using namespace parrot;

	SUBCASE("Pipeable concept validation with mock types")
	{
		struct MockGood
		{
			using ValueType = int;
			constexpr bool operator()(ValueType&&) const { return true; }
		};

		struct MockBadNoValueType
		{
			constexpr bool operator()(int&&) const { return true; }
		};

		struct MockBadWrongSignature
		{
			using ValueType = int;
			constexpr void operator()(ValueType&&) const {}
		};

		static_assert(Pipeable<MockGood>);
		static_assert(PipeableOf<MockGood, int>);
		static_assert(!Pipeable<MockBadNoValueType>);
		static_assert(!PipeableOf<MockBadWrongSignature, int>);
	}

	SUBCASE("pipe::Simple type aliases")
	{
		static_assert(std::is_same_v<pipe::Simple<int>::ValueType, int>);
		static_assert(std::is_same_v<pipe::Simple<int>::NextFn, std::function<bool(int&&)>>);
		static_assert(Pipeable<pipe::Simple<int>>);
		static_assert(PipeableOf<pipe::Simple<int>, int>);
	}

	SUBCASE("pipe::Simple with std::function (capturing lambda)")
	{
		int calls = 0;
		std::function<bool(int&&)> fn = [&](int&& v) -> bool { ++calls; return v == 5; };
		pipe::Simple<int> p(fn);

		REQUIRE(p(5) == true);
		REQUIRE(p(1) == false);
		REQUIRE(calls == 2);
	}

	SUBCASE("pipe::Simple with stateless lambda (convertible to function-like)")
	{
		auto stateless = [](int&& v) -> bool { return (v % 2) == 0; };
		pipe::Simple<int> p(stateless);

		REQUIRE(p(4) == true);
		REQUIRE(p(3) == false);
	}

	SUBCASE("pipe::Simple with capturing lambda updates state")
	{
		int acc = 0;
		pipe::Simple<int> p([&acc](int&& v) -> bool { acc += v; return true; });

		REQUIRE(p(2) == true);
		REQUIRE(p(3) == true);
		REQUIRE(acc == 5);
	}
}

TEST_CASE("Emittable functionality")
{
	using namespace parrot;
	using Ptr = std::unique_ptr<int32_t>;

	SUBCASE("Emittable concept validation")
	{
		static_assert(Emittable<emit::UnicastSimple<int32_t>>);
		static_assert(EmittableOf<emit::UnicastSimple<int32_t>, int32_t>);
		static_assert(EmittableOf<emit::UnicastSimple<Ptr>, Ptr>);
		static_assert(!EmittableOf<emit::UnicastSimple<int32_t>, float>);
	}

	SUBCASE("emit::UnicastSimple connects and pushes successfully")
	{
		int pushCount = 0;

		// 1. Create the sink (pipe::Simple) 
		auto sink = std::make_shared<pipe::Simple<int>>([&](int&&) -> bool
		{
			pushCount++;
			return true;
		});

		// 2. Create the emitter (Unicast) and connect to the shared sink
		const emit::UnicastSimple<int> emitter{};
		emitter.Connect(sink);

		// 3. Push data
		const bool resultOne = emitter << 1;
		const bool resultTwo = emitter << 2;

		REQUIRE(resultOne == true);
		REQUIRE(resultTwo == true);
		REQUIRE(pushCount == 2);
	}
	
	SUBCASE("emit::Unicast only binds to the last pipe (Unicast behavior)")
	{
		int pushCountA = 0;
		int pushCountB = 0;

		// 1. Create Pipe A
		auto pipeA = std::make_shared<pipe::Simple<int>>([&](int&&) -> bool
		{
			pushCountA++;
			return true;
		});

		// 2. Create Pipe B
		auto pipeB = std::make_shared<pipe::Simple<int>>([&](int&&) -> bool
		{
			pushCountB++;
			return true;
		});

		const emit::UnicastSimple<int> emitter{};

		// Connect to A
		emitter.Connect(pipeA);

		// Push 1: Should go to A
		emitter << 1;
		REQUIRE(pushCountA == 1);
		REQUIRE(pushCountB == 0);

		// Connect to B (This overwrites A)
		emitter.Connect(pipeB);

		// Push 2: Should go to B, NOT A
		emitter << 2;
		REQUIRE(pushCountA == 1); // Still 1
		REQUIRE(pushCountB == 1); // Now 1

		// Push 3: Should still go to B
		emitter << 3;
		REQUIRE(pushCountA == 1); // Still 1
		REQUIRE(pushCountB == 2); // Now 2
	}

	SUBCASE("emit::UnicastSimple fails gracefully if pipe is disconnected (expired)")
	{
		int pushCount = 0;
		const emit::UnicastSimple<int> emitter{};

		{ // Scope block for shared_ptr
			// 1. Create shared sink
			auto sink = std::make_shared<pipe::Simple<int>>([&](int&&) -> bool
			{
				pushCount++;
				return true;
			});

			// 2. Connect the weak_ptr
			emitter.Connect(sink);

			// 3. Push while connected
			const bool resultConnected = emitter << 1;
			REQUIRE(resultConnected == true);
			REQUIRE(pushCount == 1);
		} // sink shared_ptr goes out of scope and is destroyed. The weak_ptr is now expired.

		// 4. Push while disconnected
		const bool resultDisconnected = emitter << 2;

		REQUIRE(resultDisconnected == false);
		REQUIRE(pushCount == 1); // No new push occurred after disconnection
	}
}

TEST_CASE("Ref operator")
{
	using namespace parrot;
	namespace op = parrot::op;

	static_assert(op::Operable<op::Ref<int>>);
	static_assert(op::Operable<op::Ref<CustomType>>);
	static_assert(std::is_same_v<op::OperableValueType<op::Ref<float>>, float>);
	static_assert(std::is_same_v<op::OperableValueType<op::Ref<CustomType>>, CustomType>);

	const parrot::Signal<float> signalFloat{};
	static_assert(std::is_same_v<op::OperableValueType<decltype(op::Ref(signalFloat))>, float>);

	const parrot::Signal<CustomType> signalCustom{};
	static_assert(std::is_same_v<op::OperableValueType<decltype(op::Ref(signalCustom))>, CustomType>);

	//Emitter<float, pipe::port::in::UnicastSimple, emit::preprocess::None> emitter{ pipe::port::in::UnicastSimple<float>{}, emit::preprocess::None<float>{} };
	//Sink<float, pipe::port::out::UnicastSimple, sink::Snapshot> sinker{ pipe::port::out::UnicastSimple<float>{}, sink::Snapshot{ 4.0f } };
	emit::UnicastSimple<float> emitter{};
	float value{ 0.0f };
	sink::UnicastSimple<float> sinker = emitter | op::Effect([&value](float&& newValue) { value = std::move(newValue); return true; });
	emitter << 34.0f;
	REQUIRE(value == 34.0f);
	emitter << 98.0f;
	REQUIRE(value == 98.0f);
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
	//	isConsonant.Pipe(parrot::sink::Ref<parrot::sink::Snapshot<bool>, bool>(snapshotSink));

	//	lastPressed.Push(12u);

	//	REQUIRE(snapshotSink.Value() == false);

	//	lastPressed.Push(111u);

	//	REQUIRE(snapshotSink.Value() == true);
	//}
}
