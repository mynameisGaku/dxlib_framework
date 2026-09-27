// SPDX-License-Identifier: NOASSERTION
// キャラクター（2D円／3D球）の動く床への追従（R5）を、実DPhysicsScene2D／3D・キャラクター移動Component・Kinematicの物体で確かめる。
// 期待値は床の経路の解析値（並進＝速さ×時刻、回転＝既知の中心からの剛体変換）と、床の点の速度から独立に求める。
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/CharacterMovementComponent2D.h"
#include "Dxf/CharacterMovementComponent3D.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/KinematicMoverComponent2D.h"
#include "Dxf/KinematicMoverComponent3D.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/SceneNavigator.h"
using namespace Dxf;
using namespace Dxf::Testing;
using namespace Toolbox;
namespace
{
constexpr f64 StepSeconds = 1.0 / 60.0;
// 床の上面は0.25、キャラクターの半径0.5と接触余裕0.02で、中心の高さは0.77。
constexpr f32 StandY = 0.77f;

struct FCase2D
{
	using FScene = DPhysicsScene2D;
	using FVector = Toolbox::FVector2;
	using FMover = DKinematicMover2DComponent;
	using FMoverDescription = FKinematicMoverDescription2D;
	using FPose = FKinematicPose2D;
	using FCharacter = DCharacterMovement2DComponent;
	using FDescription = FCharacterMovementDescription2D;
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
	static FColliderDescription Box(f32 HalfX, f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOrientedBox2D{{0, 0}, {HalfX, HalfY}, 0};
		return Description;
	}
	// 2Dの回転する床は、歩ける範囲で傾く（振幅0.3ラジアン）。3Dは鉛直軸回りに回る。
	static FPose Turned(f64 Seconds)
	{
		FPose Pose;
		Pose.Rotation = static_cast<f32>(0.3 * Sin(Seconds));
		return Pose;
	}
	// 床に固定した点（開始時の床の座標）の、時刻の床の姿勢での位置。
	static FVector Rigid(FVector Local, const FPose& Pose)
	{
		const f64 C = Cos(Pose.Rotation);
		const f64 S = Sin(Pose.Rotation);
		return {static_cast<f32>(Pose.Position.X + C * Local.X - S * Local.Y),
		        static_cast<f32>(Pose.Position.Y + S * Local.X + C * Local.Y)};
	}
};
struct FCase3D
{
	using FScene = DPhysicsScene3D;
	using FVector = Toolbox::FVector3;
	using FMover = DKinematicMover3DComponent;
	using FMoverDescription = FKinematicMoverDescription3D;
	using FPose = FKinematicPose3D;
	using FCharacter = DCharacterMovement3DComponent;
	using FDescription = FCharacterMovementDescription3D;
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
	static FColliderDescription Box(f32 HalfX, f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOBB{{0, 0, 0}, {HalfX, HalfY, HalfX}};
		return Description;
	}
	static FPose Turned(f64 Seconds)
	{
		FPose Pose;
		Pose.Rotation = Toolbox::FQuaternion::FromAxisAngle({0, 1, 0}, static_cast<f32>(0.8 * Seconds));
		return Pose;
	}
	static FVector Rigid(FVector Local, const FPose& Pose)
	{
		return Pose.Position + Pose.Rotation.Rotate(Local);
	}
};

struct FFixture
{
	FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FAudioPlayer Audio{Backend};
	FSceneNavigator Navigator{Assets, Audio};
	FInputStateTracker Tracker;
	FInitContext Init()
	{
		return {Assets};
	}
	FTickContext Tick(f64 DeltaSeconds)
	{
		FFrameTime Time;
		Time.DeltaSeconds = DeltaSeconds;
		Time.UnscaledDeltaSeconds = DeltaSeconds;
		return {Tracker.GetSnapshot(), Time};
	}
};

// 一つのComponentを持つGameObject。
template <typename TComponent, typename TDescription> class TSingle : public DGameObject
{
public:
	explicit TSingle(TDescription Description) : m_Description(Toolbox::Move(Description))
	{
	}
	TComponent& Get()
	{
		auto Found = FindComponent<TComponent>();
		REQUIRE(Found.Get() != nullptr);
		return *Found.Get();
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		auto Added = AddComponent<TComponent>(m_Description);
		if (!Added)
		{
			return TResult<void>::Failure(Added.Error());
		}
		return {};
	}

private:
	TDescription m_Description;
};

// 床（半幅4×0.25）と、その上に立つキャラクターのシーン。bMoverFirstで生成順を入れ替える。
template <typename T> struct FPlatformScene
{
	FFixture Fixture;
	typename T::FScene Scene;
	typename T::FMover* Mover = nullptr;
	typename T::FCharacter* Character = nullptr;
	explicit FPlatformScene(f32 CharacterX = 0, bool bMoverFirst = true, typename T::FVector Origin = {})
	{
		typename T::FMoverDescription Platform;
		Platform.Colliders.PushBack(T::Box(4, 0.25f));
		Platform.Pose.Position = Origin;
		typename T::FDescription Walker;
		Walker.Center = T::At(CharacterX + static_cast<f32>(T::X(Origin)), StandY + static_cast<f32>(T::Y(Origin)));
		using FPlatformObject = TSingle<typename T::FMover, typename T::FMoverDescription>;
		using FWalkerObject = TSingle<typename T::FCharacter, typename T::FDescription>;
		FPlatformObject* PlatformObject = nullptr;
		if (bMoverFirst)
		{
			PlatformObject = Scene.template Spawn<FPlatformObject>(Platform).Value().Get();
		}
		FWalkerObject* WalkerObject = Scene.template Spawn<FWalkerObject>(Walker).Value().Get();
		if (!bMoverFirst)
		{
			PlatformObject = Scene.template Spawn<FPlatformObject>(Platform).Value().Get();
		}
		REQUIRE(PlatformObject != nullptr && WalkerObject != nullptr);
		REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
		Character = &WalkerObject->Get();
		Mover = &PlatformObject->Get();
		// 床の登録と接地（最初の固定更新）。
		Run(1);
		REQUIRE(Character->IsGrounded());
	}
	void Run(int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE(Scene.Tick_Internal(Fixture.Tick(StepSeconds)));
		}
	}
	~FPlatformScene()
	{
		Scene.Shutdown_Internal();
	}
};

