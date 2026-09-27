// SPDX-License-Identifier: NOASSERTION
#include "InteractionConsumer.h"
#include "Dxf/ContactListenerComponent2D.h"
#include "Dxf/ContactListenerComponent3D.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/KinematicMoverComponent2D.h"
#include "Dxf/KinematicMoverComponent3D.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/RenderContext.h"
#include "Dxf/TriggerVolumeComponent2D.h"
#include "Dxf/TriggerVolumeComponent3D.h"

namespace
{
// CPU利用者とApplicationが共有する固定更新の刻み。
constexpr Toolbox::f64 StepSeconds = 1.0 / 60.0;

// 失敗をScene／Applicationの通常の例外変換へ渡す。
void RequireInteraction(bool bCondition, const char* Message)
{
	if (!bCondition)
	{
		throw Toolbox::FException(Message);
	}
}

// 2Dの公開Componentと配置・描画だけをまとめる。
struct FInteraction2D
{
	using FScene = Dxf::DPhysicsScene2D;
	using FCharacter = Dxf::DCharacterMovement2DComponent;
	using FCharacterDescription = Dxf::FCharacterMovementDescription2D;
	using FMover = Dxf::DKinematicMover2DComponent;
	using FMoverDescription = Dxf::FKinematicMoverDescription2D;
	using FPose = Dxf::FKinematicPose2D;
	using FTrigger = Dxf::DTriggerVolume2DComponent;
	using FTriggerDescription = Dxf::FTriggerVolumeDescription2D;
	using FListener = Dxf::DContactListener2DComponent;
	using FNotice = Dxf::FContactNotice2D;
	using FBodyId = Dxf::FBodyId2D;
	using FVector = Toolbox::FVector2;
	using FCollider = Dxf::FColliderDescription2D;
	// 3Dと同じ高さ・幅を2Dの点へ変換する。
	static FVector Point(Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {X, Y};
	}
	// 原点を中心とする床・領域の矩形。
	static FCollider Box(Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FCollider Description;
		Description.Shape = Toolbox::FOrientedBox2D{{}, {HalfX, HalfY}, 0};
		return Description;
	}
	// Applicationでは実際の移動状態を2D描画へも接続する。
	static void Draw(Dxf::FRenderContext& Render, FVector Character, FVector Floor)
	{
		Dxf::FDrawStyle Style;
		Style.Color = {60, 160, 220, 255};
		// 矩形の画素位置だけ整数へ変換し、物理座標は変更しない。
		const Toolbox::int32 Left = static_cast<Toolbox::int32>(100 + (Floor.X - 4) * 40);
		const Toolbox::int32 Top = static_cast<Toolbox::int32>(500 - (Floor.Y + 0.25f) * 40);
		(void)Render.Get2D().FillRectangle({Left, Top, 320, 20}, Style);
		Style.Color = {255, 200, 40, 255};
		(void)Render.Get2D().FillCircle({100 + Character.X * 40, 500 - Character.Y * 40}, 20, Style);
	}
};

// 3Dの公開Componentと、2Dに対応する配置・描画。
struct FInteraction3D
{
	using FScene = Dxf::DPhysicsScene3D;
	using FCharacter = Dxf::DCharacterMovement3DComponent;
	using FCharacterDescription = Dxf::FCharacterMovementDescription3D;
	using FMover = Dxf::DKinematicMover3DComponent;
	using FMoverDescription = Dxf::FKinematicMoverDescription3D;
	using FPose = Dxf::FKinematicPose3D;
	using FTrigger = Dxf::DTriggerVolume3DComponent;
	using FTriggerDescription = Dxf::FTriggerVolumeDescription3D;
	using FListener = Dxf::DContactListener3DComponent;
	using FNotice = Dxf::FContactNotice3D;
	using FBodyId = Dxf::FBodyId3D;
	using FVector = Toolbox::FVector3;
	using FCollider = Dxf::FColliderDescription3D;
	// 奥行き0の点を返す。
	static FVector Point(Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {X, Y, 0};
	}
	// 奥行きにも余裕のある床・領域を返す。
	static FCollider Box(Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FCollider Description;
		Description.Shape = Toolbox::FOBB{{}, {HalfX, HalfY, HalfX}};
		return Description;
	}
	// Applicationでは球と床を3Dの描画窓口へ送る。
	static void Draw(Dxf::FRenderContext& Render, FVector Character, FVector Floor)
	{
		Dxf::FRenderView3D View;
		View.Eye = {0, 7, -12};
		View.Target = {1, 1, 0};
		(void)Render.Get3D().SetView(View);
		Dxf::FDrawStyle3D Style;
		Style.Color = {60, 160, 220, 255};
		(void)Render.Get3D().DrawBox({Floor, {4, 0.25f, 4}}, Style);
		Style.Color = {255, 200, 40, 255};
		(void)Render.Get3D().DrawSphere({Character, 0.5f}, Style, 16);
	}
};

// 移動床のBodyを一つのComponentだけで所有する。
template <typename T> class TConsumerFloor final : public Dxf::DGameObject
{
public:
	// 同じSceneの検証役が参照する世代付きComponentハンドル。
	Dxf::TObjectHandle<typename T::FMover> Mover;

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		// 上面y=0の床。最初の固定更新は静止し、以後は2m/sで移動する。
		typename T::FMoverDescription Description;
		Description.Pose.Position = T::Point(0, -0.25f);
		Description.Colliders.PushBack(T::Box(4, 0.25f));
		auto Added = this->template AddComponent<typename T::FMover>(Description);
		if (!Added)
		{
			return Dxf::TResult<void>::Failure(Added.Error());
		}
		Mover = Added.Value();
		Mover.Get()->SetPath(
		    [](Toolbox::f64 Seconds)
		    {
			    typename T::FPose Pose;
			    Pose.Position =
			        T::Point(static_cast<Toolbox::f32>(2 * Toolbox::Max(0.0, Seconds - StepSeconds)), -0.25f);
			    return Pose;
		    });
		return {};
	}
};

