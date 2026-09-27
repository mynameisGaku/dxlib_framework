// SPDX-License-Identifier: NOASSERTION
// カプセルのキャラクター移動（S3）:
// 立ち・歩行・ジャンプ・天井・低い通路・しゃがみと立ち上がり・段差・坂・Sensor・動く床・
// Upに沿う中心線・初期重なりの解消・設定の検査。実World2D／3Dで実行し、期待値は配置から求めた解析値。
// 既定（半径0.5、接触余裕0.02）で半高0.4のカプセルが上面y=0の床に立つ中心はy=0.02+0.5+0.4=0.92。
#include "TestCases.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr f64 StepSeconds = 1.0 / 60.0;
constexpr f32 Standing = 0.92f;
constexpr f32 SixthPi = 0.523598775598f;
constexpr f32 ThirdPi = 1.0471975512f;

template <typename F> bool Throws_Internal(F&& Run)
{
	try
	{
		Run();
	}
	catch (const FException&)
	{
		return true;
	}
	return false;
}
bool Near_Internal(f64 Value, f64 Expected, f64 Limit)
{
	return Abs(Value - Expected) <= Limit;
}

// 2D Worldの型と登録操作。
struct F2D
{
	using FWorld = FPhysicsWorld2D;
	using FBodyId = FBodyId2D;
	using FColliderId = FColliderId2D;
	using FVector = FVector2;
	using FSettings = FCharacterMoveSettings2D;
	using FState = FCharacterState2D;
	using FInput = FCharacterMoveInput2D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y};
	}
	static FBodyId Body(FWorld& World, FVector Position = {}, EBodyType Type = EBodyType::Static)
	{
		FBodyDescription2D Description;
		Description.Position = Position;
		Description.Type = Type;
		return World.CreateBody(Description);
	}
	static FColliderId Box(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY, f32 Angle = 0,
	                       EColliderResponse Response = EColliderResponse::Solid)
	{
		FColliderDescription2D Description;
		Description.Shape = FOrientedBox2D{Center, {HalfX, HalfY}, Angle};
		Description.Response = Response;
		return World.AttachCollider(Body, Description);
	}
	static FVector Right()
	{
		return {1, 0};
	}
};

// 3D Worldの型と登録操作（Z=0の平面に2Dと同じ配置を作る）。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	using FVector = FVector3;
	using FSettings = FCharacterMoveSettings3D;
	using FState = FCharacterState3D;
	using FInput = FCharacterMoveInput3D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y, 0};
	}
	static FBodyId Body(FWorld& World, FVector Position = {}, EBodyType Type = EBodyType::Static)
	{
		FBodyDescription3D Description;
		Description.Position = Position;
		Description.Type = Type;
		return World.CreateBody(Description);
	}
	static FColliderId Box(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY, f32 Angle = 0,
	                       EColliderResponse Response = EColliderResponse::Solid)
	{
		FOBB Shape{Center, {HalfX, HalfY, 50}};
		const f32 Cosine = static_cast<f32>(Cos(f64(Angle)));
		const f32 Sine = static_cast<f32>(Sin(f64(Angle)));
		Shape.Axes = {FVector3{Cosine, Sine, 0}, FVector3{-Sine, Cosine, 0}, FVector3{0, 0, 1}};
		FColliderDescription3D Description;
		Description.Shape = Shape;
		Description.Response = Response;
		return World.AttachCollider(Body, Description);
	}
	static FVector Right()
	{
		return {1, 0, 0};
	}
};

// 半高0.4のカプセルの設定。
template <typename T> typename T::FSettings Capsule_Internal(f64 HalfHeight = 0.4)
{
	typename T::FSettings Settings;
	Settings.Shape = ECharacterShape::Capsule;
	Settings.HalfHeight = HalfHeight;
	return Settings;
}
template <typename T>
auto Step_Internal(const typename T::FWorld& World, const typename T::FSettings& Settings, typename T::FState& State,
                   f32 MoveX, bool bJump = false)
{
	typename T::FInput Input;
	Input.Move = T::At(MoveX);
	Input.bJump = bJump;
	const auto Result = StepCharacter(World, Settings, State, Input, StepSeconds);
	State = Result.State;
	return Result;
}
template <typename T> typename T::FState StateAt_Internal(typename T::FVector Center)
{
	typename T::FState State;
	State.Center = Center;
	return State;
}
// 上面y=0の床（中心(CenterX,-1)、半幅(HalfX,1)）。
template <typename T>
typename T::FColliderId Floor_Internal(typename T::FWorld& World, typename T::FBodyId Body, f32 CenterX = 0,
                                       f32 HalfX = 50)
{
	return T::Box(World, Body, T::At(CenterX, -1), HalfX, 1);
}

