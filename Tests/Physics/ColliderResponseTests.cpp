// SPDX-License-Identifier: NOASSERTION
// ColliderのSolid／Sensorの区分と、接触・Triggerの組を決める衝突フィルター（R1）。
// 同じ契約を実FPhysicsWorld2D／FPhysicsWorld3Dの両方で実行する。期待値は物理応答の有無と、区分を使わない対照のWorldから求める。
#include "TestCases.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr f64 StepSeconds = 1.0 / 60.0;

// 指定処理がFExceptionを送出するか調べる。
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

// 2D Worldの型と登録操作（Yが上）。
struct F2D
{
	using FWorld = FPhysicsWorld2D;
	using FBodyId = FBodyId2D;
	using FColliderId = FColliderId2D;
	using FVector = FVector2;
	using FSettings = FCharacterMoveSettings2D;
	using FState = FCharacterState2D;
	using FInput = FCharacterMoveInput2D;
	static FVector At(f32 X, f32 Y)
	{
		return {X, Y};
	}
	static f32 Height(FVector Value)
	{
		return Value.Y;
	}
	static f32 Side(FVector Value)
	{
		return Value.X;
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type)
	{
		FBodyDescription2D Description;
		Description.Type = Type;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static FColliderDescription2D BallShape(f32 Radius)
	{
		FColliderDescription2D Description;
		Description.Shape = FCircle2D{{0, 0}, Radius};
		return Description;
	}
	static FColliderDescription2D BoxShape(f32 HalfX, f32 HalfY, f32 Angle = 0)
	{
		FColliderDescription2D Description;
		Description.Shape = FOrientedBox2D{{0, 0}, {HalfX, HalfY}, Angle};
		return Description;
	}
	static void Continuous(FWorld& World)
	{
		FContinuousSettings2D Settings;
		Settings.bEnabled = true;
		World.SetContinuousSettings(Settings);
	}
	static FInput Walk(f32 X)
	{
		FInput Input;
		Input.Move = {X, 0};
		return Input;
	}
};

// 3D Worldの型と登録操作（Yが上）。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	using FVector = FVector3;
	using FSettings = FCharacterMoveSettings3D;
	using FState = FCharacterState3D;
	using FInput = FCharacterMoveInput3D;
	static FVector At(f32 X, f32 Y)
	{
		return {X, Y, 0};
	}
	static f32 Height(FVector Value)
	{
		return Value.Y;
	}
	static f32 Side(FVector Value)
	{
		return Value.X;
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type)
	{
		FBodyDescription3D Description;
		Description.Type = Type;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static FColliderDescription3D BallShape(f32 Radius)
	{
		FColliderDescription3D Description;
		Description.Shape = FSphere{{0, 0, 0}, Radius};
		return Description;
	}
	static FColliderDescription3D BoxShape(f32 HalfX, f32 HalfY, f32 Angle = 0)
	{
		FColliderDescription3D Description;
		FOBB Box{{0, 0, 0}, {HalfX, HalfY, 5}};
		const f32 C = static_cast<f32>(Cos(Angle));
		const f32 S = static_cast<f32>(Sin(Angle));
		Box.Axes[0] = {C, S, 0};
		Box.Axes[1] = {-S, C, 0};
		Box.Axes[2] = {0, 0, 1};
		Description.Shape = Box;
		return Description;
	}
	static void Continuous(FWorld& World)
	{
		FContinuousSettings3D Settings;
		Settings.bEnabled = true;
		World.SetContinuousSettings(Settings);
	}
	static FInput Walk(f32 X)
	{
		FInput Input;
		Input.Move = {X, 0, 0};
		return Input;
	}
};

// 静止した床（中心y=0、上面y=0.5）と、その上へ落ちる球。床の区分・フィルターだけを変えて比べる。
template <typename T> struct FDropScene
{
	typename T::FWorld World;
	typename T::FBodyId Floor;
	typename T::FColliderId FloorCollider;
	typename T::FBodyId Ball;
	typename T::FColliderId BallCollider;
	FDropScene(EColliderResponse FloorResponse, FColliderCollisionFilter FloorFilter = {},
	           FColliderCollisionFilter BallFilter = {}, bool bWithFloor = true)
	{
		Floor = T::Body(World, T::At(0, 0), EBodyType::Static);
		if (bWithFloor)
		{
			auto Description = T::BoxShape(5, 0.5f);
			Description.Response = FloorResponse;
			Description.Collision = FloorFilter;
			FloorCollider = World.AttachCollider(Floor, Description);
		}
		Ball = T::Body(World, T::At(0, 2), EBodyType::Dynamic);
		auto Description = T::BallShape(0.5f);
		Description.Collision = BallFilter;
		BallCollider = World.AttachCollider(Ball, Description);
	}
	void Run(int32 Steps)
	{
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			World.Step(StepSeconds);
		}
	}
	f32 BallHeight() const
	{
		return T::Height(World.GetPosition(Ball));
	}
};

