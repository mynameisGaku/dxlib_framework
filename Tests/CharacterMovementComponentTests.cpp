// SPDX-License-Identifier: NOASSERTION
// キャラクター移動Componentを実際のDPhysicsScene2D／3D・GameObject・固定更新で使う。
// 確認: 一つのBodyの登録と位置の決定権、同じ固定更新の登録順に依存しない問い合わせ、ジャンプ要求の一回消費と一時停止、
// Teleport・破棄・登録前の破棄、剛体との同時装着の拒否、複数個体、同じ入力列の再現。
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/CharacterMovementComponent2D.h"
#include "Dxf/CharacterMovementComponent3D.h"
#include "Dxf/GameObject.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/RigidBodyComponent2D.h"
#include "Dxf/RigidBodyComponent3D.h"
#include "Dxf/SceneNavigator.h"
using namespace Dxf;
using namespace Dxf::Testing;
using namespace Toolbox;
namespace
{
// 2Dの型と配置。
struct FCase2D
{
	using FScene = DPhysicsScene2D;
	using FVector = Toolbox::FVector2;
	using FCharacter = DCharacterMovement2DComponent;
	using FDescription = FCharacterMovementDescription2D;
	using FRigidBody = DRigidBody2DComponent;
	using FBodyDescription = FBodyDescription2D;
	using FCollider = DCollider2DComponent;
	using FColliderDescription = FColliderDescription2D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y};
	}
	static f64 X(FVector Value)
	{
		return Value.X;
	}
	static f64 Y(FVector Value)
	{
		return Value.Y;
	}
	static void Box(FColliderDescription& Description, FVector Center, f32 HalfX, f32 HalfY)
	{
		Description.Shape = Toolbox::FOrientedBox2D{Center, {HalfX, HalfY}, 0};
	}
	static auto& World(FScene& Scene)
	{
		return Scene.GetPhysicsWorld();
	}
	static auto Probe(FVector Center, f32 Radius)
	{
		return Toolbox::FCircle2D{Center, Radius};
	}
};
// 3Dの型と配置（Z=0の平面に2Dと同じ配置を作る）。
struct FCase3D
{
	using FScene = DPhysicsScene3D;
	using FVector = Toolbox::FVector3;
	using FCharacter = DCharacterMovement3DComponent;
	using FDescription = FCharacterMovementDescription3D;
	using FRigidBody = DRigidBody3DComponent;
	using FBodyDescription = FBodyDescription3D;
	using FCollider = DCollider3DComponent;
	using FColliderDescription = FColliderDescription3D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y, 0};
	}
	static f64 X(FVector Value)
	{
		return Value.X;
	}
	static f64 Y(FVector Value)
	{
		return Value.Y;
	}
	static void Box(FColliderDescription& Description, FVector Center, f32 HalfX, f32 HalfY)
	{
		Description.Shape = Toolbox::FOBB{Center, {HalfX, HalfY, 50}};
	}
	static auto& World(FScene& Scene)
	{
		return Scene.GetPhysicsWorld();
	}
	static auto Probe(FVector Center, f32 Radius)
	{
		return Toolbox::FSphere{Center, Radius};
	}
};

// 静止した箱（床・壁）。
template <typename T> class TBoxObject : public DGameObject
{
public:
	// 箱の中心と半幅を受け取る。
	TBoxObject(typename T::FVector Center, f32 HalfX, f32 HalfY) : m_Center(Center), m_HalfX(HalfX), m_HalfY(HalfY)
	{
	}

protected:
	// 静止した剛体と箱のコライダーを追加する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		typename T::FBodyDescription Body;
		Body.Type = EBodyType::Static;
		auto Rigid = AddComponent<typename T::FRigidBody>(Body);
		if (!Rigid)
		{
			return TResult<void>::Failure(Rigid.Error());
		}
		typename T::FColliderDescription Collider;
		T::Box(Collider, m_Center, m_HalfX, m_HalfY);
		auto Attached = AddComponent<typename T::FCollider>(Collider);
		if (!Attached)
		{
			return TResult<void>::Failure(Attached.Error());
		}
		return {};
	}

private:
	// 箱の中心。
	typename T::FVector m_Center;
	// X方向の半幅。
	f32 m_HalfX;
	// Y方向の半幅。
	f32 m_HalfY;
};

