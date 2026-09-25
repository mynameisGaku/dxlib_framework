// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiDrawContext.h"
namespace Dxf
{
// 命令を記録する。
void FUiDrawContext::Add_Internal(FUiDrawItem Item)
{
	if (m_Clip.IsEmpty() || m_Opacity <= 0)
	{
		return;
	}
	Item.Clip = m_Clip;
	Item.Opacity = Toolbox::Clamp(Item.Opacity * m_Opacity, 0.0f, 1.0f);
	Item.SourceId = m_SourceId;
	m_pList->Add(Toolbox::Move(Item));
}
// 塗りつぶした矩形を記録する。
void FUiDrawContext::FillRect(const FUiRect& Rect, FColor Color)
{
	if (Rect.IsEmpty() || Color.A == 0)
	{
		return;
	}
	FUiDrawItem Item;
	Item.Kind = EUiDrawKind::Rectangle;
	Item.Rect = Rect;
	Item.Color = Color;
	Add_Internal(Toolbox::Move(Item));
}
// 枠を四辺の塗りつぶしで記録する。
void FUiDrawContext::DrawBorder(const FUiRect& Rect, FColor Color, Toolbox::f32 Width)
{
	if (Rect.IsEmpty() || !(Width > 0) || Color.A == 0)
	{
		return;
	}
	// 枠の幅（矩形の半分を超えない）。
	const Toolbox::f32 Thickness = Toolbox::Min(Width, Toolbox::Min(Rect.Width, Rect.Height) * 0.5f);
	FillRect({Rect.X, Rect.Y, Rect.Width, Thickness}, Color);
	FillRect({Rect.X, Rect.Bottom() - Thickness, Rect.Width, Thickness}, Color);
	FillRect({Rect.X, Rect.Y + Thickness, Thickness, Rect.Height - Thickness * 2}, Color);
	FillRect({Rect.Right() - Thickness, Rect.Y + Thickness, Thickness, Rect.Height - Thickness * 2}, Color);
}
// 一行の文字を記録する。
void FUiDrawContext::DrawText(const FFont& Font, const Toolbox::FString& Text, FVector2 Position, FUiSize Size,
                              FColor Color)
{
	if (Text.IsEmpty() || Color.A == 0)
	{
		return;
	}
	FUiDrawItem Item;
	Item.Kind = EUiDrawKind::Text;
	Item.Rect = {Position.X, Position.Y, Size.Width, Size.Height};
	Item.Color = Color;
	Item.Font = Font;
	Item.Text = Text;
	Add_Internal(Toolbox::Move(Item));
}
// 画像を記録する。
void FUiDrawContext::DrawImage(const FTexture& Texture, const FUiRect& Dest, FColor Tint)
{
	if (Dest.IsEmpty() || Tint.A == 0)
	{
		return;
	}
	FUiDrawItem Item;
	Item.Kind = EUiDrawKind::Image;
	Item.Rect = Dest;
	Item.Color = Tint;
	Item.Texture = Texture;
	Add_Internal(Toolbox::Move(Item));
}
// 字体を表示面の画素の大きさで解決する。
TResult<FFont> FUiDrawContext::ResolveFont(const Toolbox::FString& Family, Toolbox::f32 LogicalSize)
{
	if (m_pText == nullptr)
	{
		return TResult<FFont>::Failure(EErrorCode::InvalidState, "UI text service is not set");
	}
	return m_pText->ResolveFont({Family, m_pSurface->ToFontPixelSize(LogicalSize)});
}
} // namespace Dxf
