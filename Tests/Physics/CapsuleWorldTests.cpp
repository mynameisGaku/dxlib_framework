// SPDX-License-Identifier: NOASSERTION
// カプセルのColliderをPhysics Worldへ統合した回帰（S2）。同じ契約を実FPhysicsWorld2D／FPhysicsWorld3Dの両方で実行する。
// 期待値は配置から求めた静止高さ・線分の割合・符号付き距離と、索引を使わない総当たりの参照経路との一致。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr f64 StepSeconds = 1.0 / 60.0;
// 床の上面の高さ（中心y=0、半高0.5）。
constexpr f32 FloorTop = 0.5f;

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

// 2D Worldの型と登録操作（Yが上、カプセルは中心線をX軸に沿わせる）。
struct F2D
{
	using FWorld = FPhysicsWorld2D;
	using FBodyId = FBodyId2D;
	using FColliderId = FColliderId2D;
	using FVector = FVector2;
	using FDescription = FColliderDescription2D;
	using FCapsuleShape = FCapsule2D;
	using FBall = FCircle2D;
	static FVector At(f32 X, f32 Y)
	{
		return {X, Y};
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type, f32 Tilt = 0)
	{
		FBodyDescription2D Description;
		Description.Type = Type;
		Description.Position = Position;
		Description.Angle = Tilt;
		return World.CreateBody(Description);
	}
	// 中心線がX軸に沿う（半長Half）カプセル。
	static FCapsuleShape Lying(f32 Half, f32 Radius)
	{
		return {{-Half, 0}, {Half, 0}, Radius};
	}
	// 中心線がY軸に沿う（半高Half）カプセル。
	static FCapsuleShape Standing(f32 Half, f32 Radius)
	{
		return {{0, -Half}, {0, Half}, Radius};
	}
	static FDescription Capsule(const FCapsuleShape& Shape)
	{
		FDescription Description;
		Description.Shape = Shape;
		return Description;
	}
	static FDescription Box(f32 HalfX, f32 HalfY)
	{
		FDescription Description;
		Description.Shape = FOrientedBox2D{{0, 0}, {HalfX, HalfY}, 0};
		return Description;
	}
	static FDescription Ball(f32 Radius)
	{
		FDescription Description;
		Description.Shape = FCircle2D{{0, 0}, Radius};
		return Description;
	}
	static FBall BallAt(FVector Center, f32 Radius)
	{
		return {Center, Radius};
	}
	// 本体のX軸がWorldのXから傾いた量（正弦）。
	static f64 TiltSine(const FWorld& World, FBodyId Id)
	{
		return Sin(f64(World.GetAngle(Id)));
	}
	static f64 Spin(const FWorld& World, FBodyId Id)
	{
		return Abs(f64(World.GetAngularVelocity(Id)));
	}
	static void Continuous(FWorld& World)
	{
		FContinuousSettings2D Settings;
		Settings.bEnabled = true;
		World.SetContinuousSettings(Settings);
	}
	static const FCapsuleShape* LocalCapsule(const FPhysicsSnapshot2D& Snapshot, const FColliderId& Id)
	{
		for (const auto& Collider : Snapshot.Colliders)
		{
			if (Collider.Id == Id && Collider.LocalShape.Index() == 2)
			{
				return &Collider.LocalShape.template Get<2>();
			}
		}
		return nullptr;
	}
};

