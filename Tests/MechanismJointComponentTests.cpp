// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/JointTargetMotion.h"
#include "Support/FakeBackend.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/RevoluteJointComponent2D.h"
#include "Dxf/FixedJointComponent2D.h"
#include "Dxf/PrismaticJointComponent2D.h"
#include "Dxf/RevoluteJointComponent3D.h"
#include "Dxf/FixedJointComponent3D.h"
#include "Dxf/PrismaticJointComponent3D.h"
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
// 次元ごとの実PhysicsSceneと剛体を選ぶ。
struct FMechanismFixture2D
{
	using SceneType = DPhysicsScene2D;
	using BodyDescription = FBodyDescription2D;
	using Body = DRigidBody2DComponent;
	using Mover = DKinematicMover2DComponent;
	using MoverDescription = FKinematicMoverDescription2D;
	using Reference = FPhysicsBodyReference2D;
};
// 次元ごとの実PhysicsSceneと剛体を選ぶ。
struct FMechanismFixture3D
{
	using SceneType = DPhysicsScene3D;
	using BodyDescription = FBodyDescription3D;
	using Body = DRigidBody3DComponent;
	using Mover = DKinematicMover3DComponent;
	using MoverDescription = FKinematicMoverDescription3D;
	using Reference = FPhysicsBodyReference3D;
};
// 描画・音声だけを置換し、WorldとComponentは製品コードで通す。
template <typename T, typename J, typename D>
struct TMechanismFixture
{
	FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FAudioPlayer Audio{Backend};
	FInputStateTracker Input;
	typename T::SceneType Scene;
	TObjectHandle<typename T::Body> A;
	TObjectHandle<typename T::Body> B;
	TObjectHandle<J> Joint;
	TMechanismFixture()
	{
		const auto Left = Scene.template Spawn<DGameObject>().Value();
		const auto Right = Scene.template Spawn<DGameObject>().Value();
		const auto Link = Scene.template Spawn<DGameObject>().Value();
		typename T::BodyDescription Static;
		Static.Type = EBodyType::Static;
		A = Left.Get()->template AddComponent<typename T::Body>(Static).Value();
		B = Right.Get()->template AddComponent<typename T::Body>(typename T::BodyDescription{}).Value();
		D Description;
		Description.BodyA = T::Reference::FromRigidBody(A);
		Description.BodyB = T::Reference::FromRigidBody(B);
		Joint = Link.Get()->template AddComponent<J>(Description).Value();
		Link.Get()->SetUpdateOrder(0);
		Left.Get()->SetUpdateOrder(2);
		Right.Get()->SetUpdateOrder(3);
		Scene.GetPhysicsWorld().SetGravity({});
		REQUIRE(Scene.Initialize_Internal({Assets}));
	}
	~TMechanismFixture()
	{
		Scene.Shutdown_Internal();
	}
	TResult<void> Tick(Toolbox::f64 Seconds = 1.0 / 60.0)
	{
		Scene.GetChildren_Internal()->FreezeBoundary_Internal();
		const auto Committed = Scene.GetChildren_Internal()->CommitBoundary_Internal({Assets});
		if (!Committed)
		{
			return Committed;
		}
		FFrameTime Time;
		Time.DeltaSeconds = Seconds;
		Time.UnscaledDeltaSeconds = Seconds;
		return Scene.Tick_Internal({Input.GetSnapshot(), Time});
	}
};
// 全種類の登録順・同値接続・失敗した再接続・世代・所有終了を実Sceneで確認する。
template <typename T, typename J, typename D>
void Connections()
{
	TMechanismFixture<T, J, D> F;
	REQUIRE(F.Tick(0));
	REQUIRE(!F.Joint.Get()->GetJointId());
	REQUIRE(F.Tick());
	const auto Id = *F.Joint.Get()->GetJointId();
	const auto Good = F.Joint.Get()->GetDescription();
	for (Toolbox::uint32 I = 0; I < 60; ++I)
	{
		F.Joint.Get()->RequestConnect(Good);
		REQUIRE(F.Tick());
		REQUIRE(F.Joint.Get()->GetJointId() && *F.Joint.Get()->GetJointId() == Id);
	}
	REQUIRE(F.Joint.Get()->GetObservation()->SuccessfulStep == 61);
	auto Invalid = Good;
	Invalid.BodyB = Invalid.BodyA;
	F.Joint.Get()->RequestConnect(Invalid);
	REQUIRE(!F.Tick());
	REQUIRE(F.Scene.GetPhysicsWorld().IsJointAlive(Id));
	REQUIRE(!F.Joint.Get()->GetObservation());
	F.Joint.Get()->RequestConnect(Good);
	REQUIRE(F.Tick());
	REQUIRE(*F.Joint.Get()->GetJointId() == Id);
	F.Joint.Get()->RequestDisconnect();
	REQUIRE(F.Tick());
	REQUIRE(!F.Joint.Get()->GetJointId());
	REQUIRE(F.Scene.GetPhysicsWorld().IsAlive(F.A.Get()->GetBodyId()) && F.Scene.GetPhysicsWorld().IsAlive(F.B.Get()->GetBodyId()));
	F.Joint.Get()->RequestConnect(Good);
	REQUIRE(F.Tick());
	const auto NewId = *F.Joint.Get()->GetJointId();
	REQUIRE(Id.Index == NewId.Index && Id.Generation != NewId.Generation);
	const auto BodyId = F.B.Get()->GetBodyId();
	F.Scene.Shutdown_Internal();
	REQUIRE(!F.Scene.GetPhysicsWorld().IsJointAlive(NewId) && !F.Scene.GetPhysicsWorld().IsAlive(BodyId));
}
// 移動支点・明示ID・参照失効を全種類で試す。Worldの再採取や名前検索は使わない。
template <typename T, typename J, typename D>
void References()
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FInputStateTracker Input;
	typename T::SceneType Scene;
	const auto Link = Scene.template Spawn<DGameObject>().Value();
	const auto Support = Scene.template Spawn<DGameObject>().Value();
	const auto Load = Scene.template Spawn<DGameObject>().Value();
	const auto Mover = Support.Get()->template AddComponent<typename T::Mover>(typename T::MoverDescription{}).Value();
	const auto Body = Load.Get()->template AddComponent<typename T::Body>(typename T::BodyDescription{}).Value();
	D Description;
	Description.BodyA = T::Reference::FromKinematicMover(Mover);
	Description.BodyB = T::Reference::FromRigidBody(Body);
	const auto Joint = Link.Get()->template AddComponent<J>(Description).Value();
	Scene.GetPhysicsWorld().SetGravity({});
	REQUIRE(Scene.Initialize_Internal({Assets}));
	FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	auto Tick = [&]()
	{
		Scene.GetChildren_Internal()->FreezeBoundary_Internal();
		REQUIRE(Scene.GetChildren_Internal()->CommitBoundary_Internal({Assets}));
		return Scene.Tick_Internal({Input.GetSnapshot(), Time});
	};
	REQUIRE(Tick());
	REQUIRE(Joint.Get()->GetJointId());
	const auto OldJoint = *Joint.Get()->GetJointId();
	const auto OldBody = Body.Get()->GetBodyId();
	// 別Worldの完全IDを待機とせず拒否し、旧接続を維持する。
	typename T::SceneType Foreign;
	const auto OtherBody = Foreign.GetPhysicsWorld().CreateBody(typename T::BodyDescription{});
	auto Invalid = Description;
	Invalid.BodyB = T::Reference::FromBodyId(OtherBody);
	Joint.Get()->RequestConnect(Invalid);
	REQUIRE(!Tick());
	REQUIRE(Scene.GetPhysicsWorld().IsJointAlive(OldJoint));
	Joint.Get()->RequestConnect(Description);
	REQUIRE(Tick());
	// Bodyを所有するObjectの破棄要求で参照は即時失効する。
	Load.Get()->Destroy();
	REQUIRE(Joint.Get()->GetConnectionState() == EJointConnection::EndpointLost);
	REQUIRE(Tick());
	REQUIRE(!Scene.GetPhysicsWorld().IsAlive(OldBody) && !Scene.GetPhysicsWorld().IsJointAlive(OldJoint));
	const auto Replacement = Scene.template Spawn<DGameObject>().Value();
	const auto NewBody = Replacement.Get()->template AddComponent<typename T::Body>(typename T::BodyDescription{}).Value();
	REQUIRE(Tick());
	REQUIRE(!Joint.Get()->GetJointId());
	Description.BodyB = T::Reference::FromBodyId(NewBody.Get()->GetBodyId());
	Joint.Get()->RequestConnect(Description);
	REQUIRE(Tick());
	REQUIRE(Joint.Get()->GetJointId() && *Joint.Get()->GetJointId() != OldJoint);
	// 描画や観察ではなく所有終了がJointを解放する。支点と荷物は残す。
	Link.Get()->Destroy();
	REQUIRE(!Joint);
	REQUIRE(Tick());
	REQUIRE(Scene.GetPhysicsWorld().IsAlive(NewBody.Get()->GetBodyId()));
	Scene.Shutdown_Internal();
}
// 接続意図に属するDrive／Limitの要求順とID維持を両次元で確認する。
template <typename T, typename J, typename D>
void Drives()
{
	TMechanismFixture<T, J, D> F;
	auto Pending = F.Joint.Get()->GetDescription();
	Pending.Joint.Drive = {true, 1, 10};
	F.Joint.Get()->RequestConnect(Pending);
	F.Joint.Get()->RequestDrive({true, -1, 10});
	REQUIRE(F.Tick());
	const auto Id = *F.Joint.Get()->GetJointId();
	REQUIRE(F.Joint.Get()->GetObservation()->State.Drive.bEnabled);
	if constexpr (requires { F.Joint.Get()->GetObservation()->State.Drive.TargetAngularSpeed; })
	{
		REQUIRE(F.Joint.Get()->GetObservation()->State.Drive.TargetAngularSpeed == -1);
	}
	else
	{
		REQUIRE(F.Joint.Get()->GetObservation()->State.Drive.TargetSpeed == -1);
	}

	for (Toolbox::uint32 I = 0; I < 4; ++I)
	{
		F.Joint.Get()->RequestDrive({true, 1, 10});
		F.Joint.Get()->RequestLimits({true, -0.4, 0.4});
		REQUIRE(F.Tick());
		REQUIRE(*F.Joint.Get()->GetJointId() == Id);
	}
	const auto Good = F.Joint.Get()->GetDescription();
	bool bRejected = false;
	try
	{
		F.Joint.Get()->RequestDrive({true, 2, -1});
	}
	catch (const Toolbox::FException&)
	{
		bRejected = true;
	}
	REQUIRE(bRejected);
	REQUIRE(F.Tick());
	REQUIRE(*F.Joint.Get()->GetJointId() == Id);
	F.Joint.Get()->RequestDisconnect();
	F.Joint.Get()->RequestDrive({true, 2, 10});
	REQUIRE(F.Tick());
	REQUIRE(F.Joint.Get()->GetConnectionState() == EJointConnection::Disconnected);
	REQUIRE(!F.Joint.Get()->GetJointId());
	F.Joint.Get()->RequestConnect(Good);
	F.Joint.Get()->RequestDrive({false, 0, 10});
	F.Joint.Get()->RequestConnect(Good);
	REQUIRE(F.Tick());
	REQUIRE(F.Joint.Get()->GetDescription().Joint.Drive.bEnabled);
}
// 有限Motorの目標到達・逆転と、最大努力0の未到達を実Sceneで観察する。
template <bool bAngular, typename T, typename J, typename D>
void Targets()
{
	TMechanismFixture<T, J, D> F;
	REQUIRE(F.Tick());
	const auto Id = *F.Joint.Get()->GetJointId();
	for (Toolbox::int32 Stage = 0; Stage < 2; ++Stage)
	{
		bool bReached = false;
		for (Toolbox::uint32 I = 0; I < 600; ++I)
		{
			const auto State = F.Joint.Get()->GetObservation()->State;
			if constexpr (bAngular)
			{
				FAngularJointTargetSettings Target;
				Target.TargetAngle = Stage == 0 ? 0.5 : -0.3;
				const auto Command = ComputeJointTargetDrive(Target, State.Angle, State.AngularSpeed, FAngularJointLimits{true, -1, 1}, 1.0 / 60.0);
				bReached = Command.State == EJointTargetState::Reached;
				F.Joint.Get()->RequestDrive(Command.Drive);
			}
			else
			{
				FLinearJointTargetSettings Target;
				Target.TargetTranslation = Stage == 0 ? 0.5 : -0.3;
				const auto Command = ComputeJointTargetDrive(Target, State.Translation, State.TranslationRate, FLinearJointLimits{true, -1, 1}, 1.0 / 60.0);
				bReached = Command.State == EJointTargetState::Reached;
				F.Joint.Get()->RequestDrive(Command.Drive);
			}
			REQUIRE(F.Tick());
			REQUIRE(*F.Joint.Get()->GetJointId() == Id);
			if (bReached)
			{
				break;
			}
		}
		REQUIRE(bReached);
	}
	// 停止している状態と、届いた状態を同じ扱いにしない。
	for (Toolbox::uint32 I = 0; I < 120; ++I)
	{
		const auto State = F.Joint.Get()->GetObservation()->State;
		if constexpr (bAngular)
		{
			FAngularJointTargetSettings Target;
			Target.TargetAngle = 0.7;
			Target.MaxTorque = 0;
			const auto Command = ComputeJointTargetDrive(Target, State.Angle, State.AngularSpeed, FAngularJointLimits{true, -1, 1}, 1.0 / 60.0);
			REQUIRE(Command.State != EJointTargetState::Reached);
			F.Joint.Get()->RequestDrive(Command.Drive);
		}
		else
		{
			FLinearJointTargetSettings Target;
			Target.TargetTranslation = 0.7;
			Target.MaxForce = 0;
			const auto Command = ComputeJointTargetDrive(Target, State.Translation, State.TranslationRate, FLinearJointLimits{true, -1, 1}, 1.0 / 60.0);
			REQUIRE(Command.State != EJointTargetState::Reached);
			F.Joint.Get()->RequestDrive(Command.Drive);
		}
		REQUIRE(F.Tick());
	}
	bool bRejected = false;
	try
	{
		if constexpr (bAngular)
		{
			FAngularJointTargetSettings Target;
			Target.TargetAngle = 2;
			(void)ComputeJointTargetDrive(Target, 0, 0, FAngularJointLimits{true, -1, 1}, 1.0 / 60.0);
		}
		else
		{
			FLinearJointTargetSettings Target;
			Target.TargetTranslation = 2;
			(void)ComputeJointTargetDrive(Target, 0, 0, FLinearJointLimits{true, -1, 1}, 1.0 / 60.0);
		}
	}
	catch (const Toolbox::FException&)
	{
		bRejected = true;
	}
	REQUIRE(bRejected && *F.Joint.Get()->GetJointId() == Id);
}
} // namespace
TEST("K A01 2D Revolute real scene connections and lifetime")
{
	Connections<FMechanismFixture2D, DRevoluteJoint2DComponent, FRevoluteJointComponentDescription2D>();
}
TEST("K A01 2D Revolute drive limit request ordering and ID")
{
	Drives<FMechanismFixture2D, DRevoluteJoint2DComponent, FRevoluteJointComponentDescription2D>();
}
TEST("K A01 2D Fixed real scene connections and lifetime")
{
	Connections<FMechanismFixture2D, DFixedJoint2DComponent, FFixedJointComponentDescription2D>();
}
TEST("K A01 2D Prismatic real scene connections and lifetime")
{
	Connections<FMechanismFixture2D, DPrismaticJoint2DComponent, FPrismaticJointComponentDescription2D>();
}
TEST("K A01 2D Prismatic drive limit request ordering and ID")
{
	Drives<FMechanismFixture2D, DPrismaticJoint2DComponent, FPrismaticJointComponentDescription2D>();
}
TEST("K A01 3D Revolute real scene connections and lifetime")
{
	Connections<FMechanismFixture3D, DRevoluteJoint3DComponent, FRevoluteJointComponentDescription3D>();
}
TEST("K A01 3D Revolute drive limit request ordering and ID")
{
	Drives<FMechanismFixture3D, DRevoluteJoint3DComponent, FRevoluteJointComponentDescription3D>();
}
TEST("K A01 3D Fixed real scene connections and lifetime")
{
	Connections<FMechanismFixture3D, DFixedJoint3DComponent, FFixedJointComponentDescription3D>();
}
TEST("K A01 3D Prismatic real scene connections and lifetime")
{
	Connections<FMechanismFixture3D, DPrismaticJoint3DComponent, FPrismaticJointComponentDescription3D>();
}
TEST("K A01 3D Prismatic drive limit request ordering and ID")
{
	Drives<FMechanismFixture3D, DPrismaticJoint3DComponent, FPrismaticJointComponentDescription3D>();
}
TEST("K A02 2D Revolute real scene target reverse stop and blocked effort")
{
	Targets<true, FMechanismFixture2D, DRevoluteJoint2DComponent, FRevoluteJointComponentDescription2D>();
}
TEST("K A02 2D Prismatic real scene target reverse stop and blocked effort")
{
	Targets<false, FMechanismFixture2D, DPrismaticJoint2DComponent, FPrismaticJointComponentDescription2D>();
}
TEST("K A02 3D Revolute real scene target reverse stop and blocked effort")
{
	Targets<true, FMechanismFixture3D, DRevoluteJoint3DComponent, FRevoluteJointComponentDescription3D>();
}
TEST("K A02 3D Prismatic real scene target reverse stop and blocked effort")
{
	Targets<false, FMechanismFixture3D, DPrismaticJoint3DComponent, FPrismaticJointComponentDescription3D>();
}

TEST("mechanism Revolute-2D typed mover and explicit references preserve generations")
{
	References<FMechanismFixture2D, DRevoluteJoint2DComponent, FRevoluteJointComponentDescription2D>();
}

TEST("mechanism Fixed-2D typed mover and explicit references preserve generations")
{
	References<FMechanismFixture2D, DFixedJoint2DComponent, FFixedJointComponentDescription2D>();
}

TEST("mechanism Prismatic-2D typed mover and explicit references preserve generations")
{
	References<FMechanismFixture2D, DPrismaticJoint2DComponent, FPrismaticJointComponentDescription2D>();
}

TEST("mechanism Revolute-3D typed mover and explicit references preserve generations")
{
	References<FMechanismFixture3D, DRevoluteJoint3DComponent, FRevoluteJointComponentDescription3D>();
}

TEST("mechanism Fixed-3D typed mover and explicit references preserve generations")
{
	References<FMechanismFixture3D, DFixedJoint3DComponent, FFixedJointComponentDescription3D>();
}

TEST("mechanism Prismatic-3D typed mover and explicit references preserve generations")
{
	References<FMechanismFixture3D, DPrismaticJoint3DComponent, FPrismaticJointComponentDescription3D>();
}