// 立ち・歩行・ジャンプ・天井。水平の移動と重力の式は円／球と同じで、高さだけが半高の分だけ違う。
template <typename T> void WalkJumpCeiling_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World);
	Floor_Internal<T>(World, Level);
	const auto Settings = Capsule_Internal<T>();
	auto State = StateAt_Internal<T>(T::At(0, Standing));
	Step_Internal<T>(World, Settings, State, 0);
	PHYSICS_REQUIRE(State.Center == T::At(0, Standing) && State.Ground.State == ECharacterGroundState::Walkable);
	for (int32 Index = 0; Index < 60; ++Index)
	{
		Step_Internal<T>(World, Settings, State, 1);
		PHYSICS_REQUIRE(State.Ground.State == ECharacterGroundState::Walkable);
	}
	PHYSICS_REQUIRE(Near_Internal(State.Center.X, (40.0 / 60.0 * 28 + 53 * 5) / 60, 1e-4) &&
	                Near_Internal(State.Center.Y, Standing, 1e-6));
	// ジャンプの頂点は0.92+57/60で、着地して0.92へ戻る。
	State = StateAt_Internal<T>(T::At(0, Standing));
	f64 Highest = 0;
	bool bLanded = false;
	for (int32 Index = 0; Index < 60; ++Index)
	{
		const auto Step = Step_Internal<T>(World, Settings, State, 0, Index == 0);
		Highest = Max(Highest, f64(State.Center.Y));
		bLanded = bLanded || Step.bLanded;
	}
	PHYSICS_REQUIRE(bLanded && Near_Internal(Highest, Standing + 57.0 / 60.0, 1e-4) &&
	                Near_Internal(State.Center.Y, Standing, 1e-4));
	// 天井（下面y=2.2）: 頭（中心＋半高＋半径）が当たる。中心は2.2−0.9−0.02=1.28以下（円／球なら1.68まで上がる）。
	typename T::FWorld Low;
	const auto LowLevel = T::Body(Low);
	Floor_Internal<T>(Low, LowLevel);
	T::Box(Low, LowLevel, T::At(0, 3.2f), 50, 1);
	State = StateAt_Internal<T>(T::At(0, Standing));
	bool bCeiling = false;
	f64 Top = 0;
	for (int32 Index = 0; Index < 60; ++Index)
	{
		const auto Step = Step_Internal<T>(Low, Settings, State, 0, Index == 0);
		bCeiling = bCeiling || Step.bHitCeiling;
		Top = Max(Top, f64(State.Center.Y));
	}
	PHYSICS_REQUIRE(bCeiling && Top <= 2.2 - 0.9 + 1e-6 && Top > 2.2 - 0.9 - 0.03);
	PHYSICS_REQUIRE(State.Ground.State == ECharacterGroundState::Walkable);
}