// 3D Worldの型と登録操作（Yが上、カプセルは中心線をX軸に沿わせる）。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	using FVector = FVector3;
	using FDescription = FColliderDescription3D;
	using FCapsuleShape = FCapsule;
	using FBall = FSphere;
	static FVector At(f32 X, f32 Y)
	{
		return {X, Y, 0};
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type, f32 Tilt = 0)
	{
		FBodyDescription3D Description;
		Description.Type = Type;
		Description.Position = Position;
		// Z軸回りの回転（2DのAngleと同じ向き）。
		Description.Orientation = {0, 0, static_cast<f32>(Sin(f64(Tilt) * 0.5)),
		                           static_cast<f32>(Cos(f64(Tilt) * 0.5))};
		return World.CreateBody(Description);
	}
	static FCapsuleShape Lying(f32 Half, f32 Radius)
	{
		return {{-Half, 0, 0}, {Half, 0, 0}, Radius};
	}
	static FCapsuleShape Standing(f32 Half, f32 Radius)
	{
		return {{0, -Half, 0}, {0, Half, 0}, Radius};
	}
	static FDescription Capsule(const FCapsuleShape& Shape)
	{
		FDescription Description;
		Description.Shape = Shape;
		return Description;
	}
	static FDescription Box(f32 HalfX, f32 HalfY)
	{
		FDescription Description;
		Description.Shape = FOBB{{0, 0, 0}, {HalfX, HalfY, HalfX}};
		return Description;
	}
	static FDescription Ball(f32 Radius)
	{
		FDescription Description;
		Description.Shape = FSphere{{0, 0, 0}, Radius};
		return Description;
	}
	static FBall BallAt(FVector Center, f32 Radius)
	{
		return {Center, Radius};
	}
	static f64 TiltSine(const FWorld& World, FBodyId Id)
	{
		return World.GetOrientation(Id).Rotate({1, 0, 0}).Y;
	}
	static f64 Spin(const FWorld& World, FBodyId Id)
	{
		return Length(World.GetAngularVelocity(Id));
	}
	static void Continuous(FWorld& World)
	{
		FContinuousSettings3D Settings;
		Settings.bEnabled = true;
		World.SetContinuousSettings(Settings);
	}
	static const FCapsuleShape* LocalCapsule(const FPhysicsSnapshot3D& Snapshot, const FColliderId& Id)
	{
		for (const auto& Collider : Snapshot.Colliders)
		{
			if (Collider.Id == Id && Collider.LocalShape.Index() == 2)
			{
				return &Collider.LocalShape.template Get<2>();
			}
		}
		return nullptr;
	}
};

// 静止した床（中心y=0、上面y=0.5、半幅20）。
template <typename T>
typename T::FBodyId Floor_Internal(typename T::FWorld& World, f32 Restitution = 0, f32 Friction = 0.5f)
{
	const auto Floor = T::Body(World, T::At(0, 0), EBodyType::Static);
	auto Description = T::Box(20, 0.5f);
	Description.Restitution = Restitution;
	Description.Friction = Friction;
	World.AttachCollider(Floor, Description);
	return Floor;
}
template <typename T> void Run_Internal(typename T::FWorld& World, Toolbox::int32 Steps)
{
	for (Toolbox::int32 Index = 0; Index < Steps; ++Index)
	{
		World.Step(StepSeconds);
	}
}

// 横倒しのカプセルは床の上で半径の高さに静止し、傾かず、やがて休止する。立てたカプセルは半高＋半径で静止する。
template <typename T> void RestOnFloor_Internal()
{
	typename T::FWorld World;
	Floor_Internal<T>(World);
	const auto Lying = T::Body(World, T::At(-3, 2), EBodyType::Dynamic);
	World.AttachCollider(Lying, T::Capsule(T::Lying(0.6f, 0.3f)));
	const auto Standing = T::Body(World, T::At(3, 2), EBodyType::Dynamic);
	World.AttachCollider(Standing, T::Capsule(T::Standing(0.4f, 0.25f)));
	Run_Internal<T>(World, 240);
	PHYSICS_REQUIRE(Abs(f64(World.GetPosition(Lying).Y) - (FloorTop + 0.3)) < 0.02);
	PHYSICS_REQUIRE(Abs(T::TiltSine(World, Lying)) < 1e-3);
	PHYSICS_REQUIRE(Abs(f64(World.GetPosition(Standing).Y) - (FloorTop + 0.4 + 0.25)) < 0.02);
	Run_Internal<T>(World, 120);
	PHYSICS_REQUIRE(World.IsSleeping(Lying));
}

// 傾いて落ちたカプセルは片端が先に当たって回り、横倒しで静止する（回転と接触の腕）。
template <typename T> void TiltedFallRotates_Internal()
{
	typename T::FWorld World;
	Floor_Internal<T>(World);
	const auto Body = T::Body(World, T::At(0, 2), EBodyType::Dynamic, 0.5f);
	World.AttachCollider(Body, T::Capsule(T::Lying(0.6f, 0.3f)));
	f64 MaxSpin = 0;
	for (Toolbox::int32 Index = 0; Index < 300; ++Index)
	{
		World.Step(StepSeconds);
		MaxSpin = Max(MaxSpin, T::Spin(World, Body));
	}
	PHYSICS_REQUIRE(MaxSpin > 0.5);
	PHYSICS_REQUIRE(Abs(T::TiltSine(World, Body)) < 0.02);
	PHYSICS_REQUIRE(Abs(f64(World.GetPosition(Body).Y) - (FloorTop + 0.3)) < 0.02);
}

