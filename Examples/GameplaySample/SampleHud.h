// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_HUD_H
#define DXF_GAMEPLAY_SAMPLE_HUD_H
#include "Dxf/CharacterGroundState.h"
#include "Dxf/CharacterMoveStop.h"
#include "Dxf/CharacterRecoveryStatus.h"
#include "Dxf/Result.h"
#include <stdio.h>
namespace Dxf::GameplaySample
{
/**
 * 失敗を例外へ変え、Applicationの失敗として報告させる。
 * @param Result 処理結果。
 */
void RequireSample(TResult<void> Result);
/**
 * 停止理由の表示名。
 * @param Stop 停止理由。
 */
const char* MoveStopName(ECharacterMoveStop Stop) noexcept;
/**
 * 足元の表示名。
 * @param State 足元の状態。
 */
const char* GroundName(ECharacterGroundState State) noexcept;
/**
 * 重なりの解消の表示名。
 * @param Status 解消の結果。
 */
const char* RecoveryName(ECharacterRecoveryStatus Status) noexcept;
/**
 * 直近の固定更新の変化（ジャンプ・着地・離地・天井・段差・吸い付き）を文字列にする。
 * @param Step 固定更新の結果（2D／3D）。
 * @param Text 出力先。
 */
template <typename TStep> void StepEventText(const TStep& Step, char (&Text)[64])
{
	snprintf(Text, sizeof(Text), "%s%s%s%s%s%s", Step.bJumped ? "jump " : "", Step.bLanded ? "landed " : "",
	         Step.bLeftGround ? "left-ground " : "", Step.bHitCeiling ? "ceiling " : "",
	         Step.bSteppedUp ? "step-up " : "", Step.bSnapped ? "snap " : "");
}
} // namespace Dxf::GameplaySample
#endif
