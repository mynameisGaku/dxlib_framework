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
	 * 自アプリのウィンドウへマウスの捕捉（ウィンドウの外の移動・解放も受ける）を要求できるか。
	 */
	bool bPointerCaptureSupported = false;
	/**
	 * 自アプリのウィンドウがマウスを捕捉しているか（要求したことと、実際に取得できたことは別）。
	 */
	bool bPointerCaptured = false;
	/**
	 * 現在の拡縮の扱い（Platformが反映している値）。
	 */
	EWindowResizeMode ResizeMode = EWindowResizeMode::Fixed;
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
/**
 * マウスカーソルの形（標準のカーソルへ対応させる意図）。
 */
enum class ECursorShape : Toolbox::uint8
{
	/**
	 * 標準の矢印。
	 */
	Arrow,
	/**
	 * 操作できる対象の上。
	 */
	Hand,
	/**
	 * 横方向のドラッグ。
	 */
	ResizeHorizontal,
	/**
	 * 縦方向のドラッグ。
	 */
	ResizeVertical
};

/**
 * 1フレームの間に更新・入力の仲介がPlatformへ求める操作。Applicationがフレームの境界でまとめて反映する。
 */
struct FPlatformRequests
{
	/**
	 * 自アプリのウィンドウへマウスの捕捉を求めるか（UIのドラッグ中等）。
	 */
	bool bPointerCapture = false;
	/**
	 * カーソルの形を指定したか、と指定した形。
	 */
	bool bCursorRequested = false;
	ECursorShape Cursor = ECursorShape::Arrow;
	/**
	 * 拡縮の扱いの変更を求めるか、と求める扱い（設定画面等。フレームの境界で反映し、描画中の資源は作り直さない）。
	 */
	bool bResizeModeRequested = false;
	EWindowResizeMode ResizeMode = EWindowResizeMode::Fixed;
};
} // namespace Dxf
#endif