// 既定の登録はSolid・カテゴリ1・全マスクで、従来どおり床に止まる。Sensorの床は押し返さず、床のないWorldと同じ経過になる。
template <typename T> void SensorHasNoResponse_Internal()
{
	FColliderDescription2D Default2D;
	FColliderDescription3D Default3D;
	PHYSICS_REQUIRE(Default2D.Response == EColliderResponse::Solid && Default3D.Response == EColliderResponse::Solid);
	PHYSICS_REQUIRE(Default2D.Collision.Category == 1u && Default2D.Collision.Mask == 0xffffffffu);
	PHYSICS_REQUIRE(Default3D.Collision.Category == 1u && Default3D.Collision.Mask == 0xffffffffu);
	FDropScene<T> Solid(EColliderResponse::Solid);
	FDropScene<T> Sensor(EColliderResponse::Sensor);
	FDropScene<T> Empty(EColliderResponse::Solid, {}, {}, false);
	Solid.Run(120);
	Sensor.Run(120);
	Empty.Run(120);
	PHYSICS_REQUIRE(Solid.BallHeight() > 0.9f && Solid.BallHeight() < 1.1f);
	PHYSICS_REQUIRE(Sensor.BallHeight() < -5);
	// Sensorの床を通過する球は、床のないWorldとビット単位で同じ位置・速度（摩擦・反発・位置補正を受けない）。
	PHYSICS_REQUIRE(Sensor.World.GetPosition(Sensor.Ball) == Empty.World.GetPosition(Empty.Ball));
	PHYSICS_REQUIRE(Sensor.World.GetVelocity(Sensor.Ball) == Empty.World.GetVelocity(Empty.Ball));
}

// 衝突フィルターは両側の許可が必要。片側のマスクだけが相手を含む組は応答しない。問い合わせのカテゴリは使わない。
template <typename T> void FilterIsSymmetric_Internal()
{
	constexpr uint32 Ground = 1u << 0;
	constexpr uint32 Ghost = 1u << 3;
	// 床は幽霊を許さないが、幽霊の球は床を許す：組にならず落ちる。
	FDropScene<T> OneSided(EColliderResponse::Solid, {Ground, ~Ghost}, {Ghost, Ground});
	// 両側が許す：止まる。
	FDropScene<T> Both(EColliderResponse::Solid, {Ground, Ghost}, {Ghost, Ground});
	// 衝突カテゴリ0はどの組にもならない。
	FDropScene<T> Zero(EColliderResponse::Solid, {0u, 0xffffffffu}, {});
	OneSided.Run(120);
	Both.Run(120);
	Zero.Run(120);
	PHYSICS_REQUIRE(OneSided.BallHeight() < -5);
	PHYSICS_REQUIRE(Both.BallHeight() > 0.9f);
	PHYSICS_REQUIRE(Zero.BallHeight() < -5);
	PHYSICS_REQUIRE(AllowsCollisionPair({Ground, Ghost}, {Ghost, Ground}));
	PHYSICS_REQUIRE(!AllowsCollisionPair({Ground, ~Ghost}, {Ghost, Ground}));
	PHYSICS_REQUIRE(!AllowsCollisionPair({Ghost, Ground}, {Ground, ~Ghost}));
	// 問い合わせのカテゴリを0にしても接触は残り、衝突カテゴリを0にしても問い合わせの対象のまま（互いに独立）。
	FDropScene<T> QueryHidden(EColliderResponse::Solid);
	QueryHidden.World.SetColliderQueryCategory(QueryHidden.FloorCollider, 0u);
	QueryHidden.Run(120);
	PHYSICS_REQUIRE(QueryHidden.BallHeight() > 0.9f);
	PHYSICS_REQUIRE(Zero.World.RaycastClosest(T::At(3, 5), T::At(3, -5))->Collider == Zero.FloorCollider);
}