// 横・縦に動く床に乗ったまま運ばれ、接地を保つ。中心は床に固定した点と同じ移動（解析値）。
template <typename T> void Translates_Internal()
{
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		FPlatformScene<T> World;
		World.Mover->SetElapsedSeconds(0);
		World.Mover->SetPath(
		    [Axis](f64 Seconds)
		    {
			    typename T::FPose Pose;
			    // 横は2m/s、縦は上下に1.2mの往復（毎秒約1.8m/s以下）。
			    Pose.Position = Axis == 0 ? T::At(static_cast<f32>(2 * Seconds))
			                              : T::At(0, static_cast<f32>(1.2 * Sin(1.5 * Seconds)));
			    return Pose;
		    });
		const auto Start = World.Character->GetCenter();
		for (int32 Frame = 1; Frame <= 240; ++Frame)
		{
			World.Run(1);
			const f64 Seconds = Frame * StepSeconds;
			const f64 ExpectedX = T::X(Start) + (Axis == 0 ? 2 * Seconds : 0);
			const f64 ExpectedY = T::Y(Start) + (Axis == 0 ? 0 : 1.2 * Sin(1.5 * Seconds));
			REQUIRE(Abs(T::X(World.Character->GetCenter()) - ExpectedX) < 2e-3);
			REQUIRE(Abs(T::Y(World.Character->GetCenter()) - ExpectedY) < 2e-3);
			REQUIRE(World.Character->IsGrounded());
			REQUIRE(World.Character->GetLastStep().bCarried && !World.Character->GetLastStep().bCarryBlocked);
		}
	}
}

// 回転する床（2Dは歩ける範囲の傾き、3Dは鉛直軸回り）。中心から離れた位置の点は床に固定した点と同じ軌跡（剛体変換）。
template <typename T> void Rotates_Internal()
{
	FPlatformScene<T> World(1.5f);
	World.Mover->SetPath(
	    [](f64 Seconds)
	    {
		    return T::Turned(Seconds);
	    });
	const auto Local = World.Character->GetCenter();
	for (int32 Frame = 1; Frame <= 180; ++Frame)
	{
		World.Run(1);
		const auto Expected = T::Rigid(Local, World.Mover->GetPose());
		const auto Actual = World.Character->GetCenter();
		REQUIRE(Abs(T::X(Actual) - T::X(Expected)) < 5e-3 && Abs(T::Y(Actual) - T::Y(Expected)) < 5e-3);
		REQUIRE(World.Character->IsGrounded());
	}
}

