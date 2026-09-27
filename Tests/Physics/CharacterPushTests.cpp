// SPDX-License-Identifier: NOASSERTION
// キャラクター ⇔ Dynamicの剛体の押し合い（S4）。StepCharacterの押す要求を物理Stepの前に適用し、実World2D／3Dで進める。
// 確認:
// 軽い箱は押され重い箱は動かない、壁に挟まれた箱、二つの箱、坂、段差、ジャンプ、休止した箱の起床、Sensor・対象外の箱、
// 近づく箱に押されて退く、既定では押さない、設定の検査、2D／3Dの一致。キャラクターは半高0.4のカプセル（足元y=0で中心0.92）。
#include "TestCases.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr f64 StepSeconds = 1.0 / 60.0;
constexpr f32 Standing = 0.92f;

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
	using FStepResult = FCharacterStepResult2D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y};
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type, f32 Mass = 1)
	{
		FBodyDescription2D Description;
		Description.Position = Position;
		Description.Type = Type;
		Description.Mass = Mass;
		Description.Inertia = Mass * 0.2f;
		return World.CreateBody(Description);
	}
	static FColliderId Box(FWorld& World, FBodyId Body, f32 HalfX, f32 HalfY, f32 Angle = 0,
	                       EColliderResponse Response = EColliderResponse::Solid, uint32 Category = 1u)
	{
		FColliderDescription2D Description;
		Description.Shape = FOrientedBox2D{{0, 0}, {HalfX, HalfY}, Angle};
		Description.Response = Response;
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static FColliderId Capsule(FWorld& World, FBodyId Body, f32 Half, f32 Radius)
	{
		FColliderDescription2D Description;
		Description.Shape = FCapsule2D{{0, -Half}, {0, Half}, Radius};
		return World.AttachCollider(Body, Description);
	}
	static void Place(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, 0);
	}
};

// 3D Worldの型と登録操作（Z=0の平面に2Dと同じ配置を作る。箱の奥行きの半幅は0.5）。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	using FVector = FVector3;
	using FSettings = FCharacterMoveSettings3D;
	using FState = FCharacterState3D;
	using FInput = FCharacterMoveInput3D;
	using FStepResult = FCharacterStepResult3D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y, 0};
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type, f32 Mass = 1)
	{
		FBodyDescription3D Description;
		Description.Position = Position;
		Description.Type = Type;
		Description.Mass = Mass;
		Description.DiagonalInertia = {Mass * 0.2f, Mass * 0.2f, Mass * 0.2f};
		return World.CreateBody(Description);
	}
	static FColliderId Box(FWorld& World, FBodyId Body, f32 HalfX, f32 HalfY, f32 Angle = 0,
	                       EColliderResponse Response = EColliderResponse::Solid, uint32 Category = 1u)
	{
		FOBB Shape{{0, 0, 0}, {HalfX, HalfY, HalfX < 5 ? 0.5f : 50}};
		const f32 Cosine = static_cast<f32>(Cos(f64(Angle)));
		const f32 Sine = static_cast<f32>(Sin(f64(Angle)));
		Shape.Axes = {FVector3{Cosine, Sine, 0}, FVector3{-Sine, Cosine, 0}, FVector3{0, 0, 1}};
		FColliderDescription3D Description;
		Description.Shape = Shape;
		Description.Response = Response;
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static FColliderId Capsule(FWorld& World, FBodyId Body, f32 Half, f32 Radius)
	{
		FColliderDescription3D Description;
		Description.Shape = FCapsule{{0, -Half, 0}, {0, Half, 0}, Radius};
		return World.AttachCollider(Body, Description);
	}
	static void Place(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, FQuaternion{});
	}
};