// 高速な連続衝突の球は、Sensorの薄い壁で止まらず、Solidの薄い壁では止まる。
template <typename T> void ContinuousIgnoresSensors_Internal()
{
	for (EColliderResponse Response : {EColliderResponse::Solid, EColliderResponse::Sensor})
	{
		typename T::FWorld World;
		T::Continuous(World);
		World.SetGravity(T::At(0, 0));
		const auto Wall = T::Body(World, T::At(10, 0), EBodyType::Static);
		auto WallShape = T::BoxShape(0.05f, 3);
		WallShape.Response = Response;
		(void)World.AttachCollider(Wall, WallShape);
		const auto Ball = T::Body(World, T::At(0, 0), EBodyType::Dynamic);
		(void)World.AttachCollider(Ball, T::BallShape(0.2f));
		World.SetContinuous(Ball, true);
		World.SetVelocity(Ball, T::At(900, 0));
		World.Step(StepSeconds);
		const f32 X = T::Side(World.GetPosition(Ball));
		if (Response == EColliderResponse::Solid)
		{
			PHYSICS_REQUIRE(X < 10);
		}
		else
		{
			PHYSICS_REQUIRE(X > 14);
		}
	}
}

// Sensorだけの重なりは休止中のBodyを起こさない。区分の変更は起こし、押し返しの有無が次のStepから変わる。
template <typename T> void SensorKeepsSleepAndChangesApply_Internal()
{
	FDropScene<T> Scene(EColliderResponse::Solid);
	Scene.Run(240);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
	// 眠る球と重なるSensorを別Bodyに置いても起きない。
	const auto Trigger = T::Body(Scene.World, T::At(0, 1), EBodyType::Static);
	auto Shape = T::BallShape(1);
	Shape.Response = EColliderResponse::Sensor;
	const auto TriggerCollider = Scene.World.AttachCollider(Trigger, Shape);
	const auto Position = Scene.World.GetPosition(Scene.Ball);
	Scene.Run(30);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball) && Scene.World.GetPosition(Scene.Ball) == Position);
	PHYSICS_REQUIRE(Scene.World.GetColliderResponse(TriggerCollider) == EColliderResponse::Sensor);
	// 床をSensorへ変えると球は起きて落ちる。
	Scene.World.SetColliderResponse(Scene.FloorCollider, EColliderResponse::Sensor);
	PHYSICS_REQUIRE(!Scene.World.IsSleeping(Scene.Ball));
	Scene.Run(120);
	PHYSICS_REQUIRE(Scene.BallHeight() < -2);
	// 衝突フィルターの変更も次のStepから反映する。
	FDropScene<T> Filtered(EColliderResponse::Solid);
	Filtered.World.SetColliderCollisionFilter(Filtered.FloorCollider, {2u, 2u});
	PHYSICS_REQUIRE(Filtered.World.GetColliderCollisionFilter(Filtered.FloorCollider).Category == 2u);
	Filtered.Run(120);
	PHYSICS_REQUIRE(Filtered.BallHeight() < -5);
	// 無効なID・旧世代・範囲外の値は変更前に拒否する。
	typename T::FWorld Other;
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    Other.SetColliderResponse(Scene.FloorCollider, EColliderResponse::Solid);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    Scene.World.SetColliderResponse(Scene.FloorCollider, static_cast<EColliderResponse>(7));
	    }));
	PHYSICS_REQUIRE(Scene.World.GetColliderResponse(Scene.FloorCollider) == EColliderResponse::Sensor);
	PHYSICS_REQUIRE(Scene.World.DetachCollider(TriggerCollider));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)Scene.World.GetColliderResponse(TriggerCollider);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    Scene.World.SetColliderCollisionFilter(TriggerCollider, {});
	    }));
	auto Bad = T::BallShape(1);
	Bad.Response = static_cast<EColliderResponse>(9);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)Scene.World.AttachCollider(Trigger, Bad);
	    }));
}