// 平行なカプセルの積み重ねと、Dynamicの箱の上のカプセル（カプセル同士・カプセルと箱の組）。
template <typename T> void StackAndBox_Internal()
{
	typename T::FWorld World;
	Floor_Internal<T>(World);
	const auto Bottom = T::Body(World, T::At(0, 1), EBodyType::Dynamic);
	World.AttachCollider(Bottom, T::Capsule(T::Lying(0.8f, 0.3f)));
	const auto Top = T::Body(World, T::At(0, 2), EBodyType::Dynamic);
	World.AttachCollider(Top, T::Capsule(T::Lying(0.8f, 0.3f)));
	const auto Crate = T::Body(World, T::At(5, 1), EBodyType::Dynamic);
	World.AttachCollider(Crate, T::Box(0.5f, 0.5f));
	const auto OnCrate = T::Body(World, T::At(5, 2.5f), EBodyType::Dynamic);
	World.AttachCollider(OnCrate, T::Capsule(T::Lying(0.3f, 0.2f)));
	Run_Internal<T>(World, 300);
	PHYSICS_REQUIRE(Abs(f64(World.GetPosition(Bottom).Y) - (FloorTop + 0.3)) < 0.03);
	PHYSICS_REQUIRE(Abs(f64(World.GetPosition(Top).Y) - (FloorTop + 0.9)) < 0.04);
	PHYSICS_REQUIRE(Abs(f64(World.GetPosition(Top).X)) < 0.05);
	PHYSICS_REQUIRE(Abs(f64(World.GetPosition(Crate).Y) - (FloorTop + 0.5)) < 0.03);
	PHYSICS_REQUIRE(Abs(f64(World.GetPosition(OnCrate).Y) - (FloorTop + 1.0 + 0.2)) < 0.04);
}

// 軸方向に滑るカプセルは摩擦で止まり、摩擦0では速度を保つ。反発1の床では跳ね返り、反発0では跳ねない。
template <typename T> void FrictionAndRestitution_Internal()
{
	f64 Speeds[2]{};
	for (Toolbox::int32 Case = 0; Case < 2; ++Case)
	{
		typename T::FWorld World;
		Floor_Internal<T>(World, 0, Case == 0 ? 0.5f : 0.0f);
		const auto Body = T::Body(World, T::At(0, FloorTop + 0.3f), EBodyType::Dynamic);
		auto Description = T::Capsule(T::Lying(0.6f, 0.3f));
		Description.Friction = Case == 0 ? 0.5f : 0.0f;
		World.AttachCollider(Body, Description);
		World.SetVelocity(Body, T::At(3, 0));
		Run_Internal<T>(World, 120);
		Speeds[Case] = World.GetVelocity(Body).X;
	}
	PHYSICS_REQUIRE(Abs(Speeds[0]) < 0.05);
	PHYSICS_REQUIRE(Abs(Speeds[1] - 3) < 0.05);
	f64 Rebound[2]{};
	for (Toolbox::int32 Case = 0; Case < 2; ++Case)
	{
		typename T::FWorld World;
		const f32 Restitution = Case == 0 ? 1.0f : 0.0f;
		Floor_Internal<T>(World, Restitution);
		const auto Body = T::Body(World, T::At(0, 2), EBodyType::Dynamic);
		auto Description = T::Capsule(T::Lying(0.6f, 0.3f));
		Description.Restitution = Restitution;
		World.AttachCollider(Body, Description);
		for (Toolbox::int32 Index = 0; Index < 90; ++Index)
		{
			World.Step(StepSeconds);
			Rebound[Case] = Max(Rebound[Case], f64(World.GetVelocity(Body).Y));
		}
	}
	// 落下速度は約5.1m/s。跳ね返りは半分以上、反発0は上向きの速度をほぼ持たない。
	PHYSICS_REQUIRE(Rebound[0] > 2.5);
	PHYSICS_REQUIRE(Rebound[1] < 0.3);
}