// 最初の通知で自身を破棄する取得物。後続Componentへの配送抑止も確認する。
template <typename T> class TConsumerPickup final : public Dxf::DGameObject
{
public:
	explicit TConsumerPickup(FInteractionConsumerResult& Result) : m_Result(Result)
	{
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		// プレイヤーだけが重なる位置。床とは接触しない。
		typename T::FTriggerDescription Description;
		Description.Position = T::Point(0, 0.65f);
		Description.Shape = T::Box(0.2f, 0.2f).Shape;
		auto First = this->template AddComponent<typename T::FTrigger>(Description);
		auto Late = this->template AddComponent<typename T::FTrigger>(Description);
		if (!First || !Late)
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidState, "consumer pickup components");
		}
		First.Value().Get()->SetEnterHandler(
		    [this](typename T::FBodyId)
		    {
			    ++m_Result.Pickups;
			    Destroy();
		    });
		Late.Value().Get()->SetEnterHandler(
		    [this](typename T::FBodyId)
		    {
			    ++m_Result.LatePickupCalls;
		    });
		return {};
	}

private:
	// Sceneより長く生存する呼び出し側の結果。
	FInteractionConsumerResult& m_Result;
};

// 既存のキャラクターComponentを使い、固定更新の観測とゲーム側の入力だけを担当する。
template <typename T> class TConsumerRider final : public Dxf::DGameObject
{
public:
	TConsumerRider(FInteractionConsumerResult& Result, Dxf::TObjectHandle<TConsumerFloor<T>> Floor)
	    : m_Result(Result), m_Floor(Floor)
	{
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		// 歩行入力なしで、床が運んだ量だけを確認する。
		typename T::FCharacterDescription Description;
		Description.Center = T::Point(0, 0.52f);
		auto Character = this->template AddComponent<typename T::FCharacter>(Description);
		auto Listener = this->template AddComponent<typename T::FListener>();
		typename T::FTriggerDescription ZoneDescription;
		ZoneDescription.Position = T::Point(0, 0.8f);
		ZoneDescription.Shape = T::Box(5, 0.2f).Shape;
		auto Zone = this->template AddComponent<typename T::FTrigger>(ZoneDescription);
		if (!Character || !Listener || !Zone)
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidState, "consumer rider components");
		}
		m_Character = Character.Value();
		Listener.Value().Get()->SetHandler(
		    [this](const typename T::FNotice& Notice)
		    {
			    RequireInteraction(Notice.BatchId > 0 && Notice.StepIndex > 0, "consumer unpublished delivery");
			    if (Notice.Kind == Dxf::EWorldEventKind::Trigger)
			    {
				    m_Result.Begins += Notice.Phase == Dxf::EWorldEventPhase::Begin ? 1 : 0;
				    m_Result.Stays += Notice.Phase == Dxf::EWorldEventPhase::Stay ? 1 : 0;
				    m_Result.RemovedEnds += Notice.Phase == Dxf::EWorldEventPhase::End &&
				                                    Notice.EndReason == Dxf::EWorldEventEndReason::Removed
				                                ? 1
				                                : 0;
			    }
		    });
		Zone.Value().Get()->SetExitHandler(
		    [this](typename T::FBodyId, Dxf::EWorldEventEndReason Reason)
		    {
			    RequireInteraction(Reason == Dxf::EWorldEventEndReason::Separated, "consumer trigger exit reason");
			    ++m_Result.ZoneExits;
		    });
		return {};
	}
	void OnFixedTick(const Dxf::FFixedTickContext&) override
	{
		// 直前に完了した固定更新の値を読み、次に消費する要求だけを設定する。
		auto* Character = m_Character.Get();
		auto* Floor = m_Floor.Get();
		RequireInteraction(Character != nullptr && Floor != nullptr && Floor->Mover.Get() != nullptr,
		                   "consumer component lifetime");
		const Toolbox::int64 Steps = Character->GetStepCount();
		if (Steps >= 2 && Steps <= 60)
		{
			// 1回目は静止、以後は毎回2m/s×1/60秒。一段遅れ・二重移動を許さない。
			const Toolbox::f64 ExpectedX = 2 * (Steps - 1) * StepSeconds;
			RequireInteraction(Toolbox::Abs(Character->GetCenter().X - ExpectedX) < 2e-3 &&
			                       Toolbox::Abs(Floor->Mover.Get()->GetPose().Position.X - ExpectedX) < 2e-3,
			                   "consumer moving floor displacement");
			RequireInteraction(Character->IsGrounded() && Character->GetLastStep().bCarried &&
			                       !Character->GetLastStep().bCarryBlocked,
			                   "consumer moving floor support");
		}
		if (Steps == 60)
		{
			m_Result.CarryX = Character->GetCenter().X;
			Character->RequestJump();
		}
		if (Steps == 61)
		{
			RequireInteraction(Character->GetLastStep().bJumped && Character->GetLastStep().bInheritedGroundVelocity &&
			                       Toolbox::Abs(Character->GetVelocity().X - 2) < 2e-3,
			                   "consumer jump inheritance");
			m_Result.bJumped = true;
		}
		if (Steps == 62)
		{
			RequireInteraction(!Character->GetLastStep().bInheritedGroundVelocity && !Character->GetLastStep().bCarried,
			                   "consumer repeated inheritance");
			Character->Teleport(T::Point(0, 5));
		}
		if (Steps == 65)
		{
			RequireInteraction(m_Result.Pickups == 1 && m_Result.LatePickupCalls == 0,
			                   "consumer destroy during delivery");
			RequireInteraction(m_Result.Begins == 3 && m_Result.Stays > 0 && m_Result.RemovedEnds == 2 &&
			                       m_Result.ZoneExits == 1 && m_Result.bJumped,
			                   "consumer trigger lifecycle");
			RequireInteraction(!Character->GetLastStep().Carrier && !Character->IsGrounded(),
			                   "consumer teleport support reset");
			m_Result.bComplete = true;
		}
	}
	void OnDraw(Dxf::FRenderContext& Render) const override
	{
		// CPU側の検証では呼ばれず、Native Applicationの正規描画から呼ばれる。
		const auto* Character = m_Character.Get();
		const auto* Floor = m_Floor.Get();
		if (Character != nullptr && Floor != nullptr && Floor->Mover.Get() != nullptr)
		{
			T::Draw(Render, Character->GetRenderCenter(), Floor->Mover.Get()->GetRenderPosition());
		}
	}