// 問い合わせは既定でSolid・Sensorの両方を対象にし、Filterでどちらかだけにできる。
template <typename T> void QueriesSelectResponses_Internal()
{
	typename T::FWorld World;
	const auto Scene = T::Body(World, T::At(0, 0), EBodyType::Static);
	auto SensorShape = T::BallShape(0.5f);
	SensorShape.Response = EColliderResponse::Sensor;
	const auto Trigger = World.AttachCollider(Scene, SensorShape);
	const auto WallBody = T::Body(World, T::At(3, 0), EBodyType::Static);
	const auto Wall = World.AttachCollider(WallBody, T::BoxShape(0.5f, 1));
	const auto Start = T::At(-3, 0);
	const auto End = T::At(6, 0);
	PHYSICS_REQUIRE(World.RaycastClosest(Start, End)->Collider == Trigger);
	FWorldQueryFilter SolidOnly;
	SolidOnly.bIncludeSensors = false;
	PHYSICS_REQUIRE(World.RaycastClosest(Start, End, {}, SolidOnly)->Collider == Wall);
	FWorldQueryFilter SensorsOnly;
	SensorsOnly.bIncludeSolid = false;
	PHYSICS_REQUIRE(World.RaycastClosest(Start, End, {}, SensorsOnly)->Collider == Trigger);
	FWorldQueryFilter Nothing;
	Nothing.bIncludeSolid = false;
	Nothing.bIncludeSensors = false;
	PHYSICS_REQUIRE(!World.RaycastClosest(Start, End, {}, Nothing));
	// 索引の経路と総当たりの参照経路で同じ結果。
	World.SetQueryIndexEnabled_Internal(false);
	PHYSICS_REQUIRE(World.RaycastClosest(Start, End, {}, SolidOnly)->Collider == Wall);
	PHYSICS_REQUIRE(World.RaycastClosest(Start, End, {}, SensorsOnly)->Collider == Trigger);
	PHYSICS_REQUIRE(World.OverlapAll({T::At(0, 0), 0.1f}, {}, SolidOnly).IsEmpty());
	World.SetQueryIndexEnabled_Internal(true);
	PHYSICS_REQUIRE(World.OverlapAll({T::At(0, 0), 0.1f}, {}, SolidOnly).IsEmpty());
	PHYSICS_REQUIRE(World.OverlapAll({T::At(0, 0), 0.1f}).Size() == 1);
}

