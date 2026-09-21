// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_DEBUG_STEP_CONTROLLER_H
#define DXF_DEBUG_STEP_CONTROLLER_H
#include "Dxf/Result.h"
#include "Toolbox/FixedStepScheduler.h"
namespace Dxf
{
/**
 * 固定更新の実行計画。実Worldを更新したという意味ではない。
 */
struct FDebugStepPlan
{
	/**
	 * 呼出し側が実行する固定更新回数。
	 */
	Toolbox::uint32 StepCount = 0;
	/**
	 * 一回の固定更新秒数。
	 */
	Toolbox::f64 StepSeconds = 1.0 / 60.0;
	/**
	 * 過負荷の上限処理で進めずに捨てたゲーム秒数。
	 */
	Toolbox::f64 DroppedSeconds = 0;
};
/**
 * ゲーム停止と実時間のカメラ操作を分ける、デバッグシーン用の時間計画器。
 * Planは時間を消費する。計画を実行できなければ呼出し側はフレームを失敗にする。
 */
class FDebugStepController
{
public:
	/**
	 * 停止状態・倍率を維持し、残余時間と手送り要求だけを初期化する。
	 */
	void Reset() noexcept;
	/**
	 * @param Paused 切替時に端数時間と未実行の手送り要求を捨てる。
	 */
	void SetPaused(bool Paused) noexcept;
	/**
	 * ゲーム側の停止状態。
	 */
	FORCEINLINE bool IsPaused() const noexcept
	{
		return m_bPaused;
	}
	/**
	 * 停止時だけ固定更新一回を予約する。既に予約済みならfalse。
	 */
	bool RequestSingleStep() noexcept;
	/**
	 * @param Scale 0.01〜4の有限なゲーム時間倍率。
	 */
	TResult<void> SetTimeScale(Toolbox::f64 Scale);
	/**
	 * @param RealSeconds 0〜3600の有限な実経過秒。最大8回まで計画する。
	 */
	TResult<FDebugStepPlan> Plan(Toolbox::f64 RealSeconds);
private:
	/**
	 * Toolboxに集約された固定時間計画器。
	 */
	Toolbox::FFixedStepScheduler m_Scheduler;
	/**
	 * ゲーム側だけに適用する時間倍率。
	 */
	Toolbox::f64 m_TimeScale = 1;
	/**
	 * ゲームを停止しているか。
	 */
	bool m_bPaused = false;
	/**
	 * 手送りが未配達か。
	 */
	bool m_bPendingStep = false;
};
}
#endif