private:
	// 外部へ保存するゲーム側の観測値。
	FInteractionConsumerResult& m_Result;
	// Sceneが所有する床。破棄済みの参照を保持しない。
	Dxf::TObjectHandle<TConsumerFloor<T>> m_Floor;
	// 自身が所有する移動Component。
	Dxf::TObjectHandle<typename T::FCharacter> m_Character;
};

// シーンは生成と接続だけを担当し、Physicsの更新順序を上書きしない。
template <typename T> class TConsumerScene final : public T::FScene
{
public:
	explicit TConsumerScene(FInteractionConsumerResult& Result) : m_Result(Result)
	{
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		Dxf::FWorldEventSettings Settings;
		Settings.bEnabled = true;
		this->GetPhysicsWorld().SetEventSettings(Settings);
		auto Floor = this->template Spawn<TConsumerFloor<T>>();
		if (!Floor || !this->template Spawn<TConsumerRider<T>>(m_Result, Floor.Value()) ||
		    !this->template Spawn<TConsumerPickup<T>>(m_Result))
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidState, "consumer interaction scene");
		}
		return {};
	}

private:
	// Sceneの外へ検証値を残す借用参照。
	FInteractionConsumerResult& m_Result;
};
} // namespace

Toolbox::TUniquePtr<Dxf::DScene> MakeInteractionConsumer2D(FInteractionConsumerResult& Result)
{
	return Toolbox::MakeUnique<TConsumerScene<FInteraction2D>>(Result);
}

