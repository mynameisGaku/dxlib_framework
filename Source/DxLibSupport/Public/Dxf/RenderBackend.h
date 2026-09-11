#pragma once
#include "Dxf/RenderCommands.h"
namespace Dxf
{
class IRenderBackend
{
public:
    virtual ~IRenderBackend() = default;
    /** -1 selects the back buffer. Must not clear it. */
    virtual TResult<void> SetTarget(int Handle, int Width, int Height) = 0;
    virtual TResult<void> Clear(FColor Color) = 0;
    virtual TResult<void> ResetState(int Width, int Height) = 0;
    virtual TResult<void> DrawSprite(const FSpriteCommand& Command) = 0;
    virtual TResult<void> DrawText(const FTextCommand& Command) = 0;
    virtual TResult<void> DrawRectangle(const FRectangleCommand& Command) = 0;
    virtual TResult<void> Present() = 0;
};
}