// 床と逆向きに同じ速さで歩くと、ワールドではほぼ止まる（床の上の歩行と床の変位を分けて足す）。
template <typename T> void WalksAgainst_Internal()
{
	FPlatformScene<T> World;
	World.Mover->SetPath(
	    [](f64 Seconds)
	    {
		    typename T::FPose Pose;
		    Pose.Position = T::At(static_cast<f32>(3 * Seconds));
		    return Pose;
	    });
	auto Settings = World.Character->GetSettings();
	Settings.MaxSpeed = 3;
	Settings.Acceleration = 1e6;
	World.Character->SetSettings(Settings);
	World.Character->SetMoveInput(T::At(-1));
	World.Run(2);
	const auto Start = World.Character->GetCenter();
	World.Run(60);
	REQUIRE(Abs(T::X(World.Character->GetCenter()) - T::X(Start)) < 1e-3);
	// 床の上での移動は3m/s×1秒（床の座標では端へ近づく）。
	REQUIRE(Abs(T::X(World.Mover->GetPose().Position) - 3 * 63 * StepSeconds) < 1e-3);
}

// ジャンプで床を離れると床の点の速度を一度だけ受け継ぎ、空中では加え続けない。
template <typename T> void JumpInherits_Internal()
{
	FPlatformScene<T> World;
	World.Mover->SetPath(
	    [](f64 Seconds)
	    {
		    typename T::FPose Pose;
		    Pose.Position = T::At(static_cast<f32>(4 * Seconds));
		    return Pose;
	    });
	World.Run(30);
	World.Character->RequestJump();
	World.Run(1);
	const auto& Jump = World.Character->GetLastStep();
	REQUIRE(Jump.bJumped && Jump.bInheritedGroundVelocity);
	REQUIRE(Abs(T::X(World.Character->GetVelocity()) - 4) < 1e-3);
	// 空中は入力なしの減速（空中の操作の倍率）で遅くなるだけで、床の速度を再び加えない。
	f64 Previous = T::X(World.Character->GetVelocity());
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		World.Run(1);
		REQUIRE(!World.Character->GetLastStep().bCarried && !World.Character->GetLastStep().bInheritedGroundVelocity);
		const f64 Speed = T::X(World.Character->GetVelocity());
		REQUIRE(Speed < Previous + 1e-9);
		Previous = Speed;
	}
	// 継承を無効にすると空中の速さは床の速さを受け継がない。
	FPlatformScene<T> Plain;
	auto Settings = Plain.Character->GetSettings();
	Settings.bInheritGroundVelocity = false;
	Plain.Character->SetSettings(Settings);
	Plain.Mover->SetPath(
	    [](f64 Seconds)
	    {
		    typename T::FPose Pose;
		    Pose.Position = T::At(static_cast<f32>(4 * Seconds));
		    return Pose;
	    });
	Plain.Run(30);
	Plain.Character->RequestJump();
	Plain.Run(1);
	REQUIRE(Abs(T::X(Plain.Character->GetVelocity())) < 1e-6);
}

// 静止した箱をWorldへ直接置く（天井・壁）。
template <typename T>
void StaticBox_Internal(typename T::FScene& Scene, typename T::FVector Center, f32 HalfX, f32 HalfY)
{
	auto& Physics = Scene.GetPhysicsWorld();
	if constexpr (sizeof(typename T::FVector) == sizeof(Toolbox::FVector2))
	{
		FBodyDescription2D Body;
		Body.Type = EBodyType::Static;
		Body.Position = Center;
		(void)Physics.AttachCollider(Physics.CreateBody(Body), T::Box(HalfX, HalfY));
	}
	else
	{
		FBodyDescription3D Body;
		Body.Type = EBodyType::Static;
		Body.Position = Center;
		(void)Physics.AttachCollider(Physics.CreateBody(Body), T::Box(HalfX, HalfY));
	}
}

// 上昇する床と低い天井に挟まれると、追従は止められ（量と理由を残す）、天井を通過せず、値は有限。
template <typename T> void CrushedAgainstCeiling_Internal()
{
	FPlatformScene<T> World;
	// 天井の下面はy=2.5。床は1.6まで上がって止まる（キャラクターの入る隙間はない）。
	StaticBox_Internal<T>(World.Scene, T::At(0, 3), 6, 0.5f);
	World.Mover->SetPath(
	    [](f64 Seconds)
	    {
		    typename T::FPose Pose;
		    Pose.Position = T::At(0, static_cast<f32>(Min(1.6, 1.5 * Seconds)));
		    return Pose;
	    });
	bool bBlocked = false;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		World.Run(1);
		const auto Center = World.Character->GetCenter();
		REQUIRE(Center.IsValid());
		REQUIRE(T::Y(Center) + 0.5 <= 2.5 + 1e-3);
		bBlocked = bBlocked || World.Character->GetLastStep().bCarryBlocked;
	}
	REQUIRE(bBlocked);
}

