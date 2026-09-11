#pragma once
#include "Dxf/RenderBackend.h"
namespace Dxf
{
class FDxLibRenderBackend final : public IRenderBackend
{
public:
    TResult<void> SetTarget(int Handle, int Width, int Height) override;
    /** RGB clear only. A must be 255; render-target alpha follows DxLib's clear behavior. */
    TResult<void> Clear(FColor Color) override;
    TResult<void> ResetState(int Width, int Height) override;
    TResult<void> DrawSprite(const FSpriteCommand& Command) override;
    TResult<void> DrawText(const FTextCommand& Command) override;
    TResult<void> DrawRectangle(const FRectangleCommand& Command) override;
    TResult<void> Present() override;
private:
    TResult<void> ApplyStyle_Internal(const FDrawStyle& Style, bool bSprite);
};
}
