#include "Dxf/DxLibRenderBackend.h"
#include "NativeApi.h"
#include <array>
#include <cmath>
#include <limits>
namespace Dxf
{
TResult<void> FDxLibRenderBackend::SetTarget(int Handle, int Width, int Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid draw target dimensions");
    }
    auto Target = Detail::CheckNative_Internal(DxLib::SetDrawScreen(Handle < 0 ? DX_SCREEN_BACK : Handle), "SetDrawScreen failed");
    if (!Target) { return Target; }
    return Detail::CheckNative_Internal(DxLib::SetDrawArea(0, 0, Width, Height), "SetDrawArea failed");
}
TResult<void> FDxLibRenderBackend::ResetState(int Width, int Height)
{
    if (DxLib::SetDrawArea(0, 0, Width, Height) < 0 ||
        DxLib::SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255) < 0 ||
        DxLib::SetDrawBright(255, 255, 255) < 0 ||
        DxLib::SetDrawMode(DX_DRAWMODE_BILINEAR) < 0 ||
        DxLib::SetUseZBufferFlag(FALSE) < 0 || DxLib::SetWriteZBufferFlag(FALSE) < 0 ||
        DxLib::SetUseVertexShader(-1) < 0 || DxLib::SetUsePixelShader(-1) < 0)
    {
        return TResult<void>::Failure(EErrorCode::BackendFailure, "2D render-state reset failed");
    }
    return {};
}
TResult<void> FDxLibRenderBackend::Clear(FColor Color)
{
    if (Color.A != 255)
    {
        return TResult<void>::Failure(EErrorCode::InvalidArgument, "Native Clear accepts RGB only (A must be 255)");
    }
    auto Background = Detail::CheckNative_Internal(DxLib::SetBackgroundColor(Color.R, Color.G, Color.B), "SetBackgroundColor failed");
    if (!Background) { return Background; }
    return Detail::CheckNative_Internal(DxLib::ClearDrawScreen(), "ClearDrawScreen failed");
}
TResult<void> FDxLibRenderBackend::ApplyStyle_Internal(const FDrawStyle& Style, bool bSprite)
{
    if (!std::isfinite(Style.Opacity) || Style.Opacity < 0 || Style.Opacity > 1)
    {
        return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid opacity");
    }
    const int Alpha = static_cast<int>(std::lround(static_cast<float>(Style.Color.A) * Style.Opacity));
    if (DxLib::SetDrawBlendMode(DX_BLENDMODE_ALPHA, Alpha) < 0 ||
        DxLib::SetDrawBright(bSprite ? Style.Color.R : 255, bSprite ? Style.Color.G : 255, bSprite ? Style.Color.B : 255) < 0)
    {
        return TResult<void>::Failure(EErrorCode::BackendFailure, "Draw style application failed");
    }
    return {};
}
TResult<void> FDxLibRenderBackend::DrawSprite(const FSpriteCommand& Command)
{
    if (!Command.Texture.IsValid())
    {
        return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid texture");
    }
    auto Style = ApplyStyle_Internal(Command.Options, true);
    if (!Style) { return Style; }
    // Texture vertex identity stays fixed; changing corner positions implements both flips.
    const double Width = Command.Texture.GetWidth(), Height = Command.Texture.GetHeight();
    const double Cos = std::cos(Command.Options.RotationRadians), Sin = std::sin(Command.Options.RotationRadians);
    const std::array<double, 4> U{0, 1, 1, 0}, V{0, 0, 1, 1};
    std::array<float, 8> Vertices{};
    for (std::size_t Index = 0; Index < U.size(); ++Index)
    {
        const double X = ((Command.Options.bFlipX ? 1 - U[Index] : U[Index]) * Width - Command.Options.Pivot.X) * Command.Options.Scale.X;
        const double Y = ((Command.Options.bFlipY ? 1 - V[Index] : V[Index]) * Height - Command.Options.Pivot.Y) * Command.Options.Scale.Y;
        const double ScreenX = Command.Position.X + X * Cos - Y * Sin;
        const double ScreenY = Command.Position.Y + X * Sin + Y * Cos;
        if (!std::isfinite(ScreenX) || !std::isfinite(ScreenY) ||
            std::abs(ScreenX) > std::numeric_limits<float>::max() || std::abs(ScreenY) > std::numeric_limits<float>::max())
        {
            return TResult<void>::Failure(EErrorCode::InvalidArgument, "Sprite transform exceeds finite float coordinates");
        }
        Vertices[Index * 2] = static_cast<float>(ScreenX);
        Vertices[Index * 2 + 1] = static_cast<float>(ScreenY);
    }
    return Detail::CheckNative_Internal(DxLib::DrawModiGraphF(Vertices[0], Vertices[1], Vertices[2], Vertices[3],
        Vertices[4], Vertices[5], Vertices[6], Vertices[7], Command.Texture.GetNativeHandle_Internal(), TRUE), "DrawModiGraphF failed");
}
TResult<void> FDxLibRenderBackend::DrawText(const FTextCommand& Command)
{
    if (!Command.Font.IsValid())
    {
        return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid font");
    }
    auto Style = ApplyStyle_Internal(Command.Options, false);
    if (!Style) { return Style; }
    const auto& Color = Command.Options.Color;
    return Detail::CheckNative_Internal(DxLib::DrawStringFToHandle(Command.Position.X, Command.Position.Y, Command.Text.c_str(),
        DxLib::GetColor(Color.R, Color.G, Color.B), Command.Font.GetNativeHandle_Internal()), "DrawStringFToHandle failed");
}
TResult<void> FDxLibRenderBackend::DrawRectangle(const FRectangleCommand& Command)
{
    auto Style = ApplyStyle_Internal(Command.Options, false);
    if (!Style) { return Style; }
    const auto& Rect = Command.Rectangle;
    const auto& Color = Command.Options.Color;
    return Detail::CheckNative_Internal(DxLib::DrawBox(Rect.Left, Rect.Top, Rect.Right, Rect.Bottom,
        DxLib::GetColor(Color.R, Color.G, Color.B), TRUE), "DrawBox failed");
}
TResult<void> FDxLibRenderBackend::Present()
{
    return Detail::CheckNative_Internal(DxLib::ScreenFlip(), "ScreenFlip failed");
}
}