// キャラクター（必要なら同じオブジェクトに剛体も付ける）。
template <typename T> class TCharacterObject : public DGameObject
{
public:
	// 初期条件と、剛体を同時に付けるかを受け取る。
	explicit TCharacterObject(typename T::FDescription Description, bool bWithRigidBody = false)
	    : m_Description(Description), m_bWithRigidBody(bWithRigidBody)
	{
	}
	// 移動Componentを返す。
	typename T::FCharacter& Character()
	{
		auto Found = FindComponent<typename T::FCharacter>();
		REQUIRE(Found.Get() != nullptr);
		return *Found.Get();
	}

protected:
	// 移動Component（と、指定があれば剛体）を追加する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		if (m_bWithRigidBody)
		{
			auto Rigid = AddComponent<typename T::FRigidBody>(typename T::FBodyDescription{});
			if (!Rigid)
			{
				return TResult<void>::Failure(Rigid.Error());
			}
		}
		auto Added = AddComponent<typename T::FCharacter>(m_Description);
		if (!Added)
		{
			return TResult<void>::Failure(Added.Error());
		}
		return {};
	}

private:
	// 初期条件。
	typename T::FDescription m_Description;
	// 剛体を同時に付けるか。
	bool m_bWithRigidBody;
};

// シーンの実行環境。
struct FFixture
{
	// 検証対象が参照するバックエンド。
	FFakeBackend Backend;
	// 検証用の資源管理。
	FAssetService Assets{Backend, Backend, Backend};
	// 検証用の音声サービス。
	FAudioPlayer Audio{Backend};
	// 検証用のシーン遷移管理。
	FSceneNavigator Navigator{Assets, Audio};
	// 検証用の入力履歴。
	FInputStateTracker Tracker;
	// 初期化の実行環境。
	FInitContext Init()
	{
		return {Assets};
	}
	// 更新の実行環境。
	FTickContext Tick(f64 DeltaSeconds, bool bPaused = false)
	{
		FFrameTime Time;
		Time.DeltaSeconds = DeltaSeconds;
		Time.UnscaledDeltaSeconds = DeltaSeconds;
		Time.bPaused = bPaused;
		return {Tracker.GetSnapshot(), Time};
	}
};

// フレーム境界（生成の初期化・破棄の確定）をSceneNavigatorと同じ手順で行う。
template <typename TScene> void Boundary_Internal(FFixture& Fixture, TScene& Scene)
{
	auto* Children = Scene.GetChildren_Internal();
	REQUIRE(Children != nullptr);
	Children->FreezeBoundary_Internal();
	REQUIRE(Children->CommitBoundary_Internal(Fixture.Init()));
}
template <typename T> typename T::FDescription Description_Internal(typename T::FVector Center)
{
	typename T::FDescription Description;
	Description.Center = Center;
	return Description;
}
// 上面y=0の床を持つシーンに、キャラクターを一つ置いて初期化する。
template <typename T>
TCharacterObject<T>& PrepareFloor_Internal(FFixture& Fixture, typename T::FScene& Scene,
                                           typename T::FDescription Description)
{
	auto Character = Scene.template Spawn<TCharacterObject<T>>(Description);
	REQUIRE(Character);
	REQUIRE(Scene.template Spawn<TBoxObject<T>>(T::At(0, -1), 50.0f, 1.0f));
	REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
	return *Character.Value().Get();
}

// 歩行: 60回の固定更新で解析値の位置へ進み、接地を保つ。Bodyは一つで、位置はComponentが決める。
template <typename T> void Walk_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	auto& Object = PrepareFloor_Internal<T>(Fixture, Scene, Description_Internal<T>(T::At(0, 0.52f)));
	auto& Character = Object.Character();
	Character.SetMoveInput(T::At(1));
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
	}
	REQUIRE(Character.GetStepCount() == 60);
	REQUIRE(Abs(T::X(Character.GetCenter()) - (40.0 / 60.0 * 28 + 53 * 5) / 60) < 1e-4);
	REQUIRE(Abs(T::Y(Character.GetCenter()) - 0.52) < 1e-6);
	REQUIRE(Character.IsGrounded());
	const auto Body = Character.GetBodyId();
	REQUIRE(Body);
	auto& World = T::World(Scene);
	REQUIRE(World.IsAlive(*Body) && World.GetPosition(*Body) == Character.GetCenter());
	// 自分の位置で重なるColliderは、自分の一つだけ（別の衝突Worldへの二重登録はない）。
	const auto Found = World.OverlapAll(T::Probe(Character.GetCenter(), 0.1f));
	REQUIRE(Found.Size() == 1 && Found[0].Body == *Body);
	// 描画用の中心は、直前と直近の固定更新の間にある。
	const auto Render = Character.GetRenderCenter();
	REQUIRE(T::X(Render) <= T::X(Character.GetCenter()) && T::X(Render) >= T::X(Character.GetCenter()) - 0.1);
	Scene.Shutdown_Internal();
}

