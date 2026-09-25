// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_RENDERER_H
#define DXF_UI_RENDERER_H
#include "Dxf/Render2DContext.h"
#include "Dxf/UiDrawList.h"
#include "Dxf/UiSurface.h"
namespace Dxf
{
/**
 * 描画の列を既存の2D命令へ変える設定。
 */
struct FUiRenderOptions
{
	/**
	 * 命令のレイヤー（UIの前後関係。全画面のUIはViewportのUI・ワールドより大きい値にする）。
	 */
	Toolbox::int32 Layer = 1000;
	/**
	 * 同じレイヤー内の順序の開始値（列の順に1ずつ増やす。安定した順で描く）。
	 */
	Toolbox::int32 OrderBase = 0;
};

/**
 * 変換の結果の数。
 */
struct FUiRenderStats
{
	/**
	 * 送った命令の数。
	 */
	Toolbox::size_t Commands = 0;
	/**
	 * クリップが空で送らなかった数。
	 */
	Toolbox::size_t SkippedClipped = 0;
	/**
	 * フォント・画像が無効で送らなかった数。
	 */
	Toolbox::size_t SkippedInvalidResource = 0;
	/**
	 * 次に使える順序（続けて別の列を送るときのOrderBase）。
	 */
	Toolbox::int32 NextOrder = 0;
};

/**
 * 描画の列（論理座標）を表示面の変換で画素へ変え、既存のGet2D()の命令として送る。
 * 各命令は表示面の範囲と要素のクリップの共通部分を値で持つため、並べ替えの後も同じ範囲に切り抜かれる。
 * 矩形は辺ごとの切り捨て（半開区間）、文字は左上の切り捨ての位置、画像は描画先の矩形へ拡縮する。
 * @param Render 2Dの描画窓口。
 * @param List 描画の列。
 * @param Surface 表示面。
 * @param Options 設定。
 */
TResult<FUiRenderStats> SubmitUiDrawList(FRender2DContext& Render, const FUiDrawList& List, const FUiSurface& Surface,
                                         const FUiRenderOptions& Options = {});
} // namespace Dxf
#endif
