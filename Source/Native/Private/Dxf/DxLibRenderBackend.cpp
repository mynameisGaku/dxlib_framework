#include "Dxf/DxLibRenderBackend.h"
#include "NativeApi.h"
#include "Toolbox/Array.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
// 描画先と描画範囲を設定する。
// @param Handle ハンドル。
// @param Width 幅。
// @param Height 高さ。
TResult<void> FDxLibRenderBackend::SetTarget(Toolbox::int32 Handle, Toolbox::int32 Width, Toolbox::int32 Height)
{
	if (Width <= 0 || Height <= 0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid draw target dimensions");
	}
	// 描画先またはその設定結果。
	auto Target = Detail::CheckNative_Internal(DxLib::SetDrawScreen(Handle < 0 ? DX_SCREEN_BACK : Handle),
	"SetDrawScreen failed");
	if (!Target)
	{
		return Target;
	}
	m_Width2D = Width;
	m_Height2D = Height;
	return Detail::CheckNative_Internal(DxLib::SetDrawArea(0, 0, Width, Height), "SetDrawArea failed");
}
// 2D命令のクリップを設定する。
// @param bEnabled 切り抜くか。
// @param Rect 描画先の画素の矩形。
TResult<void> FDxLibRenderBackend::SetClip2D(bool bEnabled, FIntRect Rect)
{
	if (!bEnabled)
	{
		return Detail::CheckNative_Internal(DxLib::SetDrawArea(0, 0, m_Width2D, m_Height2D), "SetDrawArea failed");
	}
	// 描画先の範囲との共通部分（SetDrawAreaは右・下を含まない範囲）。
	const Toolbox::int32 Left = Toolbox::Clamp(Rect.Left, 0, m_Width2D);
	const Toolbox::int32 Top = Toolbox::Clamp(Rect.Top, 0, m_Height2D);
	const Toolbox::int32 Right = Toolbox::Clamp(Rect.Right, Left, m_Width2D);
	const Toolbox::int32 Bottom = Toolbox::Clamp(Rect.Bottom, Top, m_Height2D);
	return Detail::CheckNative_Internal(DxLib::SetDrawArea(Left, Top, Right, Bottom), "SetDrawArea failed");
}
// 2D描画に必要な状態へ戻す。
// @param Width 幅。
// @param Height 高さ。
TResult<void> FDxLibRenderBackend::ResetState(Toolbox::int32 Width, Toolbox::int32 Height)
{
	m_Width2D = Width;
	m_Height2D = Height;
	if (DxLib::SetDrawArea(0, 0, Width, Height) < 0 || DxLib::SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255) < 0 ||
	DxLib::SetDrawBright(255, 255, 255) < 0 || DxLib::SetDrawMode(DX_DRAWMODE_BILINEAR) < 0 ||
	DxLib::SetUseZBufferFlag(FALSE) < 0 || DxLib::SetWriteZBufferFlag(FALSE) < 0 ||
	DxLib::SetUseVertexShader(-1) < 0 || DxLib::SetUsePixelShader(-1) < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "2D render-state reset failed");
	}
	return {};
}
// 蓄積した内容を消去する。
// @param Color 描画色。
TResult<void> FDxLibRenderBackend::Clear(FColor Color)
{
	if (Color.A != 255)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Native Clear accepts RGB only (A must be 255)");
	}
	// 背景の描画設定または設定結果。
	auto Background =
	Detail::CheckNative_Internal(DxLib::SetBackgroundColor(Color.R, Color.G, Color.B), "SetBackgroundColor failed");
	if (!Background)
	{
		return Background;
	}
	return Detail::CheckNative_Internal(DxLib::ClearDrawScreen(), "ClearDrawScreen failed");
}
// 描画色と不透明度をバックエンドへ適用する。
// @param Style 描画状態またはその適用結果。
// @param bSprite スプライト用の色変調を適用するか。
TResult<void> FDxLibRenderBackend::ApplyStyle_Internal(const FDrawStyle& Style, bool bSprite)
{
	if (!Toolbox::IsFinite(Style.Opacity) || Style.Opacity < 0 || Style.Opacity > 1)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid opacity");
	}
	// アルファ値または透過の使用設定。
	const Toolbox::int32 Alpha =
	static_cast<Toolbox::int32>(Toolbox::RoundToLong(static_cast<Toolbox::f32>(Style.Color.A) * Style.Opacity));
	if (DxLib::SetDrawBlendMode(DX_BLENDMODE_ALPHA, Alpha) < 0 ||
	DxLib::SetDrawBright(bSprite ? Style.Color.R : 255, bSprite ? Style.Color.G : 255,
	bSprite ? Style.Color.B : 255) < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Draw style application failed");
	}
	return {};
}
// スプライトの描画命令を処理する。
// @param Command 実行する描画命令。
TResult<void> FDxLibRenderBackend::DrawSprite(const FSpriteCommand& Command)
{
	if (!Command.Texture.IsValid())
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid texture");
	}
	// 描画状態またはその適用結果。
	auto Style = ApplyStyle_Internal(Command.Options, true);
	if (!Style)
	{
		return Style;
	}
	// テクスチャ頂点の対応を固定し、四隅の位置を変更して反転する。
	//
	// 幅。
	const Toolbox::f64 Width = Command.Texture.GetWidth();
	// 元画像の高さ。
	const Toolbox::f64 Height = Command.Texture.GetHeight();
	// 回転角の余弦。
	const Toolbox::f64 Cos = Toolbox::Cos(Command.Options.RotationRadians);
	// 回転角の正弦。
	const Toolbox::f64 Sin = Toolbox::Sin(Command.Options.RotationRadians);
	// 頂点ごとの横方向のテクスチャ座標。
	const Toolbox::TArray<Toolbox::f64, 4> U{0, 1, 1, 0};
	// 頂点ごとの縦方向のテクスチャ座標。
	const Toolbox::TArray<Toolbox::f64, 4> V{0, 0, 1, 1};
	// 変換後の描画頂点。
	Toolbox::TArray<Toolbox::f32, 8> Vertices{};
	// 要素の位置を進めて順に処理する。
	for (Toolbox::size_t Index = 0; Index < U.Size(); ++Index)
	{
		// X座標。
		const Toolbox::f64 X = ((Command.Options.bFlipX ? 1 - U[Index] : U[Index]) * Width - Command.Options.Pivot.X) *
		Command.Options.Scale.X;
		// Y座標。
		const Toolbox::f64 Y = ((Command.Options.bFlipY ? 1 - V[Index] : V[Index]) * Height - Command.Options.Pivot.Y) *
		Command.Options.Scale.Y;
		// 変換後の画面X座標。
		const Toolbox::f64 ScreenX = Command.Position.X + X * Cos - Y * Sin;
		// 変換後の画面Y座標。
		const Toolbox::f64 ScreenY = Command.Position.Y + X * Sin + Y * Cos;
		if (!Toolbox::IsFinite(ScreenX) || !Toolbox::IsFinite(ScreenY) ||
		Toolbox::Abs(ScreenX) > Toolbox::TNumericLimits<Toolbox::f32>::Max() ||
		Toolbox::Abs(ScreenY) > Toolbox::TNumericLimits<Toolbox::f32>::Max())
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument,
			"Sprite transform exceeds finite float coordinates");
		}
		Vertices[Index * 2] = static_cast<Toolbox::f32>(ScreenX);
		Vertices[Index * 2 + 1] = static_cast<Toolbox::f32>(ScreenY);
	}
	return Detail::CheckNative_Internal(DxLib::DrawModiGraphF(Vertices[0], Vertices[1], Vertices[2], Vertices[3],
	Vertices[4], Vertices[5], Vertices[6], Vertices[7],
	Command.Texture.GetNativeHandle_Internal(), TRUE),
	"DrawModiGraphF failed");
}
// 文字列の描画命令を処理する。
// @param Command 実行する描画命令。
TResult<void> FDxLibRenderBackend::DrawText(const FTextCommand& Command)
{
	if (!Command.Font.IsValid())
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid font");
	}
	// 描画状態またはその適用結果。
	auto Style = ApplyStyle_Internal(Command.Options, false);
	if (!Style)
	{
		return Style;
	}
	// 描画色。
	const auto& Color = Command.Options.Color;
	return Detail::CheckNative_Internal(
	DxLib::DrawStringFToHandle(Command.Position.X, Command.Position.Y, Command.Text.CStr(),
	DxLib::GetColor(Color.R, Color.G, Color.B), Command.Font.GetNativeHandle_Internal()),
	"DrawStringFToHandle failed");
}
// 矩形の描画命令を処理する。
// @param Command 実行する描画命令。
TResult<void> FDxLibRenderBackend::DrawRectangle(const FRectangleCommand& Command)
{
	// 描画状態またはその適用結果。
	auto Style = ApplyStyle_Internal(Command.Options, false);
	if (!Style)
	{
		return Style;
	}
	// 描画する矩形。
	const auto& Rect = Command.Rectangle;
	// 描画色。
	const auto& Color = Command.Options.Color;
	return Detail::CheckNative_Internal(
	DxLib::DrawBox(Rect.Left, Rect.Top, Rect.Right, Rect.Bottom, DxLib::GetColor(Color.R, Color.G, Color.B), Command.bFilled ? TRUE : FALSE),
	"DrawBox failed");
}
// 描画したフレームを画面へ提示する。
TResult<void> FDxLibRenderBackend::Present()
{
	return Detail::CheckNative_Internal(DxLib::ScreenFlip(), "ScreenFlip failed");
}
}
// namespace Dxf
