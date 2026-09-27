// SPDX-License-Identifier: NOASSERTION
// カプセル・Solverの組の候補・押し合い・高さ変更の確保故障注入（S7）。隔離した故障注入exeだけで実行する。
// 確認: カプセルの登録（Collider領域・索引の節点）の各確保を順に失敗させても、部分登録・古い境界・新しいIDを残さない。
// Step中の確保（組の候補の配列の拡張を含む）を順に失敗させても、次の正常なStepで復帰する。
// カプセルのSensorのイベント発行・押す要求・高さの変更（形状の置き換え）は確保しない（途中で失敗し得ない）。
#include "CapsuleAllocationFaultTests.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
#include "../../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
#include <stdio.h>
namespace PhysicsTest
{
namespace
{
// 2Dの型と形状。
struct F2D
{
	using FWorld = Dxf::FPhysicsWorld2D;
	using FBodyDescription = Dxf::FBodyDescription2D;
	using FColliderDescription = Dxf::FColliderDescription2D;
	using FVector = Toolbox::FVector2;
	using FCapsuleShape = Toolbox::FCapsule2D;
	using FSettings = Dxf::FCharacterMoveSettings2D;
	using FState = Dxf::FCharacterState2D;
	using FInput = Dxf::FCharacterMoveInput2D;
	static constexpr const char* Name = "2D";
	static FVector At(Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {X, Y};
	}
	static FColliderDescription Capsule(Toolbox::f32 X, Toolbox::f32 Half, Toolbox::f32 Radius)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FCapsule2D{{X - Half, 0}, {X + Half, 0}, Radius};
		return Description;
	}
	static FColliderDescription Box(Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOrientedBox2D{{0, 0}, {HalfX, HalfY}, 0};
		return Description;
	}
	static FCapsuleShape Standing(Toolbox::f32 Half, Toolbox::f32 Radius)
	{
		return {{0, -Half}, {0, Half}, Radius};
	}
};
// 3Dの型と形状。
struct F3D
{
	using FWorld = Dxf::FPhysicsWorld3D;
	using FBodyDescription = Dxf::FBodyDescription3D;
	using FColliderDescription = Dxf::FColliderDescription3D;
	using FVector = Toolbox::FVector3;
	using FCapsuleShape = Toolbox::FCapsule;
	using FSettings = Dxf::FCharacterMoveSettings3D;
	using FState = Dxf::FCharacterState3D;
	using FInput = Dxf::FCharacterMoveInput3D;
	static constexpr const char* Name = "3D";
	static FVector At(Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {X, Y, 0};
	}
	static FColliderDescription Capsule(Toolbox::f32 X, Toolbox::f32 Half, Toolbox::f32 Radius)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FCapsule{{X - Half, 0, 0}, {X + Half, 0, 0}, Radius};
		return Description;
	}
	static FColliderDescription Box(Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOBB{{0, 0, 0}, {HalfX, HalfY, HalfX}};
		return Description;
	}
	static FCapsuleShape Standing(Toolbox::f32 Half, Toolbox::f32 Radius)
	{
		return {{0, -Half, 0}, {0, Half, 0}, Radius};
	}
};

// 成功・失敗を既存の隔離exeと同じ形式で出力する。
Toolbox::int32 Check(bool bCondition, const char* Dimension, const char* Name)
{
	printf("%s %s capsule allocation %s\n", bCondition ? "PASS" : "FAIL", Dimension, Name);
	fflush(stdout);
	return bCondition ? 0 : 1;
}
// 次の確保を失敗させて処理を行い、確保そのものが起きなかったかを返す。
template <typename F> bool WithoutAllocation(F&& Run)
{
	Toolbox::Testing::SetAllocationFailureCountdown(0);
	bool bSucceeded = true;
	try
	{
		Run();
	}
	catch (const Toolbox::FException&)
	{
		bSucceeded = false;
	}
	const bool bInjected = Toolbox::Testing::WasAllocationFailureInjected();
	Toolbox::Testing::SetAllocationFailureCountdown(-1);
	return bSucceeded && !bInjected;
}