// 床（上面y=0、x∈[−50,50]）とカプセルのキャラクター。キャラクターのBodyは任意で登録する（Kinematicのカプセル）。
template <typename T> struct TPushScene
{
	typename T::FWorld World;
	typename T::FSettings Settings;
	typename T::FState State;
	Toolbox::TOptional<typename T::FBodyId> Self;
	// 押す要求を出したStepの数と、要求の最大の大きさ・Up成分の最大。
	int32 PushSteps = 0;
	f64 LargestImpulse = 0;
	f64 LargestVertical = 0;
	explicit TPushScene(bool bPush = true, bool bRegisterBody = false, f32 StartX = 0)
	{
		const auto Floor = T::Body(World, T::At(0, -1), EBodyType::Static);
		T::Box(World, Floor, 50, 1);
		Settings.Shape = ECharacterShape::Capsule;
		Settings.HalfHeight = 0.4;
		Settings.bPushDynamicBodies = bPush;
		State.Center = T::At(StartX, Standing);
		if (bRegisterBody)
		{
			Self = T::Body(World, State.Center, EBodyType::Kinematic);
			T::Capsule(World, *Self, 0.4f, 0.5f);
		}
	}
	// 1回の固定更新: キャラクターを進め、押す要求を適用してから物理Stepを進める。
	typename T::FStepResult Tick(f32 MoveX, bool bJump = false)
	{
		typename T::FInput Input;
		Input.Move = T::At(MoveX);
		Input.bJump = bJump;
		const auto Result = StepCharacter(World, Settings, State, Input, StepSeconds, Self);
		State = Result.State;
		if (Self)
		{
			T::Place(World, *Self, State.Center);
		}
		PushSteps += Result.Pushes.Count > 0 ? 1 : 0;
		for (uint32 Index = 0; Index < Result.Pushes.Count; ++Index)
		{
			const auto& Push = Result.Pushes.Items[Index];
			LargestImpulse = Max(LargestImpulse, Sqrt(f64(Dot(Push.Impulse, Push.Impulse))));
			LargestVertical = Max(LargestVertical, Abs(f64(Push.Impulse.Y)));
			World.ApplyLinearImpulse(Push.Body, Push.Impulse);
		}
		World.Step(StepSeconds);
		return Result;
	}
	// 床に置いた箱（半幅0.5、中心の高さ0.5）。
	typename T::FBodyId Crate(f32 X, f32 Mass = 1, EColliderResponse Response = EColliderResponse::Solid,
	                          uint32 Category = 1u)
	{
		const auto Body = T::Body(World, T::At(X, 0.5f), EBodyType::Dynamic, Mass);
		T::Box(World, Body, 0.5f, 0.5f, 0, Response, Category);
		return Body;
	}
};

// 軽い箱は押されて進み、重い箱（質量200）は摩擦に負けて動かない。要求は水平で上限以下。既定の設定では押さない。
template <typename T> void LightAndHeavy_Internal()
{
	TPushScene<T> Light;
	const auto LightBox = Light.Crate(2);
	for (int32 Index = 0; Index < 120; ++Index)
	{
		Light.Tick(1);
	}
	// 軽い箱は押されてキャラクターより先へ進む（押す要求は接している間だけ）。
	PHYSICS_REQUIRE(Light.World.GetPosition(LightBox).X > 4 && Light.PushSteps > 0);
	PHYSICS_REQUIRE(Light.LargestImpulse <= Light.Settings.MaxPushImpulse + 1e-6 && Light.LargestVertical == 0);
	PHYSICS_REQUIRE(Light.State.Center.X > 2.5f);
	TPushScene<T> Heavy;
	const auto HeavyBox = Heavy.Crate(2, 200);
	for (int32 Index = 0; Index < 120; ++Index)
	{
		Heavy.Tick(1);
	}
	PHYSICS_REQUIRE(Abs(f64(Heavy.World.GetPosition(HeavyBox).X) - 2) < 0.02 && Heavy.PushSteps > 60);
	PHYSICS_REQUIRE(Heavy.State.Center.X < 1.5f);
	TPushScene<T> Off(false);
	const auto Still = Off.Crate(2);
	for (int32 Index = 0; Index < 120; ++Index)
	{
		Off.Tick(1);
	}
	PHYSICS_REQUIRE(Off.PushSteps == 0 && Abs(f64(Off.World.GetPosition(Still).X) - 2) < 1e-3);
}

