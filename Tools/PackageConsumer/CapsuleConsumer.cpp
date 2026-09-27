// SPDX-License-Identifier: NOASSERTION
#include "CapsuleConsumer.h"
#include "Dxf/CharacterMovementComponent2D.h"
#include "Dxf/CharacterMovementComponent3D.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/RigidBodyComponent2D.h"
#include "Dxf/RigidBodyComponent3D.h"
#include "Dxf/SceneNavigator.h"

namespace
{
// 固定更新の刻み。
constexpr Toolbox::f64 StepSeconds = 1.0 / 60.0;

// 検証の結果。Sceneの外へ残す。
struct FCapsuleConsumerResult
{
	// 全ての確認を終えたか。
	bool bComplete = false;
	// 失敗した確認の説明（成功ならnullptr）。
	const char* Failure = nullptr;
};

// 2Dの公開Componentと形状。
struct FCapsule2D
{
	using FScene = Dxf::DPhysicsScene2D;
	using FCharacter = Dxf::DCharacterMovement2DComponent;
	using FCharacterDescription = Dxf::FCharacterMovementDescription2D;
	using FRigid = Dxf::DRigidBody2DComponent;
	using FCollider = Dxf::DCollider2DComponent;
	using FBodyDescription = Dxf::FBodyDescription2D;
	using FColliderDescription = Dxf::FColliderDescription2D;
	using FVector = Toolbox::FVector2;
	static FVector Point(Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {X, Y};
	}
	static FColliderDescription Box(Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOrientedBox2D{{}, {HalfX, HalfY}, 0};
		return Description;
	}
};
// 3Dの公開Componentと形状。
struct FCapsule3D
{
	using FScene = Dxf::DPhysicsScene3D;
	using FCharacter = Dxf::DCharacterMovement3DComponent;
	using FCharacterDescription = Dxf::FCharacterMovementDescription3D;
	using FRigid = Dxf::DRigidBody3DComponent;
	using FCollider = Dxf::DCollider3DComponent;
	using FBodyDescription = Dxf::FBodyDescription3D;
	using FColliderDescription = Dxf::FColliderDescription3D;
	using FVector = Toolbox::FVector3;
	static FVector Point(Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {X, Y, 0};
	}
	static FColliderDescription Box(Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOBB{{}, {HalfX, HalfY, HalfX}};
		return Description;
	}
};

// 剛体と箱のColliderを一つ持つオブジェクト（床・天井・押す箱）。
template <typename T> class TConsumerBox final : public Dxf::DGameObject
{
public:
	TConsumerBox(Dxf::EBodyType Type, typename T::FVector Center, Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	    : m_Type(Type), m_Center(Center), m_HalfX(HalfX), m_HalfY(HalfY)
	{
	}
	// 剛体（最初の固定更新でBodyを登録する）。
	const typename T::FRigid* Rigid() const noexcept
	{
		return m_Rigid.Get();
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		typename T::FBodyDescription Body;
		Body.Type = m_Type;
		Body.Position = m_Center;
		auto Rigid = this->template AddComponent<typename T::FRigid>(Body);
		auto Collider = this->template AddComponent<typename T::FCollider>(T::Box(m_HalfX, m_HalfY));
		if (!Rigid || !Collider)
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidState, "consumer capsule box");
		}
		m_Rigid = Rigid.Value();
		return {};
	}

private:
	// 運動区分。
	Dxf::EBodyType m_Type;
	// 中心。
	typename T::FVector m_Center;
	// 半幅。
	Toolbox::f32 m_HalfX;
	Toolbox::f32 m_HalfY;
	// 剛体。
	Dxf::TObjectHandle<typename T::FRigid> m_Rigid;
};

