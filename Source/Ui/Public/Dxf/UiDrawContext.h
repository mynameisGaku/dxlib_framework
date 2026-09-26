// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_DRAW_CONTEXT_H
#define DXF_UI_DRAW_CONTEXT_H
#include "Dxf/UiDrawList.h"
#include "Dxf/UiSurface.h"
#include "Dxf/UiTextService.h"
namespace Dxf
{
/**
 * 要素の描画に渡す記録の窓口。座標はルートの論理座標で、現在のクリップと不透明度を各命令へ付ける。
 * 描画の実行（Backendの呼出し）はしない。
 */
class FUiDrawContext
{
public:
	/**
	 * @param List 記録先。
	 * @param Surface 表示面（文字の画素の大きさの決定に使う）。
	 * @param Text フォントの取得の窓口（なければ文字を描かない）。
	 */
	FUiDrawContext(FUiDrawList& List, const FUiSurface& Surface, IUiTextService* Text) noexcept
	    : m_pList(&List), m_pSurface(&Surface), m_pText(Text),
	      m_Clip{0, 0, Surface.GetLogicalSize().Width, Surface.GetLogicalSize().Height}
	{
	}
	FUiDrawContext(const FUiDrawContext&) = delete;
	FUiDrawContext& operator=(const FUiDrawContext&) = delete;
	/**
	 * 塗りつぶした矩形を記録する。
	 * @param Rect 論理座標の矩形。
	 * @param Color 色。
	 */
	void FillRect(const FUiRect& Rect, FColor Color);
	/**
	 * 矩形の内側に枠を記録する（四辺の塗りつぶし）。
	 * @param Rect 外周の矩形。
	 * @param Color 色。
	 * @param Width 枠の幅（論理単位）。
	 */
	void DrawBorder(const FUiRect& Rect, FColor Color, Toolbox::f32 Width);
	/**
	 * 一行の文字を記録する。
	 * @param Font 画素の大きさで解決したフォント。
	 * @param Text 一行の文字（共有の所有を参照数だけで渡す）。
	 * @param Position 左上（論理座標）。
	 * @param Size 文字の範囲の大きさ（論理単位、検査用）。
	 * @param Color 色。
	 */
	void DrawText(const FFont& Font, const FSharedText& Text, FVector2 Position, FUiSize Size, FColor Color);
	/**
	 * 画像を描画先の矩形へ拡縮して記録する。
	 * @param Texture 画像。
	 * @param Dest 描画先の矩形。
	 * @param Tint 乗算色。
	 */
	void DrawImage(const FTexture& Texture, const FUiRect& Dest, FColor Tint);
	/**
	 * 文字の大きさ（論理単位）の字体を表示面の画素の大きさで解決する。
	 * @param Family 字体名。
	 * @param LogicalSize 論理単位の大きさ。
	 */
	TResult<FFont> ResolveFont(const Toolbox::FString& Family, Toolbox::f32 LogicalSize);
	/**
	 * 表示面。
	 */
	FORCEINLINE const FUiSurface& GetSurface() const noexcept
	{
		return *m_pSurface;
	}
	/**
	 * 現在のクリップ（論理座標）。
	 */
	FORCEINLINE const FUiRect& GetClip() const noexcept
	{
		return m_Clip;
	}
	/**
	 * クリップを現在との共通部分へ狭める（戻す値を返す）。
	 * @param Rect 狭める矩形。
	 */
	FUiRect PushClip_Internal(const FUiRect& Rect) noexcept
	{
		const FUiRect Previous = m_Clip;
		m_Clip = m_Clip.Intersect(Rect);
		return Previous;
	}
	/**
	 * クリップを戻す。
	 * @param Previous PushClip_Internalが返した値。
	 */
	FORCEINLINE void PopClip_Internal(const FUiRect& Previous) noexcept
	{
		m_Clip = Previous;
	}
	/**
	 * クリップを置き換える（要素の配置で決めたクリップへ切り替える）。
	 * @param Clip 新しいクリップ。
	 */
	FORCEINLINE void SetClip_Internal(const FUiRect& Clip) noexcept
	{
		m_Clip = Clip;
	}
	/**
	 * 継承した不透明度を設定する（戻す値を返す）。
	 * @param Opacity 掛ける不透明度。
	 */
	Toolbox::f32 PushOpacity_Internal(Toolbox::f32 Opacity) noexcept
	{
		const Toolbox::f32 Previous = m_Opacity;
		m_Opacity = Toolbox::Clamp(m_Opacity * Opacity, 0.0f, 1.0f);
		return Previous;
	}
	/**
	 * 不透明度を戻す。
	 * @param Previous PushOpacity_Internalが返した値。
	 */
	FORCEINLINE void PopOpacity_Internal(Toolbox::f32 Previous) noexcept
	{
		m_Opacity = Previous;
	}
	/**
	 * 記録する命令に付ける要素の番号を設定する。
	 * @param Id 要素の番号。
	 */
	FORCEINLINE void SetSource_Internal(Toolbox::uint64 Id) noexcept
	{
		m_SourceId = Id;
	}

private:
	/**
	 * 命令を記録する（クリップが空なら記録しない）。
	 * @param Item 命令。
	 */
	void Add_Internal(FUiDrawItem Item);
	/**
	 * 記録先。
	 */
	FUiDrawList* m_pList;
	/**
	 * 表示面。
	 */
	const FUiSurface* m_pSurface;
	/**
	 * フォントの取得。
	 */
	IUiTextService* m_pText;
	/**
	 * 現在のクリップ。
	 */
	FUiRect m_Clip;
	/**
	 * 継承した不透明度。
	 */
	Toolbox::f32 m_Opacity = 1;
	/**
	 * 記録する命令の要素の番号。
	 */
	Toolbox::uint64 m_SourceId = 0;
};
} // namespace Dxf
#endif
