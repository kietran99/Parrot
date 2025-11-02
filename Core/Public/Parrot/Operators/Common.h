#pragma once

namespace parrot::op
{
template<class Op>
concept Operable = requires
{
	typename Op::ValueType;
};

template<Operable Op>
using OperableValueType = Op::ValueType;
}
