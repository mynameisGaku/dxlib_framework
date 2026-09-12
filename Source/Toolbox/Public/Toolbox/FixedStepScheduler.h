// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_FIXED_STEP_SCHEDULER_H
#define TOOLBOX_FIXED_STEP_SCHEDULER_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 固定時間更新の設定。シーンや物理次元に依存しない。
 */
struct FFixedStepSettings
{
	/**
	 * 一回の更新に渡す有限・正の秒数。
	 */
	f64 StepSeconds = 1.0 / 60.0;
	/**
	 * 一フレームに実行可能な更新数。防御的上限として1〜1024を受け付ける。
	 */
	uint32 MaxStepsPerFrame = 8;
	/**
	 * 一フレームで受け入れる有限・正の実経過秒数。超過分はDroppedSecondsで返す。
	 */
	f64 MaximumFrameSeconds = 0.25;
};
/**
 * 消費済み時間から組み立てた更新計画。実際の更新処理は呼び出し元が行う。
 */
struct FFixedStepPlan
{
	/**
	 * このフレームに実行する更新数。
	 */
	uint32 StepCount = 0;
	/**
	 * 各更新へ渡す固定秒数。
	 */
	f64 StepSeconds = 0;
	/**
	 * 描画補間用の残余割合。0以上1未満。
	 */
	f64 InterpolationAlpha = 0;
	/**
	 * フレーム時間上限または更新数上限によって捨てた秒数。
	 */
	f64 DroppedSeconds = 0;
};
/**
 * 経過秒数を固定更新の個数へ変換する単一スレッド用の時刻管理。
 * 上限を超えた完全ステップは明示的に破棄し、一ステップ未満の余りを保持する。
 * コールバック、衝突応答、状態のロールバックは担当しない。
 */
class FFixedStepScheduler
{
public:
	/**
	 * 更新幅と追いつき処理の上限を設定する。不正な設定はFException。
	 * @param Settings 固定更新と時間上限の設定。
	 */
	explicit FFixedStepScheduler(FFixedStepSettings Settings = {});
	/**
	 * 経過時間を追加し、計画分の時間を消費する。
	 * 非有限値、負数、内部表現の桁あふれは例外。失敗時は以前の余りを保持する。
	 * @param ElapsedSeconds 前回からの実経過秒数。
	 */
	FFixedStepPlan Advance(f64 ElapsedSeconds);
	/**
	 * 残余時間だけを消去する。
	 */
	FORCEINLINE void Reset() noexcept
	{
		m_Accumulator = 0;
	}
	/**
	 * 次フレームへ持ち越す一ステップ未満の秒数を返す。
	 */
	FORCEINLINE f64 GetRemainderSeconds() const noexcept
	{
		return m_Accumulator;
	}
	/**
	 * 更新一回あたりの秒数を返す。
	 */
	FORCEINLINE f64 GetStepSeconds() const noexcept
	{
		return m_Settings.StepSeconds;
	}

private:
	/**
	 * 構築時に検査済みの時刻管理設定。
	 */
	FFixedStepSettings m_Settings;
	/**
	 * まだ固定更新へ渡していない秒数。
	 */
	f64 m_Accumulator = 0;
};
} // namespace Toolbox
#endif