// 登録順: キャラクターが先に固定更新を受けても、同じ固定更新で後から登録された壁を見落とさない。
template <typename T> void SameStepRegistration_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	auto Description = Description_Internal<T>(T::At(0, 0.52f));
	// 1回の固定更新で10進む速さ（登録されていなければ壁x∈[5,7]を越える）。
	Description.Settings.MaxSpeed = 600;
	Description.Settings.Acceleration = 1e6;
	auto Character = Scene.template Spawn<TCharacterObject<T>>(Description);
	REQUIRE(Character);
	REQUIRE(Scene.template Spawn<TBoxObject<T>>(T::At(0, -1), 50.0f, 1.0f));
	REQUIRE(Scene.template Spawn<TBoxObject<T>>(T::At(6, 5), 1.0f, 5.0f));
	REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
	Character.Value().Get()->Character().SetMoveInput(T::At(1));
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
	const auto Center = Character.Value().Get()->Character().GetCenter();
	REQUIRE(T::X(Center) <= 4.48 + 1e-6 && T::X(Center) > 4.47);
	Scene.Shutdown_Internal();
}

// ジャンプ要求: 固定更新0回のフレームを跨いでも失わず、同じフレームの2回目の固定更新では繰り返さない。
// 空中の要求では跳ばず、一時停止中の要求は破棄する。
template <typename T> void JumpRequests_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	auto& Character = PrepareFloor_Internal<T>(Fixture, Scene, Description_Internal<T>(T::At(0, 0.52f))).Character();
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Character.IsGrounded());
	Character.RequestJump();
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 240.0)));
	REQUIRE(Character.GetStepCount() == 1);
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 30.0)));
	REQUIRE(Character.GetStepCount() == 3);
	// 1回目で初速6、2回目は重力で6-20/60（2回目にもう一度跳べば6になる）。
	REQUIRE(Abs(T::Y(Character.GetVelocity()) - (6 - 20.0 / 60.0)) < 1e-4);
	REQUIRE(!Character.GetLastStep().bJumped && !Character.IsGrounded());
	int32 Jumps = 0;
	bool bLanded = false;
	for (int32 Frame = 0; Frame < 60 && !bLanded; ++Frame)
	{
		Character.RequestJump();
		REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
		Jumps += Character.GetLastStep().bJumped ? 1 : 0;
		bLanded = Character.GetLastStep().bLanded;
	}
	REQUIRE(bLanded && Jumps == 0);
	// 空中の要求は、跳ばずにその固定更新で消費される（着地後へ持ち越さない）。新しい要求がなければ跳ばない。
	REQUIRE(Character.IsGrounded());
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
		REQUIRE(!Character.GetLastStep().bJumped && Character.IsGrounded());
	}
	const auto Before = Character.GetStepCount();
	Character.RequestJump();
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0, true)));
	REQUIRE(Character.GetStepCount() == Before);
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Character.GetStepCount() == Before + 1 && !Character.GetLastStep().bJumped && Character.IsGrounded());
	Scene.Shutdown_Internal();
}

// Teleport・破棄・登録前の破棄。
template <typename T> void Lifetime_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	auto& Object = PrepareFloor_Internal<T>(Fixture, Scene, Description_Internal<T>(T::At(0, 0.52f)));
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
	auto& Character = Object.Character();
	const auto Body = Character.GetBodyId();
	REQUIRE(Body);
	Character.Teleport(T::At(3, 0.52f));
	REQUIRE(Character.GetRenderCenter() == T::At(3, 0.52f) && Character.GetVelocity() == T::At(0));
	REQUIRE(T::World(Scene).GetPosition(*Body) == T::At(3, 0.52f));
	Object.Destroy();
	// 破棄を要求した固定更新以降、移動は行わない。境界でBodyを解放する。
	const auto Steps = Character.GetStepCount();
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Character.GetStepCount() == Steps);
	Boundary_Internal(Fixture, Scene);
	REQUIRE(!T::World(Scene).IsAlive(*Body));
	// 固定更新の前に破棄されたキャラクターは、Bodyを作らず、解放も誤らない。
	auto Early = Scene.template Spawn<TCharacterObject<T>>(Description_Internal<T>(T::At(-5, 0.52f)));
	REQUIRE(Early);
	Early.Value().Get()->Destroy();
	Boundary_Internal(Fixture, Scene);
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
	Boundary_Internal(Fixture, Scene);
	Scene.Shutdown_Internal();
}

// 同じオブジェクトの剛体とは位置の決定権が重なるので拒否する。
template <typename T> void RejectsRigidBody_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	REQUIRE(Scene.template Spawn<TCharacterObject<T>>(Description_Internal<T>(T::At(0, 0.52f)), true));
	REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
	REQUIRE(!Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
	Scene.Shutdown_Internal();
}

