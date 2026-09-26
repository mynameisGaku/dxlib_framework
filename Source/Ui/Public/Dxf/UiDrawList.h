// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_DRAW_LIST_H
#define DXF_UI_DRAW_LIST_H
#include "Dxf/Font.h"
#include "Dxf/SharedText.h"
#include "Dxf/Texture.h"
#include "Dxf/UiTypes.h"
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 描画の要素の種類。
 */
enum class EUiDrawKind : Toolbox::uint8
{
	/**
	 * 塗りつぶした矩形。
	 */
	Rectangle,
	/**
	 * 一行の文字。
	 */
	Text,
	/**
	 * 画像（描画先の矩形へ拡縮して描く）。
	 */
	Image
};

/**
 * ルートの論理座標で記録した描画の一件。命令ごとにクリップ矩形を値で持つ（後段の並べ替えで崩れない）。
 */
struct FUiDrawItem
{
	/**
	 * 種類。
	 */
	EUiDrawKind Kind = EUiDrawKind::Rectangle;
	/**
	 * 矩形・画像の描画先・文字の左上（幅と高さは文字の範囲）。
	 */
	FUiRect Rect;
	/**
	 * 有効なクリップ矩形（論理座標）。空なら何も描かない。
	 */
	FUiRect Clip;
	/**
	 * 色（画像では乗算色）。
	 */
	FColor Color;
	/**
	 * 不透明度（0〜1）。
	 */
	Toolbox::f32 Opacity = 1;
	/**
	 * 文字のフォント（描画面の画素の大きさで解決済み）。
	 */
	FFont Font;
	/**
	 * 一行の文字（共有の所有。要素が保持する配置済みの文字を複製せずに参照する）。
	 */
	FSharedText Text;
	/**
	 * 画像。
	 */
	FTexture Texture;
	/**
	 * 記録した要素の番号（検査・診断で使う。0は不明）。
	 */
	Toolbox::uint64 SourceId = 0;
};

/**
 * 一回の描画でルートが記録した描画の列（描く順）。
 */
class FUiDrawList
{
public:
	/**
	 * 一件を加える。
	 * @param Item 描画の一件。
	 */
	FORCEINLINE void Add(FUiDrawItem Item)
	{
		m_Items.PushBack(Toolbox::Move(Item));
	}
	/**
	 * 描く順の一覧。
	 */
	FORCEINLINE const Toolbox::TVector<FUiDrawItem>& GetItems() const noexcept
	{
		return m_Items;
	}
	/**
	 * 描く順の一覧（クリップを後から狭める表示先の処理が使う）。
	 */
	FORCEINLINE Toolbox::TVector<FUiDrawItem>& EditItems() noexcept
	{
		return m_Items;
	}
	/**
	 * 空にする（確保済みの領域は保つ）。
	 */
	FORCEINLINE void Clear() noexcept
	{
		m_Items.Clear();
	}

private:
	/**
	 * 描く順の一覧。
	 */
	Toolbox::TVector<FUiDrawItem> m_Items;
};
} // namespace Dxf
#endif
