#pragma once
#include "Dxf/Contexts.h"
namespace Dxf
{
class FRenderContext;
class ILifecycleGroup
{
public:
	virtual ~ILifecycleGroup() = default;
	virtual void FreezeBoundary_Internal() = 0;
	virtual TResult<void> CommitBoundary_Internal(const FInitContext& Context) = 0;
	virtual TResult<void> Tick_Internal(const FTickContext& Context) = 0;
	virtual TResult<void> Draw_Internal(FRenderContext& Context) = 0;
	virtual void Shutdown_Internal() noexcept = 0;
};
}