// 低い通路（x=3〜17、下面y=1.5）: 立ったカプセルは角で止まり、しゃがむ（半高0）と通り抜ける。
// 通路の下では立ち上がれず（天井のColliderが妨げる）、通路を出ると立ち上がって足元を保つ。
template <typename T> void LowPassage_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World);
	Floor_Internal<T>(World, Level);
	const auto Roof = T::Box(World, Level, T::At(10, 2.5f), 7, 1);
	auto Settings = Capsule_Internal<T>();
	auto State = StateAt_Internal<T>(T::At(0, Standing));
	for (int32 Index = 0; Index < 90; ++Index)
	{
		Step_Internal<T>(World, Settings, State, 1);
	}
	// 上端の球の中心(x,1.32)と角(3,1.5)の距離が0.52になる位置: x = 3 − √(0.52²−0.18²)。
	const f64 Blocked = 3 - Sqrt(0.52 * 0.52 - 0.18 * 0.18);
	PHYSICS_REQUIRE(Near_Internal(State.Center.X, Blocked, 1e-3) && Near_Internal(State.Center.Y, Standing, 1e-4));
	// しゃがむ（縮めるのは常にできる）。足元を保つので中心は0.4下がる。
	const auto Crouch = ResizeCharacterCapsule(World, State.Center, Settings, 0);
	PHYSICS_REQUIRE(Crouch.bResized && !Crouch.Blocker && Near_Internal(Crouch.Center.Y, Standing - 0.4, 1e-6) &&
	                Crouch.Center.X == State.Center.X);
	Settings.HalfHeight = 0;
	State.Center = Crouch.Center;
	for (int32 Index = 0; Index < 90; ++Index)
	{
		Step_Internal<T>(World, Settings, State, 1);
	}
	PHYSICS_REQUIRE(State.Center.X > 5 && State.Center.X < 17 && Near_Internal(State.Center.Y, Standing - 0.4, 1e-4));
	// 通路の下では立ち上がれず、元の中心と天井のColliderを返す。
	const auto Stand = ResizeCharacterCapsule(World, State.Center, Settings, 0.4);
	PHYSICS_REQUIRE(!Stand.bResized && Stand.Blocker && *Stand.Blocker == Roof && Stand.Center == State.Center);
	// 通路を出て立ち上がる。
	for (int32 Index = 0; Index < 240 && State.Center.X < 18.5f; ++Index)
	{
		Step_Internal<T>(World, Settings, State, 1);
	}
	const auto Out = ResizeCharacterCapsule(World, State.Center, Settings, 0.4);
	PHYSICS_REQUIRE(Out.bResized && Near_Internal(Out.Center.Y, Standing, 1e-6));
	Settings.HalfHeight = 0.4;
	State.Center = Out.Center;
	Step_Internal<T>(World, Settings, State, 0);
	PHYSICS_REQUIRE(State.Ground.State == ECharacterGroundState::Walkable &&
	                Near_Internal(State.Center.Y, Standing, 1e-4));
	// 円／球（同じ半径）は立ったままでも通り抜ける。
	typename T::FSettings Round;
	auto Ball = StateAt_Internal<T>(T::At(0, 0.52f));
	for (int32 Index = 0; Index < 120; ++Index)
	{
		Step_Internal<T>(World, Round, Ball, 1);
	}
	PHYSICS_REQUIRE(Ball.Center.X > 5);
}