// Kinematicのカプセルは箱を押し、Sensorのカプセルは押さずにTriggerの開始・継続・終了を出す。Solidは接触のイベントを出す。
template <typename T> void KinematicSensorEvents_Internal()
{
	typename T::FWorld World;
	Floor_Internal<T>(World);
	FWorldEventSettings Events;
	Events.bEnabled = true;
	World.SetEventSettings(Events);
	const auto Pusher = T::Body(World, T::At(-3, FloorTop + 0.6f), EBodyType::Kinematic);
	World.AttachCollider(Pusher, T::Capsule(T::Standing(0.3f, 0.3f)));
	World.SetVelocity(Pusher, T::At(2, 0));
	const auto Crate = T::Body(World, T::At(-1.5f, FloorTop + 0.5f), EBodyType::Dynamic);
	const auto CrateCollider = World.AttachCollider(Crate, T::Box(0.5f, 0.5f));
	const auto Gate = T::Body(World, T::At(6, FloorTop + 3), EBodyType::Static);
	auto GateDescription = T::Capsule(T::Standing(1, 0.5f));
	GateDescription.Response = EColliderResponse::Sensor;
	const auto GateCollider = World.AttachCollider(Gate, GateDescription);
	const auto Ball = T::Body(World, T::At(3, FloorTop + 3), EBodyType::Kinematic);
	const auto BallCollider = World.AttachCollider(Ball, T::Ball(0.25f));
	World.SetVelocity(Ball, T::At(3, 0));
	bool bContact = false;
	bool bBegin = false;
	bool bStay = false;
	bool bEnd = false;
	for (Toolbox::int32 Index = 0; Index < 120; ++Index)
	{
		World.Step(StepSeconds);
		for (const auto& Event : World.GetEventBatch().Events)
		{
			const bool bGatePair = (Event.ColliderA == GateCollider && Event.ColliderB == BallCollider) ||
			                       (Event.ColliderA == BallCollider && Event.ColliderB == GateCollider);
			if (bGatePair && Event.Kind == EWorldEventKind::Trigger)
			{
				bBegin = bBegin || Event.Phase == EWorldEventPhase::Begin;
				bStay = bStay || Event.Phase == EWorldEventPhase::Stay;
				bEnd = bEnd || Event.Phase == EWorldEventPhase::End;
			}
			if ((Event.ColliderA == CrateCollider || Event.ColliderB == CrateCollider) &&
			    Event.Kind == EWorldEventKind::Contact && Event.Phase == EWorldEventPhase::Begin)
			{
				bContact = true;
			}
		}
	}
	// 押す側の速度（2m/s×2秒）で箱は大きく前へ進み、Sensorを抜けたボールは速度を保つ。
	PHYSICS_REQUIRE(World.GetPosition(Crate).X > 1.5f);
	PHYSICS_REQUIRE(Abs(f64(World.GetVelocity(Ball).X) - 3) < 1e-4);
	PHYSICS_REQUIRE(bContact && bBegin && bStay && bEnd);
}

