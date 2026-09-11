#pragma once
#include "Dxf/Input.h"
namespace Dxf
{
class FDxLibInputSource final : public IInputSource
{
public:
    TResult<FRawInput> Poll() override;
};
}