// 横に動く床が、床の上だけを塞ぐ壁へ向かうと、キャラクターは壁で止められ（通過しない）、床は下を通り抜ける。
template <typename T> void PushedIntoWall_Internal()
{
	FPlatformScene<T> World;
	// 壁の左面はx=2.5、下面はy=0.5（床の上面0.25より上）。
	StaticBox_Internal<T>(World.Scene, T::At(3, 3), 0.5f, 2.5f);
	World.Mover->SetPath(
	    [](f64 Seconds)
	    {
		    typename T::FPose Pose;
		    Pose.Position = T::At(static_cast<f32>(2 * Seconds));
		    return Pose;
	    });
	bool bBlocked = false;
	for (int32 Frame = 0; Frame < 150; ++Frame)
	{
		World.Run(1);
		const auto Center = World.Character->GetCenter();
		REQUIRE(Center.IsValid() && T::X(Center) + 0.5 <= 2.5 + 1e-3);
		bBlocked = bBlocked || World.Character->GetLastStep().bCarryBlocked;
	}
	REQUIRE(bBlocked);
	REQUIRE(T::X(World.Mover->GetPose().Position) > 4.9);
}

// 床の生成順（キャラクターの前／後）で、キャラクターの軌跡はビット単位で同じ。
template <typename T> void OrderIndependent_Internal()
{
	Toolbox::TVector<typename T::FVector> Paths[2];
	for (int32 Order = 0; Order < 2; ++Order)
	{
		FPlatformScene<T> World(0, Order == 0);
		World.Mover->SetPath(
		    [](f64 Seconds)
		    {
			    auto Pose = T::Turned(Seconds);
			    Pose.Position = T::At(static_cast<f32>(Seconds), static_cast<f32>(0.5 * Sin(Seconds)));
			    return Pose;
		    });
		World.Character->SetMoveInput(T::At(0.5f));
		for (int32 Frame = 0; Frame < 120; ++Frame)
		{
			World.Run(1);
			Paths[Order].PushBack(World.Character->GetCenter());
		}
	}
	for (Toolbox::size_t Index = 0; Index < Paths[0].Size(); ++Index)
	{
		REQUIRE(Paths[0][Index] == Paths[1][Index]);
	}
}

// Teleportした床は乗っている物体を運ばず、削除された床からは落ちる（支持の世代を引き継がない）。
template <typename T> void TeleportAndRemoval_Internal()
{
	FPlatformScene<T> World;
	const auto Before = World.Character->GetCenter();
	typename T::FPose Away;
	Away.Position = T::At(30);
	World.Mover->Teleport(Away);
	World.Run(1);
	REQUIRE(!World.Character->GetLastStep().bCarried || Abs(T::X(World.Character->GetCenter()) - T::X(Before)) < 1e-6);
	World.Run(30);
	REQUIRE(!World.Character->IsGrounded() && T::Y(World.Character->GetCenter()) < T::Y(Before) - 0.5);
	REQUIRE(Abs(T::X(World.Character->GetCenter()) - T::X(Before)) < 1e-4);
}
} // namespace

TEST("2D character rides translating platforms")
{
	Translates_Internal<FCase2D>();
}
TEST("3D character rides translating platforms")
{
	Translates_Internal<FCase3D>();
}
TEST("2D character follows a tilting platform as a rigid point")
{
	Rotates_Internal<FCase2D>();
}
TEST("3D character follows a turning platform as a rigid point")
{
	Rotates_Internal<FCase3D>();
}
TEST("2D character walking against a platform stays in place")
{
	WalksAgainst_Internal<FCase2D>();
}
TEST("3D character walking against a platform stays in place")
{
	WalksAgainst_Internal<FCase3D>();
}
TEST("2D character jump inherits platform velocity once")
{
	JumpInherits_Internal<FCase2D>();
}
TEST("3D character jump inherits platform velocity once")
{
	JumpInherits_Internal<FCase3D>();
}
TEST("2D character crushed against a ceiling does not pass it")
{
	CrushedAgainstCeiling_Internal<FCase2D>();
}
TEST("3D character crushed against a ceiling does not pass it")
{
	CrushedAgainstCeiling_Internal<FCase3D>();
}
TEST("2D character carried into a wall is stopped by it")
{
	PushedIntoWall_Internal<FCase2D>();
}
TEST("3D character carried into a wall is stopped by it")
{
	PushedIntoWall_Internal<FCase3D>();
}
TEST("2D character platform result does not depend on spawn order")
{
	OrderIndependent_Internal<FCase2D>();
}
TEST("3D character platform result does not depend on spawn order")
{
	OrderIndependent_Internal<FCase3D>();
}
TEST("2D platform teleport does not carry and the rider falls")
{
	TeleportAndRemoval_Internal<FCase2D>();
}
TEST("3D platform teleport does not carry and the rider falls")
{
	TeleportAndRemoval_Internal<FCase3D>();
}