// 問い合わせ：線分・範囲・接触・移動（球とカプセル）。索引を使わない総当たりと結果が一致する。
template <typename T> void Queries_Internal()
{
	typename T::FWorld World;
	Floor_Internal<T>(World);
	const auto Body = T::Body(World, T::At(0, 3), EBodyType::Static);
	const auto Collider = World.AttachCollider(Body, T::Capsule(T::Lying(1, 0.5f)));
	for (Toolbox::int32 Pass = 0; Pass < 2; ++Pass)
	{
		World.SetQueryIndexEnabled_Internal(Pass == 0);
		// 胴体を上から：y=3.5で、割合(5−3.5)/4=0.375。
		const auto Ray = World.RaycastClosest(T::At(0.3f, 5), T::At(0.3f, 1));
		PHYSICS_REQUIRE(Ray && Ray->Collider == Collider && Abs(Ray->Fraction - 0.375) < 1e-5);
		// 端の球の外側を通る線分は当たらない。
		PHYSICS_REQUIRE(!World.RaycastClosest(T::At(1.6f, 5), T::At(1.6f, 3.1f)));
		// 球の範囲：胴体の上0.1離れた球は重ならず、半径を0.2増やすと重なる。
		PHYSICS_REQUIRE(World.OverlapAll(T::BallAt(T::At(0, 4.1f), 0.5f)).IsEmpty());
		PHYSICS_REQUIRE(World.OverlapAll(T::BallAt(T::At(0, 4.1f), 0.7f)).Size() == 1);
		// 球の接触：端(1,3)から(2.5,3)まで1.5、距離1.5−0.5−0.25=0.75、法線は+X。
		const auto Contacts = World.QueryContacts(T::BallAt(T::At(2.5f, 3), 0.25f), 1.0);
		PHYSICS_REQUIRE(Contacts.Count == 1 && Abs(Contacts.Items[0].Separation - 0.75) < 1e-5);
		PHYSICS_REQUIRE(Contacts.Items[0].Normal && Abs(f64(Contacts.Items[0].Normal->X) - 1) < 1e-5);
		// 球の移動：上から落とすと中心y=3.75で接触、割合(6−3.75)/4。
		const auto Sweep = World.SweepClosest(T::BallAt(T::At(0, 6), 0.25f), T::At(0, 2));
		PHYSICS_REQUIRE(Sweep && Sweep->Collider == Collider && Abs(Sweep->Fraction - 2.25 / 4) < 1e-5);
		// カプセルの接触：宙に浮いたカプセルは何にも触れず、床の上0.2（下面y=0.7）のカプセルは床との距離0.2だけが返る。
		typename T::FCapsuleShape High = T::Lying(0.5f, 0.2f);
		High.Start = High.Start + T::At(5, 5);
		High.End = High.End + T::At(5, 5);
		PHYSICS_REQUIRE(World.QueryCapsuleContacts(High, 0.3).Count == 0);
		typename T::FCapsuleShape Low = T::Lying(0.5f, 0.2f);
		Low.Start = Low.Start + T::At(5, 0.9f);
		Low.End = Low.End + T::At(5, 0.9f);
		const auto LowContacts = World.QueryCapsuleContacts(Low, 0.3);
		PHYSICS_REQUIRE(LowContacts.Count == 1 && Abs(LowContacts.Items[0].Separation - 0.2) < 1e-5);
		// カプセルの移動：中点(5,0.9)から(5,−3)へ。下端が床に当たるのは0.2進んだ時。
		const auto Drop = World.SweepCapsuleClosest(Low, T::At(5, -3));
		PHYSICS_REQUIRE(Drop && !Drop->bInitialContact && Abs(Drop->Fraction - 0.2 / 3.9) < 1e-4);
		PHYSICS_REQUIRE(Drop->Normal && Abs(f64(Drop->Normal->Y) - 1) < 1e-4);
		PHYSICS_REQUIRE(Abs(f64(Drop->CenterAtHit.Y) - 0.7) < 1e-3);
		// 開始時に接触している対象は、除く問い合わせでは候補にならない。
		typename T::FCapsuleShape Touching = Low;
		Touching.Start = Touching.Start - T::At(0, 0.2f);
		Touching.End = Touching.End - T::At(0, 0.2f);
		PHYSICS_REQUIRE(World.SweepCapsuleClosest(Touching, T::At(5, -3))->bInitialContact);
		PHYSICS_REQUIRE(!World.SweepCapsuleClosestIgnoringInitialContacts(Touching, T::At(5, -3)));
		// カプセルの範囲：浮いたカプセルと重なる。
		typename T::FCapsuleShape Probe = T::Lying(0.5f, 0.1f);
		Probe.Start = Probe.Start + T::At(1.4f, 3);
		Probe.End = Probe.End + T::At(1.4f, 3);
		const auto Overlaps = World.OverlapCapsuleAll(Probe);
		PHYSICS_REQUIRE(Overlaps.Size() == 1 && Overlaps[0] == Collider);
		// 不正な形状は例外。
		PHYSICS_REQUIRE(Throws_Internal(
		    [&]
		    {
			    (void)World.QueryCapsuleContacts(typename T::FCapsuleShape{T::At(0, 0), T::At(1, 0), -1}, 0);
		    }));
		PHYSICS_REQUIRE(Throws_Internal(
		    [&]
		    {
			    (void)World.SweepCapsuleClosestIgnoringInitialContacts(T::Lying(1, 0), T::At(0, 0));
		    }));
	}
}