// カプセルのキャラクター。右へ歩いて箱を押し、途中で高さを変える。
template <typename T> class TConsumerCapsule final : public Dxf::DGameObject
{
public:
	TConsumerCapsule(FCapsuleConsumerResult& Result, Dxf::TObjectHandle<TConsumerBox<T>> Crate)
	    : m_Result(Result), m_Crate(Crate)
	{
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		typename T::FCharacterDescription Description;
		Description.Center = T::Point(0, 0.92f);
		Description.Settings.Shape = Dxf::ECharacterShape::Capsule;
		Description.Settings.HalfHeight = 0.4;
		Description.Settings.bPushDynamicBodies = true;
		auto Character = this->template AddComponent<typename T::FCharacter>(Description);
		if (!Character)
		{
			return Dxf::TResult<void>::Failure(Character.Error());
		}
		m_Character = Character.Value();
		m_Character.Get()->SetMoveInput(T::Point(1, 0));
		return {};
	}
	void OnFixedTick(const Dxf::FFixedTickContext&) override
	{
		auto* Character = m_Character.Get();
		const auto* Crate = m_Crate.Get();
		if (Character == nullptr || Crate == nullptr || Crate->Rigid() == nullptr)
		{
			Fail_Internal("consumer capsule lifetime");
			return;
		}
		const Toolbox::int64 Steps = Character->GetStepCount();
		if (Steps == 90)
		{
			// 押した箱（初期x=2）は進み、キャラクターは床の上の高さ（0.92）を保つ。
			const auto Box = Crate->Rigid()->GetRenderPosition();
			if (Box.X < 3 || Toolbox::Abs(Character->GetCenter().Y - 0.92) > 1e-3 || !Character->IsGrounded())
			{
				Fail_Internal("consumer capsule push");
				return;
			}
			// 開けた場所では縮めて伸ばせ、足元を保つ。
			Character->SetMoveInput(T::Point(0, 0));
			if (!Character->TrySetCapsuleHalfHeight(0.1) || Toolbox::Abs(Character->GetCenter().Y - 0.62) > 1e-4 ||
			    !Character->TrySetCapsuleHalfHeight(0.4) || Toolbox::Abs(Character->GetCenter().Y - 0.92) > 1e-4)
			{
				Fail_Internal("consumer capsule open resize");
				return;
			}
			// 低い天井（下面y=1.5）の下へしゃがんで移し、立ち上がれないことを確かめる。
			if (!Character->TrySetCapsuleHalfHeight(0.1))
			{
				Fail_Internal("consumer capsule crouch");
				return;
			}
			Character->Teleport(T::Point(-10, 0.62f));
		}
		if (Steps == 93)
		{
			if (Character->TrySetCapsuleHalfHeight(0.4) || Character->GetSettings().HalfHeight != 0.1 ||
			    Toolbox::Abs(Character->GetCenter().Y - 0.62) > 1e-3)
			{
				Fail_Internal("consumer capsule ceiling");
				return;
			}
			m_Result.bComplete = m_Result.Failure == nullptr;
		}
	}

private:
	// 失敗を記録する（最初の一つ）。
	void Fail_Internal(const char* Message) noexcept
	{
		if (m_Result.Failure == nullptr)
		{
			m_Result.Failure = Message;
		}
	}
	// 検証の結果。
	FCapsuleConsumerResult& m_Result;
	// 押す箱。
	Dxf::TObjectHandle<TConsumerBox<T>> m_Crate;
	// 自身が所有する移動Component。
	Dxf::TObjectHandle<typename T::FCharacter> m_Character;
};

// 床・天井・箱・キャラクターを置くScene。
template <typename T> class TCapsuleScene final : public T::FScene
{
public:
	explicit TCapsuleScene(FCapsuleConsumerResult& Result) : m_Result(Result)
	{
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext&) override
	{
		auto Floor = this->template Spawn<TConsumerBox<T>>(Dxf::EBodyType::Static, T::Point(0, -0.5f), 40.0f, 0.5f);
		auto Ceiling = this->template Spawn<TConsumerBox<T>>(Dxf::EBodyType::Static, T::Point(-10, 2), 1.0f, 0.5f);
		auto Crate = this->template Spawn<TConsumerBox<T>>(Dxf::EBodyType::Dynamic, T::Point(2, 0.5f), 0.5f, 0.5f);
		if (!Floor || !Ceiling || !Crate || !this->template Spawn<TConsumerCapsule<T>>(m_Result, Crate.Value()))
		{
			return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InvalidState, "consumer capsule scene");
		}
		return {};
	}

private:
	// 検証の結果。
	FCapsuleConsumerResult& m_Result;
};
} // namespace

Toolbox::int32 RunCapsuleConsumer(Dxf::FAssetService& Assets, Dxf::FAudioPlayer& Audio)
{
	for (Toolbox::int32 Dimension = 0; Dimension < 2; ++Dimension)
	{
		FCapsuleConsumerResult Result;
		Toolbox::TUniquePtr<Dxf::DScene> Scene;
		if (Dimension == 0)
		{
			Scene = Toolbox::MakeUnique<TCapsuleScene<FCapsule2D>>(Result);
		}
		else
		{
			Scene = Toolbox::MakeUnique<TCapsuleScene<FCapsule3D>>(Result);
		}
		Dxf::FSceneNavigator Navigator(Assets, Audio);
		Dxf::FInputStateTracker Input;
		Dxf::FFrameTime Time;
		Time.DeltaSeconds = StepSeconds;
		Time.UnscaledDeltaSeconds = StepSeconds;
		if (!Navigator.RequestChange(Toolbox::Move(Scene)) || !Navigator.Commit())
		{
			return 501 + Dimension * 10;
		}
		for (Toolbox::int32 Frame = 0; Frame < 100; ++Frame)
		{
			if (!Navigator.CommitObjects() || !Navigator.Tick(Time, Input.GetSnapshot()))
			{
				Navigator.Shutdown();
				return 502 + Dimension * 10;
			}
		}
		Navigator.Shutdown();
		if (!Result.bComplete)
		{
			return 503 + Dimension * 10;
		}
	}
	return 0;
}
