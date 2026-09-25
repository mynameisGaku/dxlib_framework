// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_PHYSICS_SCENE_2D_H
#define DXF_GAMEPLAY_PHYSICS_SCENE_2D_H
#include "Dxf/GameScene.h"
#include "Dxf/PrePhysicsStep.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/SceneNavigator.h"
#include "Toolbox/FixedStepScheduler.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 固定更新で2D物理を駆動するシーン。更新計画は時刻管理が作り、呼び出し元は一つ。
 * 可変更新の入力Snapshotは消費も変更もしない。音声は固定更新で扱わない。
 */
class DPhysicsScene2D : public DGameScene
{
public:
	/**
	 * 固定更新の刻み幅を受け取り、初期状態を構築する。
	 * @param Settings 固定更新と時間上限の設定。
	 */
	explicit DPhysicsScene2D(Toolbox::FFixedStepSettings Settings = {}) : m_Scheduler(Settings)
	{
	}
	/**
	 * 物理ワールドを取得する。
	 */
	FORCEINLINE FPhysicsWorld2D& GetPhysicsWorld() noexcept
	{
		return m_World;
	}
	/**
	 * 物理ワールドを取得する。
	 */
	FORCEINLINE const FPhysicsWorld2D& GetPhysicsWorld() const noexcept
	{
		return m_World;
	}
	/**
	 * 直近の描画補間割合を取得する。
	 */
	FORCEINLINE Toolbox::f64 GetInterpolationAlpha() const noexcept
	{
		return m_Alpha;
	}

protected:
	/**
	 * 固定更新の計画を立て、子階層へ配ってから物理を進める。
	 * @param Context フレーム更新の実行環境。
	 */
	void OnTick(const FTickContext& Context) final
	{
		if (Context.Time.bPaused)
		{
			m_PendingInputs.Clear();
			m_Alpha = 0;
			return;
		}
		// 固定更新の計画。無効な経過時間は例外で通知する。
		const Toolbox::FFixedStepPlan Plan = m_Scheduler.Advance(Context.Time.DeltaSeconds);
		m_Alpha = Plan.InterpolationAlpha;
		if (Plan.StepCount == 0)
		{
			// 未配達の押下を複写して次回へ持ち越す。
			if (m_PendingInputs.Size() < 8)
			{
				m_PendingInputs.PushBack(Context.Input);
			}
			return;
		}
		for (Toolbox::uint32 Step = 0; Step < Plan.StepCount; ++Step)
		{
			// 固定更新の実行環境。
			FFixedTickContext Fixed{
			    Context.Input,           m_PendingInputs, Plan.StepSeconds, Step,           Step == 0,    false,
			    Plan.InterpolationAlpha, &m_World,        nullptr,          Context.Scenes, Context.Game, &m_PreStep};
			// 子階層への配布結果（生成・登録・移動要求など）。
			m_PreStep.Clear_Internal();
			auto Dispatch = FixedTickChildren_Internal(Fixed);
			if (!Dispatch)
			{
				m_PreStep.Clear_Internal();
				throw Toolbox::FException(Dispatch.Error().Message);
			}
			if (Context.Scenes != nullptr && Context.Scenes->WantsQuit())
			{
				m_PreStep.Clear_Internal();
				break;
			}
			// 同じ固定更新の登録がすべて済んだ後、物理Stepの直前に予約された処理（キャラクター移動など）を行う。
			m_PreStep.Run_Internal(Fixed);
			m_World.Step(Plan.StepSeconds);
		}
		m_PendingInputs.Clear();
	}

private:
	/**
	 * 所有する2D物理ワールド。
	 */
	FPhysicsWorld2D m_World;
	/**
	 * 物理Stepの直前に行う処理の予約（固定更新1回分）。
	 */
	FPrePhysicsStepQueue m_PreStep;
	/**
	 * 固定更新の計画を作る時刻管理。
	 */
	Toolbox::FFixedStepScheduler m_Scheduler;
	/**
	 * 固定更新が0回だったフレームの入力複写。
	 */
	Toolbox::TVector<FInputSnapshot> m_PendingInputs;
	/**
	 * 直近の描画補間割合。
	 */
	Toolbox::f64 m_Alpha = 0;
};
} // namespace Dxf
#endif
