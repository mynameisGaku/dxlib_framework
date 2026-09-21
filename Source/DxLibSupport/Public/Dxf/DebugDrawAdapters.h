// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_DEBUG_DRAW_ADAPTERS_H
#define DXF_DEBUG_DRAW_ADAPTERS_H
#include "Dxf/DebugDrawStore.h"
#include "Dxf/RenderContext.h"
namespace Dxf
{
/**
 * 所有者・カテゴリで選んだ調査用2D線を、通常の2D命令へ変換する。
 * @param Store 変更せず参照する記録。物理WorldやNative資源を所有しない。
 * @param Filter 対象ビューの選択条件。
 * @param Render 共有実行器が設定済みの2D窓口。
 */
TResult<void> SubmitDebugDraw(const FDebugDrawStore2D& Store,
const FDebugDrawFilter& Filter, FRender2DContext& Render);
/**
 * 物理などから採取した値の記録を、対象ビューの3D命令へ変換する。
 * @param Store 変更せず参照する記録。Snapshotの寿命は生成完了まで保持する。
 * @param Filter 対象ビューのカテゴリ・選択条件。
 * @param Render 共有実行器が設定済みの3D窓口。
 */
TResult<void> SubmitDebugDraw(const FDebugDrawStore3D& Store,
const FDebugDrawFilter& Filter, FRender3DContext& Render);
}
#endif