// 壁に接した箱は押しても壁を越えず、値は有限のまま。二つ並んだ箱は前の箱を通して後ろの箱も進む。
template <typename T> void WallAndTwoBoxes_Internal()
{
	TPushScene<T> Pinned;
	const auto Box = Pinned.Crate(2);
	const auto Wall = T::Body(Pinned.World, T::At(3.5f, 2), EBodyType::Static);
	T::Box(Pinned.World, Wall, 1, 2);
	for (int32 Index = 0; Index < 120; ++Index)
	{
		Pinned.Tick(1);
	}
	const auto Position = Pinned.World.GetPosition(Box);
	PHYSICS_REQUIRE(Position.IsValid() && Position.X < 2.03f && Position.X > 1.9f && Pinned.PushSteps > 60);
	PHYSICS_REQUIRE(Pinned.State.Center.IsValid() && Pinned.State.Center.X < 1.5f);
	TPushScene<T> Pair;
	Pair.Crate(2);
	const auto Back = Pair.Crate(3.05f);
	for (int32 Index = 0; Index < 120; ++Index)
	{
		Pair.Tick(1);
	}
	PHYSICS_REQUIRE(Pair.World.GetPosition(Back).X > 4.5f);
}

// 段差（高さ0.2、x≥3）を上ってから、上の段の箱を押す。上った固定更新では押す要求を出さない。
template <typename T> void StepThenPush_Internal()
{
	TPushScene<T> Scene;
	const auto Step = T::Body(Scene.World, T::At(23, 0.1f), EBodyType::Static);
	T::Box(Scene.World, Step, 20, 0.1f);
	const auto Box = T::Body(Scene.World, T::At(6, 0.7f), EBodyType::Dynamic);
	T::Box(Scene.World, Box, 0.5f, 0.5f);
	bool bStepped = false;
	for (int32 Index = 0; Index < 180; ++Index)
	{
		const auto Result = Scene.Tick(1);
		if (Result.bSteppedUp)
		{
			bStepped = true;
			PHYSICS_REQUIRE(Result.Pushes.Count == 0);
		}
	}
	PHYSICS_REQUIRE(bStepped && Scene.World.GetPosition(Box).X > 7);
	PHYSICS_REQUIRE(Abs(f64(Scene.State.Center.Y) - (0.2 + Standing)) < 1e-3);
}

// 15度の坂を上りながら坂の上の箱を押す: 要求は水平で、箱は坂を上る。
template <typename T> void SlopePush_Internal()
{
	TPushScene<T> Scene;
	const f32 Angle = 0.2617993877991494f;
	const f64 Cosine = Cos(f64(Angle));
	const f64 Sine = Sin(f64(Angle));
	// 坂の上面はx=0からの直線y=x·tan15°（厚さ2の板、上面の中心(10cos,10sin)）。
	const auto Ramp =
	    T::Body(Scene.World, T::At(static_cast<f32>(10 * Cosine + Sine), static_cast<f32>(10 * Sine - Cosine)),
	            EBodyType::Static);
	T::Box(Scene.World, Ramp, 10, 1, Angle);
	// 坂の上（x≈4）に箱を置いて落ち着かせる。
	const auto Box =
	    T::Body(Scene.World, T::At(4, static_cast<f32>(4 * Sine / Cosine + 0.52 / Cosine + 0.05)), EBodyType::Dynamic);
	T::Box(Scene.World, Box, 0.5f, 0.5f, Angle);
	for (int32 Index = 0; Index < 60; ++Index)
	{
		Scene.World.Step(StepSeconds);
	}
	const f32 Settled = Scene.World.GetPosition(Box).X;
	Scene.State.Center = T::At(-1, Standing);
	for (int32 Index = 0; Index < 150; ++Index)
	{
		Scene.Tick(1);
	}
	PHYSICS_REQUIRE(Scene.PushSteps > 5 && Scene.LargestVertical == 0);
	PHYSICS_REQUIRE(Scene.World.GetPosition(Box).X > Settled + 0.5f && Scene.World.GetPosition(Box).Y > 1.1f);
}