// 形状の置き換え（高さの変更）：IDは生きたまま、問い合わせ・Snapshotへ直ちに反映する。不正な形状は拒否して元の形状を保つ。
// 取り外し・再登録は世代で区別し、カプセルの組の連続衝突は対象外として明示する。
template <typename T> void ShapeChangeAndLifetime_Internal()
{
	typename T::FWorld World;
	const auto Floor = Floor_Internal<T>(World);
	const auto Body = T::Body(World, T::At(0, FloorTop + 1), EBodyType::Kinematic);
	const auto Collider = World.AttachCollider(Body, T::Capsule(T::Standing(0.5f, 0.3f)));
	PHYSICS_REQUIRE(Abs(World.RaycastClosest(T::At(0, 5), T::At(0, 0))->Fraction - (5 - (FloorTop + 1.8)) / 5) < 1e-5);
	World.SetColliderShape(Collider, T::Standing(0.2f, 0.3f));
	PHYSICS_REQUIRE(World.IsColliderAlive(Collider));
	PHYSICS_REQUIRE(Abs(World.RaycastClosest(T::At(0, 5), T::At(0, 0))->Fraction - (5 - (FloorTop + 1.5)) / 5) < 1e-5);
	const auto Snapshot = World.CaptureSnapshot();
	const auto* Local = T::LocalCapsule(Snapshot, Collider);
	PHYSICS_REQUIRE(Local != nullptr && Local->End == T::Standing(0.2f, 0.3f).End);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.SetColliderShape(Collider, typename T::FCapsuleShape{T::At(0, 0), T::At(0, 1), -0.5f});
	    }));
	PHYSICS_REQUIRE(World.GetColliderShape(Collider).template Get<2>().End == T::Standing(0.2f, 0.3f).End);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.AttachCollider(Body, T::Capsule(typename T::FCapsuleShape{T::At(0, NAN), T::At(0, 1), 0.5f}));
	    }));
	// 連続衝突の対応：カプセルを含む組は対象外。
	const auto FloorCollider = World.OverlapAll(T::BallAt(T::At(0, 0), 0.1f))[0];
	PHYSICS_REQUIRE(World.QueryContinuousSupport(Collider, FloorCollider) == EContinuousSupport::UnsupportedPair);
	(void)Floor;
	// 取り外すと古いIDは無効で、同じスロットの再登録は世代で区別する。
	PHYSICS_REQUIRE(World.DetachCollider(Collider));
	PHYSICS_REQUIRE(!World.IsColliderAlive(Collider));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.SetColliderShape(Collider, T::Standing(0.2f, 0.3f));
	    }));
	const auto Again = World.AttachCollider(Body, T::Capsule(T::Standing(0.5f, 0.3f)));
	PHYSICS_REQUIRE(Again.Index == Collider.Index && Again.Generation != Collider.Generation);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0, 5), T::At(0, 0))->Collider == Again);
	PHYSICS_REQUIRE(World.DestroyBody(Body));
	PHYSICS_REQUIRE(!World.IsColliderAlive(Again));
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0, 5), T::At(0, 0))->Collider == FloorCollider);
}

// 連続衝突を有効にしたDynamicのカプセルも、離散の接触で床に止まる（カプセルの組はCCDの候補から外れる）。
template <typename T> void ContinuousFallsBackToDiscrete_Internal()
{
	typename T::FWorld World;
	T::Continuous(World);
	Floor_Internal<T>(World);
	const auto Body = T::Body(World, T::At(0, 2), EBodyType::Dynamic);
	World.AttachCollider(Body, T::Capsule(T::Lying(0.6f, 0.3f)));
	World.SetContinuous(Body, true);
	// 診断はStepごとに作り直すため、落下中に床の組を離散へ回したStepがあるかを数える。
	Toolbox::uint32 FallbackSteps = 0;
	for (Toolbox::int32 Index = 0; Index < 180; ++Index)
	{
		World.Step(StepSeconds);
		FallbackSteps += World.GetContinuousDiagnostics().FallbackPairs > 0 ? 1 : 0;
	}
	PHYSICS_REQUIRE(Abs(f64(World.GetPosition(Body).Y) - (FloorTop + 0.3)) < 0.02);
	PHYSICS_REQUIRE(FallbackSteps > 0);
}