// 段差（高さ0.2は上り、0.5は止まる）・30度の坂を上り60度は上らない・Sensorは妨げない・初期重なりの解消。
template <typename T> void TerrainAndSensor_Internal()
{
	const auto Settings = Capsule_Internal<T>();
	const auto StepRun = [&](f32 Height, bool bExpectUp)
	{
		typename T::FWorld World;
		const auto Level = T::Body(World);
		Floor_Internal<T>(World, Level);
		T::Box(World, Level, T::At(22, Height / 2), 20, Height / 2);
		auto State = StateAt_Internal<T>(T::At(0, Standing));
		bool bStepped = false;
		for (int32 Index = 0; Index < 90; ++Index)
		{
			bStepped = Step_Internal<T>(World, Settings, State, 1).bSteppedUp || bStepped;
		}
		if (bExpectUp)
		{
			PHYSICS_REQUIRE(bStepped && State.Center.X > 2.5f &&
			                Near_Internal(State.Center.Y, Height + Standing, 1e-4));
		}
		else
		{
			// 段差（0.5）は下端の球の中心（0.52）より低いので角に止められる: x = 2 − √(0.52² − 0.02²)。
			PHYSICS_REQUIRE(!bStepped && Near_Internal(State.Center.X, 2 - Sqrt(0.52 * 0.52 - 0.02 * 0.02), 1e-4) &&
			                Near_Internal(State.Center.Y, Standing, 1e-4));
		}
	};
	StepRun(0.2f, true);
	StepRun(0.5f, false);
	for (int32 Case = 0; Case < 2; ++Case)
	{
		typename T::FWorld World;
		const auto Level = T::Body(World);
		Floor_Internal<T>(World, Level, -10, 10);
		const f32 Angle = Case == 0 ? SixthPi : ThirdPi;
		const f64 Cosine = Cos(f64(Angle));
		const f64 Sine = Sin(f64(Angle));
		T::Box(World, Level, T::At(static_cast<f32>(10 * Cosine + Sine), static_cast<f32>(10 * Sine - Cosine)), 10, 1,
		       Angle);
		auto State = StateAt_Internal<T>(T::At(-2, Standing));
		for (int32 Index = 0; Index < 120; ++Index)
		{
			Step_Internal<T>(World, Settings, State, 1);
		}
		if (Case == 0)
		{
			PHYSICS_REQUIRE(State.Center.X > 3 && State.Ground.State == ECharacterGroundState::Walkable &&
			                State.Center.Y > Standing + 0.5 * Sine * State.Center.X);
		}
		else
		{
			PHYSICS_REQUIRE(State.Center.X < 0.1f && State.Center.Y < Standing + 0.05f);
		}
	}
	{
		typename T::FWorld World;
		const auto Level = T::Body(World);
		Floor_Internal<T>(World, Level);
		T::Box(World, Level, T::At(3, 1), 0.5f, 1, 0, EColliderResponse::Sensor);
		auto State = StateAt_Internal<T>(T::At(0, Standing));
		for (int32 Index = 0; Index < 90; ++Index)
		{
			Step_Internal<T>(World, Settings, State, 1);
		}
		PHYSICS_REQUIRE(State.Center.X > 5 && Near_Internal(State.Center.Y, Standing, 1e-4));
		// 床へ0.07めり込んだ開始位置は、接触余裕の位置まで押し戻す。
		auto Sunk = StateAt_Internal<T>(T::At(0, Standing - 0.09f));
		const auto Recovered = Step_Internal<T>(World, Settings, Sunk, 0);
		PHYSICS_REQUIRE(Recovered.Recovery.Status == ECharacterRecoveryStatus::Resolved &&
		                Near_Internal(Sunk.Center.Y, Standing, 1e-4));
	}
}

// Kinematicの動く床（速さ1で右へ）に乗ったカプセルは運ばれ、床の上の高さを保つ。
template <typename T> void MovingGround_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World);
	Floor_Internal<T>(World, Level);
	const auto Platform = T::Body(World, T::At(0, 0.5f), EBodyType::Kinematic);
	T::Box(World, Platform, T::At(0, 0), 3, 0.25f);
	World.SetVelocity(Platform, T::At(1, 0));
	const auto Settings = Capsule_Internal<T>();
	auto State = StateAt_Internal<T>(T::At(0, 0.75f + Standing));
	for (int32 Index = 0; Index < 60; ++Index)
	{
		const auto Step = Step_Internal<T>(World, Settings, State, 0);
		PHYSICS_REQUIRE(Index == 0 || Step.bCarried);
		World.Step(StepSeconds);
	}
	PHYSICS_REQUIRE(Near_Internal(State.Center.X, f64(World.GetPosition(Platform).X), 0.02) &&
	                Near_Internal(State.Center.X, 1, 0.03) && Near_Internal(State.Center.Y, 0.75 + Standing, 1e-3));
}

// 中心線はUpに沿う: Upを+Xにすると、x<0の壁を床として立ち、中心はx=0.92。高さの変更も足元（x=0.02）を保つ。
template <typename T> void UpAxis_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World);
	T::Box(World, Level, T::At(-1, 0), 1, 50);
	auto Settings = Capsule_Internal<T>();
	Settings.Up = T::Right();
	auto State = StateAt_Internal<T>(T::At(Standing + 0.3f, 0));
	for (int32 Index = 0; Index < 60; ++Index)
	{
		Step_Internal<T>(World, Settings, State, 0);
	}
	PHYSICS_REQUIRE(Near_Internal(State.Center.X, Standing, 1e-4) && Near_Internal(State.Center.Y, 0, 1e-6) &&
	                State.Ground.State == ECharacterGroundState::Walkable);
	const auto Taller = ResizeCharacterCapsule(World, State.Center, Settings, 1.0);
	PHYSICS_REQUIRE(Taller.bResized && Near_Internal(Taller.Center.X, Standing + 0.6, 1e-6));
}