// 複数個体: 入力・状態は個体ごとで、互いのBodyを障害物として見る（自分のBodyだけを除く）。
template <typename T> void Multiple_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	auto Left = Scene.template Spawn<TCharacterObject<T>>(Description_Internal<T>(T::At(-3, 0.52f)));
	auto Right = Scene.template Spawn<TCharacterObject<T>>(Description_Internal<T>(T::At(3, 0.52f)));
	REQUIRE(Left && Right);
	REQUIRE(Scene.template Spawn<TBoxObject<T>>(T::At(0, -1), 50.0f, 1.0f));
	REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
	auto& A = Left.Value().Get()->Character();
	auto& B = Right.Value().Get()->Character();
	A.SetMoveInput(T::At(1));
	B.SetMoveInput(T::At(-1));
	B.RequestJump();
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
	}
	// 向かい合って進み、重ならずに止まる（中心の距離は直径以上）。
	REQUIRE(T::X(B.GetCenter()) - T::X(A.GetCenter()) >= 1.0 - 1e-6);
	REQUIRE(A.GetStepCount() == 120 && B.GetStepCount() == 120);
	Scene.Shutdown_Internal();
}

// 同じ固定入力列なら、別のシーンでも、フレーム時間の分け方（1/60と1/120）が違っても同じ経過になる。
template <typename T> void Replay_Internal()
{
	const auto Run = [](f64 FrameSeconds)
	{
		FFixture Fixture;
		typename T::FScene Scene;
		auto& Object = PrepareFloor_Internal<T>(Fixture, Scene, Description_Internal<T>(T::At(0, 0.52f)));
		REQUIRE(Scene.template Spawn<TBoxObject<T>>(T::At(24, 0.1f), 20.0f, 0.1f));
		Boundary_Internal(Fixture, Scene);
		auto& Character = Object.Character();
		int64 Previous = -1;
		for (int32 Frame = 0; Frame < 1000 && Character.GetStepCount() < 200; ++Frame)
		{
			// 固定更新の回数ごとに入力を決める（描画の頻度に依存しない入力列）。
			if (Character.GetStepCount() != Previous)
			{
				Previous = Character.GetStepCount();
				Character.SetMoveInput(T::At(Previous % 40 < 30 ? 1.0f : -0.5f));
				if (Previous % 50 == 10)
				{
					Character.RequestJump();
				}
			}
			REQUIRE(Scene.Tick_Internal(Fixture.Tick(FrameSeconds)));
		}
		REQUIRE(Character.GetStepCount() == 200);
		const auto Center = Character.GetCenter();
		Scene.Shutdown_Internal();
		return Center;
	};
	const auto First = Run(1.0 / 60.0);
	const auto Second = Run(1.0 / 60.0);
	const auto Split = Run(1.0 / 120.0);
	REQUIRE(First == Second && First == Split && T::X(First) > 0);
}
} // namespace
TEST("2D character component walks grounded with one registered body")
{
	Walk_Internal<FCase2D>();
}
TEST("3D character component walks grounded with one registered body")
{
	Walk_Internal<FCase3D>();
}
TEST("2D character component sees colliders registered in the same fixed step")
{
	SameStepRegistration_Internal<FCase2D>();
}
TEST("3D character component sees colliders registered in the same fixed step")
{
	SameStepRegistration_Internal<FCase3D>();
}
TEST("2D character component consumes a jump request once and discards it while paused")
{
	JumpRequests_Internal<FCase2D>();
}
TEST("3D character component consumes a jump request once and discards it while paused")
{
	JumpRequests_Internal<FCase3D>();
}
TEST("2D character component teleports, releases its body and handles early destruction")
{
	Lifetime_Internal<FCase2D>();
}
TEST("3D character component teleports, releases its body and handles early destruction")
{
	Lifetime_Internal<FCase3D>();
}
TEST("2D character component rejects a rigid body on the same object")
{
	RejectsRigidBody_Internal<FCase2D>();
}
TEST("3D character component rejects a rigid body on the same object")
{
	RejectsRigidBody_Internal<FCase3D>();
}
TEST("2D character components keep separate state and block each other")
{
	Multiple_Internal<FCase2D>();
}
TEST("3D character components keep separate state and block each other")
{
	Multiple_Internal<FCase3D>();
}
TEST("2D character component replays the same fixed input sequence")
{
	Replay_Internal<FCase2D>();
}
TEST("3D character component replays the same fixed input sequence")
{
	Replay_Internal<FCase3D>();
}
