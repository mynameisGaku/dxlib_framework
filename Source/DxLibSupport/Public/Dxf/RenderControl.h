#pragma once
#include "Dxf/Texture.h"
#include "Dxf/MathTypes.h"
#include "Dxf/Result.h"
#include <functional>
namespace Dxf
{
/** Narrow immediate-control facet. It never exposes assets, scene management or device ownership. */
class IRenderControl
{
public:
	virtual ~IRenderControl() = default;
	virtual TResult<void> SetRenderTarget(const FRenderTarget& Target) = 0;
	virtual TResult<void> SetBackBuffer() = 0;
	virtual TResult<void> ClearTarget(FColor Color) = 0;
	virtual TResult<void> Native(const std::function<TResult<void>()>& Callback) = 0;
};
}