// 設定の検査: 負・非有限の半高、Roundでの高さ変更、不正な新しい半高は例外。Roundの既定は従来の問い合わせのまま。
template <typename T> void Validation_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World);
	Floor_Internal<T>(World, Level);
	auto Settings = Capsule_Internal<T>(-0.1);
	auto State = StateAt_Internal<T>(T::At(0, Standing));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    Step_Internal<T>(World, Settings, State, 0);
	    }));
	Settings.HalfHeight = NAN;
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)ProbeCharacterGround(World, State.Center, Settings);
	    }));
	typename T::FSettings Round;
	PHYSICS_REQUIRE(Round.Shape == ECharacterShape::Round && Round.HalfHeight == 0);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)ResizeCharacterCapsule(World, State.Center, Round, 0.2);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)ResizeCharacterCapsule(World, State.Center, Capsule_Internal<T>(), -1);
	    }));
	// 半高0のカプセルは同じ半径の円／球と同じ位置で立つ。
	auto Flat = StateAt_Internal<T>(T::At(0, 0.6f));
	auto Ball = Flat;
	for (int32 Index = 0; Index < 30; ++Index)
	{
		Step_Internal<T>(World, Capsule_Internal<T>(0), Flat, 1);
		Step_Internal<T>(World, Round, Ball, 1);
	}
	PHYSICS_REQUIRE(Near_Internal(Flat.Center.X, Ball.Center.X, 1e-5) &&
	                Near_Internal(Flat.Center.Y, Ball.Center.Y, 1e-5));
}

// 2Dと3Dの同じ配置で、低い通路の手前で止まる位置と、しゃがんで抜けた位置が一致する。
void Equivalence_Internal()
{
	F2D::FWorld World2D;
	F3D::FWorld World3D;
	Floor_Internal<F2D>(World2D, F2D::Body(World2D));
	Floor_Internal<F3D>(World3D, F3D::Body(World3D));
	F2D::Box(World2D, F2D::Body(World2D), F2D::At(10, 2.5f), 7, 1);
	F3D::Box(World3D, F3D::Body(World3D), F3D::At(10, 2.5f), 7, 1);
	auto State2D = StateAt_Internal<F2D>(F2D::At(0, Standing));
	auto State3D = StateAt_Internal<F3D>(F3D::At(0, Standing));
	const auto Settings2D = Capsule_Internal<F2D>();
	const auto Settings3D = Capsule_Internal<F3D>();
	for (int32 Index = 0; Index < 90; ++Index)
	{
		Step_Internal<F2D>(World2D, Settings2D, State2D, 1);
		Step_Internal<F3D>(World3D, Settings3D, State3D, 1);
		PHYSICS_REQUIRE(Near_Internal(State2D.Center.X, State3D.Center.X, 1e-5) &&
		                Near_Internal(State2D.Center.Y, State3D.Center.Y, 1e-5));
	}
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"2D capsule character walk jump ceiling", &WalkJumpCeiling_Internal<F2D>},
    {"3D capsule character walk jump ceiling", &WalkJumpCeiling_Internal<F3D>},
    {"2D capsule character low passage crouch and stand", &LowPassage_Internal<F2D>},
    {"3D capsule character low passage crouch and stand", &LowPassage_Internal<F3D>},
    {"2D capsule character steps slopes sensor recovery", &TerrainAndSensor_Internal<F2D>},
    {"3D capsule character steps slopes sensor recovery", &TerrainAndSensor_Internal<F3D>},
    {"2D capsule character moving ground", &MovingGround_Internal<F2D>},
    {"3D capsule character moving ground", &MovingGround_Internal<F3D>},
    {"2D capsule character axis follows up", &UpAxis_Internal<F2D>},
    {"3D capsule character axis follows up", &UpAxis_Internal<F3D>},
    {"2D capsule character validation", &Validation_Internal<F2D>},
    {"3D capsule character validation", &Validation_Internal<F3D>},
    {"2D and 3D capsule characters agree", &Equivalence_Internal}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetCharacterCapsuleCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