// キャラクター移動は、呼出し側のFilterにかかわらずSensorを壁・床にしない。
template <typename T> void CharacterIgnoresSensors_Internal()
{
	typename T::FWorld World;
	const auto Ground = T::Body(World, T::At(0, -0.5f), EBodyType::Static);
	(void)World.AttachCollider(Ground, T::BoxShape(50, 0.5f));
	const auto WallBody = T::Body(World, T::At(2, 1), EBodyType::Static);
	auto Wall = T::BoxShape(0.2f, 1);
	Wall.Response = EColliderResponse::Sensor;
	(void)World.AttachCollider(WallBody, Wall);
	// 空中のSensorの板は接地面にならない。
	const auto PlankBody = T::Body(World, T::At(-10, 2), EBodyType::Static);
	auto Plank = T::BoxShape(2, 0.1f);
	Plank.Response = EColliderResponse::Sensor;
	(void)World.AttachCollider(PlankBody, Plank);
	typename T::FSettings Settings;
	typename T::FState State;
	State.Center = T::At(0, 0.5f + static_cast<f32>(Settings.SkinWidth));
	for (int32 Index = 0; Index < 90; ++Index)
	{
		State = StepCharacter(World, Settings, State, T::Walk(1), StepSeconds).State;
	}
	PHYSICS_REQUIRE(T::Side(State.Center) > 4);
	const auto Air = ProbeCharacterGround(World, T::At(-10, 2.65f), Settings);
	PHYSICS_REQUIRE(Air.State == ECharacterGroundState::Airborne);
	// 明示的にSensorを含めるFilterを渡しても同じ。
	FWorldQueryFilter All;
	PHYSICS_REQUIRE(ProbeCharacterGround(World, T::At(-10, 2.65f), Settings, {}, All).State ==
	                ECharacterGroundState::Airborne);
	PHYSICS_REQUIRE(ResolveCharacterOverlap(World, T::At(2, 1), Settings, {}, All).Status ==
	                ECharacterRecoveryStatus::NoOverlap);
}

// 回転した箱のSensorは押し返さず、回転した箱のSolidは押し返す（箱同士・円／球と箱の組）。
template <typename T> void RotatedBoxes_Internal()
{
	for (EColliderResponse Response : {EColliderResponse::Solid, EColliderResponse::Sensor})
	{
		typename T::FWorld World;
		const auto Floor = T::Body(World, T::At(0, 0), EBodyType::Static);
		auto Tilted = T::BoxShape(5, 0.5f, 0.2f);
		Tilted.Response = Response;
		(void)World.AttachCollider(Floor, Tilted);
		const auto Box = T::Body(World, T::At(0, 3), EBodyType::Dynamic);
		(void)World.AttachCollider(Box, T::BoxShape(0.3f, 0.3f));
		for (int32 Index = 0; Index < 90; ++Index)
		{
			World.Step(StepSeconds);
		}
		const f32 Y = T::Height(World.GetPosition(Box));
		PHYSICS_REQUIRE(Response == EColliderResponse::Solid ? Y > 0 : Y < -5);
	}
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"2D sensor has no physical response", &SensorHasNoResponse_Internal<F2D>},
    {"3D sensor has no physical response", &SensorHasNoResponse_Internal<F3D>},
    {"2D collision filter needs both sides", &FilterIsSymmetric_Internal<F2D>},
    {"3D collision filter needs both sides", &FilterIsSymmetric_Internal<F3D>},
    {"2D continuous collision ignores sensors", &ContinuousIgnoresSensors_Internal<F2D>},
    {"3D continuous collision ignores sensors", &ContinuousIgnoresSensors_Internal<F3D>},
    {"2D sensor keeps sleep and response changes apply", &SensorKeepsSleepAndChangesApply_Internal<F2D>},
    {"3D sensor keeps sleep and response changes apply", &SensorKeepsSleepAndChangesApply_Internal<F3D>},
    {"2D queries select solid or sensor", &QueriesSelectResponses_Internal<F2D>},
    {"3D queries select solid or sensor", &QueriesSelectResponses_Internal<F3D>},
    {"2D character ignores sensors", &CharacterIgnoresSensors_Internal<F2D>},
    {"3D character ignores sensors", &CharacterIgnoresSensors_Internal<F3D>},
    {"2D rotated sensor boxes", &RotatedBoxes_Internal<F2D>},
    {"3D rotated sensor boxes", &RotatedBoxes_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetColliderResponseCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