template <typename T> Toolbox::int32 RunDimension()
{
	Toolbox::int32 Failures = 0;
	typename T::FWorld World;
	typename T::FBodyDescription Static;
	Static.Type = Dxf::EBodyType::Static;
	const auto Holder = World.CreateBody(Static);
	// カプセルの登録: 各確保を順に失敗させ、失敗した登録は何も残さない（Collider数・索引・線分の結果が変わらない）。
	Toolbox::int32 Injected = 0;
	bool bClean = true;
	for (Toolbox::int32 Index = 0; Index < 40; ++Index)
	{
		const Toolbox::f32 X = static_cast<Toolbox::f32>(Index * 3);
		for (Toolbox::int64 Countdown = 0;; ++Countdown)
		{
			const auto Before = World.GetQueryDiagnostics();
			Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
			bool bThrew = false;
			Dxf::FWorldQueryFilter Filter;
			try
			{
				(void)World.AttachCollider(Holder, T::Capsule(X, 0.5f, 0.25f));
			}
			catch (const Toolbox::FException&)
			{
				bThrew = true;
			}
			const bool bInjected = Toolbox::Testing::WasAllocationFailureInjected();
			Toolbox::Testing::SetAllocationFailureCountdown(-1);
			if (!bThrew)
			{
				break;
			}
			++Injected;
			const auto After = World.GetQueryDiagnostics();
			// 部分登録なし: 生存数・索引の葉の数が変わらず、その位置の線分は何にも当たらない。
			bClean = bClean && bInjected && After.AliveColliders == Before.AliveColliders &&
			         After.IndexedColliders == Before.IndexedColliders &&
			         !World.RaycastClosest(T::At(X, 5), T::At(X, -5), {}, Filter);
		}
		const auto Hit = World.RaycastClosest(T::At(X, 5), T::At(X, -5));
		bClean = bClean && Hit && World.IsColliderAlive(Hit->Collider);
	}
	Failures += Check(Injected > 0 && bClean, T::Name, "capsule registration and index node");
	// Step中の確保: Dynamicのカプセルを密集させて組の候補を増やし、各確保を順に失敗させる。失敗後も次のStepで復帰する。
	typename T::FWorld Dense;
	const auto Floor = Dense.CreateBody(Static);
	Dense.AttachCollider(Floor, T::Box(50, 0.5f));
	for (Toolbox::int32 Index = 0; Index < 48; ++Index)
	{
		typename T::FBodyDescription Body;
		Body.Position = T::At(static_cast<Toolbox::f32>(Index % 8) * 0.9f, 1.0f + static_cast<Toolbox::f32>(Index / 8));
		const auto Id = Dense.CreateBody(Body);
		Dense.AttachCollider(Id, T::Capsule(0, 0.3f, 0.3f));
	}
	Toolbox::int32 StepInjected = 0;
	bool bRecovered = true;
	for (Toolbox::int64 Countdown = 0; Countdown < 64; ++Countdown)
	{
		Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
		bool bThrew = false;
		try
		{
			Dense.Step(1.0 / 60.0);
		}
		catch (const Toolbox::FException&)
		{
			bThrew = true;
		}
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
		if (!bThrew)
		{
			break;
		}
		++StepInjected;
		// 途中で失敗したStepの後は問い合わせを拒否し、次の正常なStepで受け付ける。
		bool bRejected = false;
		try
		{
			(void)Dense.RaycastClosest(T::At(0, 10), T::At(0, -10));
		}
		catch (const Toolbox::FException&)
		{
			bRejected = true;
		}
		Dense.Step(1.0 / 60.0);
		const auto Snapshot = Dense.CaptureSnapshot();
		bool bFinite = true;
		for (const auto& Body : Snapshot.Bodies)
		{
			bFinite = bFinite && Body.Position.IsValid() && Body.Velocity.IsValid();
		}
		bRecovered = bRecovered && bRejected && bFinite && Dense.RaycastClosest(T::At(0, 10), T::At(0, -10));
	}
	Failures += Check(StepInjected > 0 && bRecovered, T::Name, "step and pair buffer growth");
	// カプセルのSensorのイベント: 有効化・慣らしの後は、発行まで確保しない。
	typename T::FWorld Events;
	Dxf::FWorldEventSettings Settings;
	Settings.bEnabled = true;
	Events.SetEventSettings(Settings);
	typename T::FBodyDescription Kinematic;
	Kinematic.Type = Dxf::EBodyType::Kinematic;
	for (Toolbox::int32 Index = 0; Index < 6; ++Index)
	{
		const auto Id = Events.CreateBody(Kinematic);
		auto Shape = T::Capsule(0, 0.5f, 0.4f);
		Shape.Response = Dxf::EColliderResponse::Sensor;
		Events.AttachCollider(Id, Shape);
	}
	Events.Step(1.0 / 60.0);
	const bool bNoEventAllocation = WithoutAllocation(
	    [&]
	    {
		    Events.Step(1.0 / 60.0);
	    });
	Failures += Check(bNoEventAllocation && Events.GetEventBatch().bPublished && Events.GetEventBatch().PairCount == 15,
	                  T::Name, "capsule sensor event publish");
	// 押す要求と高さの変更: 計算と形状の置き換えは確保しない（Impulseだけが適用された中途半端な状態はない）。
	typename T::FWorld Push;
	const auto Ground = Push.CreateBody(Static);
	Push.AttachCollider(Ground, T::Box(50, 0.5f));
	typename T::FBodyDescription Crate;
	Crate.Position = T::At(1.2f, 1);
	const auto CrateId = Push.CreateBody(Crate);
	Push.AttachCollider(CrateId, T::Box(0.5f, 0.5f));
	Push.Step(1.0 / 60.0);
	typename T::FSettings Character;
	Character.Shape = Dxf::ECharacterShape::Capsule;
	Character.HalfHeight = 0.4;
	Character.bPushDynamicBodies = true;
	typename T::FState State;
	State.Center = T::At(0, 1.42f);
	typename T::FInput Input;
	Input.Move = T::At(1, 0);
	Toolbox::uint32 Pushes = 0;
	bool bPushWithoutAllocation = true;
	for (Toolbox::int32 Step = 0; Step < 10; ++Step)
	{
		bPushWithoutAllocation = bPushWithoutAllocation && WithoutAllocation(
		                                                       [&]
		                                                       {
			                                                       const auto Result = Dxf::StepCharacter(
			                                                           Push, Character, State, Input, 1.0 / 60.0);
			                                                       State = Result.State;
			                                                       Pushes += Result.Pushes.Count;
		                                                       });
		Push.Step(1.0 / 60.0);
	}
	Failures += Check(bPushWithoutAllocation && Pushes > 0, T::Name, "push requests");
	const auto Body = Push.CreateBody(Kinematic);
	typename T::FColliderDescription Tall;
	Tall.Shape = T::Standing(0.4f, 0.5f);
	const auto Collider = Push.AttachCollider(Body, Tall);
	bool bResized = false;
	const bool bResizeWithoutAllocation = WithoutAllocation(
	    [&]
	    {
		    const auto Resize = Dxf::ResizeCharacterCapsule(Push, State.Center, Character, 0.1);
		    Push.SetColliderShape(Collider, T::Standing(0.1f, 0.5f));
		    bResized = Resize.bResized;
	    });
	Failures += Check(bResizeWithoutAllocation && bResized && Push.IsColliderAlive(Collider), T::Name, "height change");
	return Failures;
}
} // namespace

Toolbox::int32 RunCapsuleAllocationChecks()
{
	return RunDimension<F2D>() + RunDimension<F3D>();
}
} // namespace PhysicsTest