// ジャンプ: 空中で横から押せる。箱の上に着地しても、下へ押す要求は出さず箱は沈まない。
template <typename T> void JumpPush_Internal()
{
	// 10回目に跳ぶ（x≈0.64、速さ約4.4）と、箱（x=4、左面3.5）の横には空中で届く。
	TPushScene<T> Air;
	const auto Box = Air.Crate(4);
	int32 AirPushes = 0;
	for (int32 Index = 0; Index < 90; ++Index)
	{
		const auto Result = Air.Tick(1, Index == 10);
		// 空中（歩ける床に立っていない。箱の上の角に触れるとSteepになる）で押した回数。
		AirPushes += (Result.Pushes.Count > 0 && Result.State.Ground.State != ECharacterGroundState::Walkable) ? 1 : 0;
	}
	PHYSICS_REQUIRE(AirPushes > 0 && Air.World.GetPosition(Box).X > 4.5f);
	TPushScene<T> Land;
	const auto Under = Land.Crate(0);
	Land.State.Center = T::At(0, 1 + Standing + 0.5f);
	for (int32 Index = 0; Index < 90; ++Index)
	{
		Land.Tick(0);
	}
	PHYSICS_REQUIRE(Land.PushSteps == 0 && Land.State.Ground.State == ECharacterGroundState::Walkable);
	PHYSICS_REQUIRE(Abs(f64(Land.World.GetPosition(Under).Y) - 0.5) < 0.02 &&
	                Abs(f64(Land.State.Center.Y) - (1 + Standing)) < 0.02);
}

// 休止した箱は押す要求で起き、Sensorの箱と問い合わせの対象外の箱は押さず、妨げにもならない。
template <typename T> void SleepSensorFilter_Internal()
{
	TPushScene<T> Scene;
	const auto Box = Scene.Crate(4);
	Scene.State.Center = T::At(0, Standing);
	for (int32 Index = 0; Index < 120; ++Index)
	{
		Scene.Tick(0);
	}
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Box));
	bool bWoke = false;
	for (int32 Index = 0; Index < 90; ++Index)
	{
		const auto Result = Scene.Tick(1);
		if (Result.Pushes.Count > 0)
		{
			bWoke = !Scene.World.IsSleeping(Box);
			break;
		}
	}
	PHYSICS_REQUIRE(bWoke);
	for (int32 Index = 0; Index < 30; ++Index)
	{
		Scene.Tick(1);
	}
	PHYSICS_REQUIRE(Scene.World.GetPosition(Box).X > 4.2f);
	for (int32 Case = 0; Case < 2; ++Case)
	{
		TPushScene<T> Pass;
		const auto Ignored =
		    Case == 0 ? Pass.Crate(2, 1, EColliderResponse::Sensor) : Pass.Crate(2, 1, EColliderResponse::Solid, 2u);
		// 問い合わせのカテゴリ1だけを見るキャラクター。
		for (int32 Index = 0; Index < 90; ++Index)
		{
			typename T::FInput Input;
			Input.Move = T::At(1);
			FWorldQueryFilter Filter;
			Filter.IncludeCategories = 1u;
			const auto Result = StepCharacter(Pass.World, Pass.Settings, Pass.State, Input, StepSeconds, {}, Filter);
			Pass.State = Result.State;
			PHYSICS_REQUIRE(Result.Pushes.Count == 0);
			Pass.World.Step(StepSeconds);
		}
		PHYSICS_REQUIRE(Pass.State.Center.X > 4 && Abs(f64(Pass.World.GetPosition(Ignored).X) - 2) < 1e-3);
	}
}

