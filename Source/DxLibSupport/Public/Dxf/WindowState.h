// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WINDOW_STATE_H
#define DXF_WINDOW_STATE_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 利用者によるウィンドウの拡縮の扱い。
 */
enum class EWindowResizeMode : Toolbox::uint8
{
	/**
	 * 拡縮しない（既定。起動時の幅・高さの描画先とウィンドウ）。
	 */
	Fixed,
	/**
	 * ウィンドウは拡縮でき、描画先は起動時の寸法のまま全体へ引き伸ばして表示する（縦横比は保たない）。
	 * 入力の位置は描画先の画素で届く。
	 */
	Stretch,
	/**
	 * ウィンドウの拡縮に合わせて、描画先の寸法をクライアント領域と同じ画素数へ変える（フレームの境界で反映）。
	 */
	Resizable
};

/**
 * フレームの境界で確定したウィンドウの状態。OSの通知から直接は変えず、Applicationが各フレームの最初に取得する。
 */
struct FWindowState
{
	/**
	 * Platformが状態を取得できたか（falseは「情報なし」で、0x0の表示不能とは別）。
	 */
	bool bKnown = false;
	/**
	 * クライアント領域の画素数（最小化中は0）。
	 */
	Toolbox::int32 ClientWidth = 0;
	Toolbox::int32 ClientHeight = 0;
	/**
	 * このフレームの描画先（画面）の画素数。入力の位置と描画はこの画素で扱う。
	 */
	Toolbox::int32 RenderWidth = 0;
	Toolbox::int32 RenderHeight = 0;
	/**
	 * 最小化されているか。
	 */
	bool bMinimized = false;
	/**
	 * 入力のフォーカスを持つか。
	 */
	bool bFocused = true;
	/**
	 * ウィンドウが表示されているモニターのDPI（96が等倍）。
	 */
	Toolbox::int32 Dpi = 96;
	/**
	 * 寸法・最小化・フォーカス・DPIのいずれかが変わるたびに増える番号。
	 */
	Toolbox::uint64 Revision = 0;
	/**
	 * 描画できるか（状態が不明なら描画できるものとして扱う）。
	 */
	FORCEINLINE bool CanRender() const noexcept
	{
		return !bKnown || (!bMinimized && RenderWidth > 0 && RenderHeight > 0);
	}
	/**
	 * 同じ状態か（変更番号を除く）。
	 */
	FORCEINLINE bool IsSameState(const FWindowState& Other) const noexcept
	{
		return bKnown == Other.bKnown && ClientWidth == Other.ClientWidth && ClientHeight == Other.ClientHeight &&
		       RenderWidth == Other.RenderWidth && RenderHeight == Other.RenderHeight &&
		       bMinimized == Other.bMinimized && bFocused == Other.bFocused && Dpi == Other.Dpi;
	}
};
} // namespace Dxf
#endif