Toolbox::TUniquePtr<Dxf::DScene> MakeInteractionConsumer3D(FInteractionConsumerResult& Result)
{
	return Toolbox::MakeUnique<TConsumerScene<FInteraction3D>>(Result);
}

Toolbox::int32 RunInteractionConsumer(Dxf::FAssetService& Assets, Dxf::FAudioPlayer& Audio)
{
	// Sceneへ渡す値の寿命はScene終了まで保つ。2DのScene終了後に3Dを作る。
	for (Toolbox::int32 Dimension = 0; Dimension < 2; ++Dimension)
	{
		FInteractionConsumerResult Result;
		auto Scene = Dimension == 0 ? MakeInteractionConsumer2D(Result) : MakeInteractionConsumer3D(Result);
		Dxf::FSceneNavigator Navigator(Assets, Audio);
		Dxf::FInputStateTracker Input;
		Dxf::FFrameTime Time;
		Time.DeltaSeconds = StepSeconds;
		Time.UnscaledDeltaSeconds = StepSeconds;
		if (!Navigator.RequestChange(Toolbox::Move(Scene)) || !Navigator.Commit())
		{
			return 401 + Dimension * 10;
		}
		for (Toolbox::int32 Frame = 0; Frame < 70; ++Frame)
		{
			if (!Navigator.CommitObjects() || !Navigator.Tick(Time, Input.GetSnapshot()))
			{
				Navigator.Shutdown();
				return 402 + Dimension * 10;
			}
		}
		Navigator.Shutdown();
		if (!Result.bComplete)
		{
			return 403 + Dimension * 10;
		}
	}
	return 0;
}