// 形状の置き換えで休止中の剛体は起き、支えを失った剛体は落ちる。
template <typename T> void ShapeChangeWakes_Internal()
{
	typename T::FWorld World;
	Floor_Internal<T>(World);
	const auto Body = T::Body(World, T::At(0, 2), EBodyType::Dynamic);
	const auto Collider = World.AttachCollider(Body, T::Capsule(T::Standing(0.5f, 0.3f)));
	Run_Internal<T>(World, 300);
	PHYSICS_REQUIRE(World.IsSleeping(Body));
	const f32 Before = World.GetPosition(Body).Y;
	World.SetColliderShape(Collider, T::Standing(0.2f, 0.3f));
	PHYSICS_REQUIRE(!World.IsSleeping(Body));
	Run_Internal<T>(World, 120);
	PHYSICS_REQUIRE(Abs(f64(Before - World.GetPosition(Body).Y) - 0.3) < 0.03);
}

// 2Dと3Dで同じ配置（Z=0の平面）の静止高さと線分の割合が一致する。
void Equivalence_Internal()
{
	F2D::FWorld World2D;
	F3D::FWorld World3D;
	Floor_Internal<F2D>(World2D);
	Floor_Internal<F3D>(World3D);
	const auto Body2D = F2D::Body(World2D, F2D::At(0, 2), EBodyType::Dynamic);
	World2D.AttachCollider(Body2D, F2D::Capsule(F2D::Lying(0.6f, 0.3f)));
	const auto Body3D = F3D::Body(World3D, F3D::At(0, 2), EBodyType::Dynamic);
	World3D.AttachCollider(Body3D, F3D::Capsule(F3D::Lying(0.6f, 0.3f)));
	Run_Internal<F2D>(World2D, 180);
	Run_Internal<F3D>(World3D, 180);
	PHYSICS_REQUIRE(Abs(f64(World2D.GetPosition(Body2D).Y) - World3D.GetPosition(Body3D).Y) < 0.01);
	const auto Ray2D = World2D.RaycastClosest(F2D::At(0.2f, 5), F2D::At(0.2f, 0));
	const auto Ray3D = World3D.RaycastClosest(F3D::At(0.2f, 5), F3D::At(0.2f, 0));
	PHYSICS_REQUIRE(Ray2D && Ray3D && Abs(Ray2D->Fraction - Ray3D->Fraction) < 1e-3);
}

void Rest2D_Internal()
{
	RestOnFloor_Internal<F2D>();
	TiltedFallRotates_Internal<F2D>();
	StackAndBox_Internal<F2D>();
}
void Rest3D_Internal()
{
	RestOnFloor_Internal<F3D>();
	TiltedFallRotates_Internal<F3D>();
	StackAndBox_Internal<F3D>();
}
void Material2D_Internal()
{
	FrictionAndRestitution_Internal<F2D>();
	KinematicSensorEvents_Internal<F2D>();
}
void Material3D_Internal()
{
	FrictionAndRestitution_Internal<F3D>();
	KinematicSensorEvents_Internal<F3D>();
}
void Query2D_Internal()
{
	Queries_Internal<F2D>();
}
void Query3D_Internal()
{
	Queries_Internal<F3D>();
}
void Shape2D_Internal()
{
	ShapeChangeAndLifetime_Internal<F2D>();
	ContinuousFallsBackToDiscrete_Internal<F2D>();
	ShapeChangeWakes_Internal<F2D>();
}
void Shape3D_Internal()
{
	ShapeChangeAndLifetime_Internal<F3D>();
	ContinuousFallsBackToDiscrete_Internal<F3D>();
	ShapeChangeWakes_Internal<F3D>();
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"2D capsule rests, rotates and stacks", &Rest2D_Internal},
    {"3D capsule rests, rotates and stacks", &Rest3D_Internal},
    {"2D capsule friction, restitution, kinematic push and events", &Material2D_Internal},
    {"3D capsule friction, restitution, kinematic push and events", &Material3D_Internal},
    {"2D capsule queries match the brute-force path", &Query2D_Internal},
    {"3D capsule queries match the brute-force path", &Query3D_Internal},
    {"2D capsule shape change, lifetime and continuous fallback", &Shape2D_Internal},
    {"3D capsule shape change, lifetime and continuous fallback", &Shape3D_Internal},
    {"2D and 3D capsule worlds agree", &Equivalence_Internal}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetCapsuleWorldCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
