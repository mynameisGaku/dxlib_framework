#pragma once
#include "Dxf/InputTypes.h"
#include "Dxf/Result.h"

namespace Dxf
{
class IInputSource
{
public:
	virtual ~IInputSource() = default;
	virtual TResult<FRawInput> Poll() = 0;
};
}