// 近づく箱（速さ3、摩擦0）:
// 押される設定では退き、箱は速さを保って進む。既定では箱がキャラクター（KinematicのBody）に止められる。
template <typename T> void ReceivePush_Internal()
{
	for (int32 Case = 0; Case < 2; ++Case)
	{
		TPushScene<T> Scene(false, true);
		Scene.Settings.bReceiveDynamicPush = Case == 0;
		const auto Box = T::Body(Scene.World, T::At(3, 0.5f), EBodyType::Dynamic);
		T::Box(Scene.World, Box, 0.5f, 0.5f);
		Scene.World.SetGravity(T::At(0, 0));
		Scene.World.SetVelocity(Box, T::At(-3, 0));
		// 箱は約40回で届き、その後は同じ速さで押し続ける。
		bool bPushed = false;
		for (int32 Index = 0; Index < 90; ++Index)
		{
			bPushed = Scene.Tick(0).bPushedByBody || bPushed;
		}
		if (Case == 0)
		{
			PHYSICS_REQUIRE(bPushed && Scene.State.Center.X < -1.5f && Scene.World.GetVelocity(Box).X < -2.5f);
			// 退いた後も、箱の面から接触余裕の近くにいる（箱にめり込まない）。
			PHYSICS_REQUIRE(f64(Scene.World.GetPosition(Box).X) - Scene.State.Center.X > 0.5 + 0.5 - 1e-3);
		}
		else
		{
			PHYSICS_REQUIRE(!bPushed && Scene.State.Center.X == 0 && Scene.World.GetPosition(Box).X > 0.9f);
		}
	}
}

// 設定の検査: 負・非有限の係数は例外。
template <typename T> void Validation_Internal()
{
	TPushScene<T> Scene;
	for (int32 Case = 0; Case < 3; ++Case)
	{
		auto Settings = Scene.Settings;
		if (Case == 0)
		{
			Settings.PushForceScale = -1;
		}
		else if (Case == 1)
		{
			Settings.MaxPushImpulse = NAN;
		}
		else
		{
			Settings.MaxReceivedPushSpeed = -0.5;
		}
		typename T::FInput Input;
		PHYSICS_REQUIRE(Throws_Internal(
		    [&]
		    {
			    (void)StepCharacter(Scene.World, Settings, Scene.State, Input, StepSeconds);
		    }));
	}
}

// 2Dと3Dの同じ配置で、押された箱とキャラクターの位置が一致する。
void Equivalence_Internal()
{
	TPushScene<F2D> Scene2D;
	TPushScene<F3D> Scene3D;
	const auto Box2D = Scene2D.Crate(2);
	const auto Box3D = Scene3D.Crate(2);
	for (int32 Index = 0; Index < 90; ++Index)
	{
		Scene2D.Tick(1);
		Scene3D.Tick(1);
	}
	PHYSICS_REQUIRE(Abs(f64(Scene2D.World.GetPosition(Box2D).X) - Scene3D.World.GetPosition(Box3D).X) < 0.02);
	PHYSICS_REQUIRE(Abs(f64(Scene2D.State.Center.X) - Scene3D.State.Center.X) < 0.02);
	PHYSICS_REQUIRE(Scene2D.PushSteps == Scene3D.PushSteps);
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"2D character pushes light boxes but not heavy ones", &LightAndHeavy_Internal<F2D>},
    {"3D character pushes light boxes but not heavy ones", &LightAndHeavy_Internal<F3D>},
    {"2D character push against a wall and through two boxes", &WallAndTwoBoxes_Internal<F2D>},
    {"3D character push against a wall and through two boxes", &WallAndTwoBoxes_Internal<F3D>},
    {"2D character steps up then pushes", &StepThenPush_Internal<F2D>},
    {"3D character steps up then pushes", &StepThenPush_Internal<F3D>},
    {"2D character pushes up a slope horizontally", &SlopePush_Internal<F2D>},
    {"3D character pushes up a slope horizontally", &SlopePush_Internal<F3D>},
    {"2D character pushes in the air but not down", &JumpPush_Internal<F2D>},
    {"3D character pushes in the air but not down", &JumpPush_Internal<F3D>},
    {"2D character push wakes sleepers and skips sensors and filtered boxes", &SleepSensorFilter_Internal<F2D>},
    {"3D character push wakes sleepers and skips sensors and filtered boxes", &SleepSensorFilter_Internal<F3D>},
    {"2D character receives pushes from approaching boxes", &ReceivePush_Internal<F2D>},
    {"3D character receives pushes from approaching boxes", &ReceivePush_Internal<F3D>},
    {"2D character push validation", &Validation_Internal<F2D>},
    {"3D character push validation", &Validation_Internal<F3D>},
    {"2D and 3D character pushes agree", &Equivalence_Internal}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetCharacterPushCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
