// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/DistanceJointComponent2D.h"
#include "Dxf/DistanceJointComponent3D.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/InputStateTracker.h"
#include "Toolbox/JobSystem.h"
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
// Bodyが次の固定更新まで登録されない利用側のComponent。
template <typename TBody>
class TDelayedJointBody : public TBody
{
public:
	// 次元ごとの既存Body設定を転送する。
	template <typename TDescription>
	explicit TDelayedJointBody(TDescription Description) : TBody(Description)
	{
	}

protected:
	// 最初の一回は意図的に未登録を維持する。
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		if (m_bFirst)
		{
			m_bFirst = false;
			return;
		}
		TBody::OnFixedTick(Context);
	}

private:
	// 遅延する最初の固定更新か。
	bool m_bFirst = true;
};
// 予約後の破棄要求を同じ固定更新へ入れる。
class DDestroyJointEndpoint final : public DGameObjectComponent, private IPrePhysicsStep
{
public:
	// 解放は所有境界へ任せる。
	explicit DDestroyJointEndpoint(DLifecycleObject& Target) : m_pTarget(&Target)
	{
	}

protected:
	// Jointより後ろの予約から対象を失効させるため順序を試験で指定する。
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		Context.PrePhysicsStep->Enqueue(*this);
	}

private:
	// 予約された対象のメモリは固定更新中は保持される。
	void OnPrePhysicsStep_Internal(const FFixedTickContext&) override
	{
		m_pTarget->RequestDestroy_Internal();
	}
	// 同じ固定更新の間だけ有効な対象。
	DLifecycleObject* m_pTarget;
};
// 実Sceneを外部の描画・音声境界だけ置換して駆動する。
struct FJointSceneEnvironment
{
	// 資源と音声の試験境界。
	FFakeBackend Backend;
	// WorldやComponentは実製品を使う。
	FAssetService Assets{Backend, Backend, Backend};
	// Scene終了まで保持する音声窓口。
	FAudioPlayer Audio{Backend};
	// 固定更新へ渡す入力の保存先。
	FInputStateTracker Input;
	// Sceneを実際に初期化する環境。
	FInitContext Init()
	{
		return {Assets};
	}
	// 固定更新の数とPauseを指定する。
	FTickContext Tick(Toolbox::f64 Seconds = 1.0 / 60.0, bool bPaused = false)
	{
		// 固定更新へ渡す時間。
		FFrameTime Time;
		Time.DeltaSeconds = Seconds;
		Time.UnscaledDeltaSeconds = Seconds;
		Time.bPaused = bPaused;
		return {Input.GetSnapshot(), Time};
	}
};
} // namespace
namespace
{
// Bodyを別Objectへ配置し、JointはBodyより先に固定更新する。
struct FJointSceneFixture2D
{
	// Worldより長く有効な外部境界。
	FJointSceneEnvironment Environment;
	// 試験対象の実PhysicsScene。
	DPhysicsScene2D Scene;
	// 生存を追跡する世代付きComponent参照。
	TObjectHandle<DRigidBody2DComponent> A;
	// 接続のB側。
	TObjectHandle<DRigidBody2DComponent> B;
	// 今回調べる接続Component。
	TObjectHandle<DDistanceJoint2DComponent> Joint;
	// 毎回独立した所有階層を作る。
	explicit FJointSceneFixture2D(bool bReverse = false, bool bDelayed = false)
	{
		const auto Left = Scene.Spawn<DGameObject>();
		const auto Right = Scene.Spawn<DGameObject>();
		const auto Link = Scene.Spawn<DGameObject>();
		REQUIRE(Left && Right && Link);
		// 支点の生成条件。
		FBodyDescription2D Anchor;
		Anchor.Type = EBodyType::Static;
		Anchor.Position = {0, 3};
		A = Left.Value().Get()->AddComponent<DRigidBody2DComponent>(Anchor).Value();
		// 荷物の生成条件。
		FBodyDescription2D Weight;
		Weight.Position = {0, 1};
		if (bDelayed)
		{
			B = Right.Value().Get()->AddComponent<TDelayedJointBody<DRigidBody2DComponent>>(Weight).Value().template Cast<DRigidBody2DComponent>();
		}
		else
		{
			B = Right.Value().Get()->AddComponent<DRigidBody2DComponent>(Weight).Value();
		}
		// 登録へ渡す生成条件。
		FDistanceJointComponentDescription2D Description;
		Description.BodyA = FPhysicsBodyReference2D::FromRigidBody(A);
		Description.BodyB = FPhysicsBodyReference2D::FromRigidBody(B);
		Description.Joint.Length = 2;
		Joint = Link.Value().Get()->AddComponent<DDistanceJoint2DComponent>(Description).Value();
		Left.Value().Get()->SetUpdateOrder(bReverse ? 3 : 1);
		Right.Value().Get()->SetUpdateOrder(bReverse ? 1 : 3);
		Link.Value().Get()->SetUpdateOrder(0);
		REQUIRE(Scene.Initialize_Internal(Environment.Init()));
	}
	// Worldが破棄される前にComponentを停止する。
	~FJointSceneFixture2D()
	{
		Scene.Shutdown_Internal();
	}
	// 現実の固定更新→予約→Step→成功観察を一回通す。
	TResult<void> Tick(Toolbox::f64 Seconds = 1.0 / 60.0, bool bPaused = false)
	{
		// Applicationと同じ所有境界を明示的に通すCPU fixture。
		Scene.GetChildren_Internal()->FreezeBoundary_Internal();
		const auto Committed = Scene.GetChildren_Internal()->CommitBoundary_Internal(Environment.Init());
		if (!Committed)
		{
			return Committed;
		}
		return Scene.Tick_Internal(Environment.Tick(Seconds, bPaused));
	}
};
} // namespace
TEST("Joint component 2D registers once after all bodies in either order")
{
	for (Toolbox::int32 Order = 0; Order < 2; ++Order)
	{
		FJointSceneFixture2D Fixture(Order != 0);
		REQUIRE(Fixture.Joint.Get()->GetConnectionState() == EDistanceJointConnection::PendingBodies);
		REQUIRE(Fixture.Tick(0));
		REQUIRE(!Fixture.Joint.Get()->GetJointId());
		REQUIRE(Fixture.Tick());
		// 比較に使う世代付きID。
		const auto Id = Fixture.Joint.Get()->GetJointId();
		REQUIRE(Id);
		for (Toolbox::int32 Step = 0; Step < 60; ++Step)
		{
			Fixture.Joint.Get()->RequestConnect(Fixture.Joint.Get()->GetDescription());
			REQUIRE(Fixture.Tick());
			REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Id));
		}
		const auto Observed = Fixture.Joint.Get()->GetObservation();
		REQUIRE(Observed && Observed->SuccessfulStep == 61);
		REQUIRE(Toolbox::Abs(Observed->Error) < 0.03);
	}
}
TEST("Joint component 2D waits for delayed body and pauses requests")
{
	FJointSceneFixture2D Fixture(false, true);
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	REQUIRE(Fixture.Joint.Get()->GetConnectionState() == EDistanceJointConnection::PendingBodies);
	REQUIRE(Fixture.Tick());
	// 比較に使う世代付きID。
	const auto Id = Fixture.Joint.Get()->GetJointId();
	REQUIRE(Id);
	for (Toolbox::int32 Repeat = 0; Repeat < 100; ++Repeat)
	{
		Fixture.Joint.Get()->RequestDisconnect();
	}
	REQUIRE(Fixture.Tick(1.0 / 30.0, true));
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Id));
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Scene.GetPhysicsWorld().IsJointAlive(*Id));
	REQUIRE(Fixture.Joint.Get()->GetConnectionState() == EDistanceJointConnection::Disconnected);
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	Fixture.Joint.Get()->RequestConnect(Fixture.Joint.Get()->GetDescription());
	REQUIRE(Fixture.Tick(1.0 / 30.0));
	REQUIRE(Fixture.Joint.Get()->GetJointId());
	REQUIRE(Fixture.Joint.Get()->GetObservation()->SuccessfulStep == 6);
}
TEST("Joint component 2D applies only last request and preserves old connection on rejection")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture2D Fixture;
	REQUIRE(Fixture.Tick());
	// 比較に使う世代付きID。
	const auto Id = Fixture.Joint.Get()->GetJointId();
	const auto Good = Fixture.Joint.Get()->GetDescription();
	Fixture.Joint.Get()->RequestDisconnect();
	Fixture.Joint.Get()->RequestConnect(Good);
	REQUIRE(Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Id));
	auto Invalid = Good;
	Invalid.BodyB = Invalid.BodyA;
	Fixture.Joint.Get()->RequestConnect(Invalid);
	REQUIRE(!Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Id));
	REQUIRE(!Fixture.Joint.Get()->GetObservation());
	Fixture.Joint.Get()->RequestConnect(Good);
	REQUIRE(Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Id));
	Fixture.Joint.Get()->RequestConnect(Good);
	Fixture.Joint.Get()->RequestDisconnect();
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
}
TEST("Joint component 2D sees external joint loss and requires explicit reconnect")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture2D Fixture;
	REQUIRE(Fixture.Tick());
	// 比較に使う世代付きID。
	const auto Id = Fixture.Joint.Get()->GetJointId();
	REQUIRE(Fixture.Scene.GetPhysicsWorld().DestroyJoint(*Id));
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	REQUIRE(!Fixture.Joint.Get()->GetObservation());
	REQUIRE(Fixture.Joint.Get()->GetConnectionState() == EDistanceJointConnection::EndpointLost);
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	Fixture.Joint.Get()->RequestConnect(Fixture.Joint.Get()->GetDescription());
	REQUIRE(Fixture.Tick());
	// 成功した新しい接続ID。
	const auto New = Fixture.Joint.Get()->GetJointId();
	REQUIRE(New && New->Index == Id->Index && New->Generation != Id->Generation);
}
TEST("Joint component 2D never follows reused endpoint component or body slot")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture2D Fixture;
	REQUIRE(Fixture.Tick());
	// 比較に使う世代付きID。
	const auto Id = Fixture.Joint.Get()->GetJointId();
	const auto Body = Fixture.B.Get()->GetBodyId();
	// Componentを所有し、終了境界を通すObject。
	const auto Owner = Fixture.B.Get()->GetOwner();
	const auto OldReference = FPhysicsBodyReference2D::FromRigidBody(Fixture.B);
	REQUIRE(Owner->RemoveComponent(Fixture.B));
	REQUIRE(!Fixture.B.Get());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Scene.GetPhysicsWorld().IsJointAlive(*Id));
	// 登録へ渡す生成条件。
	FBodyDescription2D Description;
	Description.Position = {0, 1};
	const auto Replacement = Owner->AddComponent<DRigidBody2DComponent>(Description).Value();
	REQUIRE(Fixture.Tick());
	REQUIRE(Replacement.Get()->GetBodyId().Index == Body.Index);
	REQUIRE(Replacement.Get()->GetBodyId().Generation != Body.Generation);
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	REQUIRE(!OldReference.IsAvailable_Internal(Fixture.Scene.GetPhysicsWorld()));
	auto Settings = Fixture.Joint.Get()->GetDescription();
	Settings.BodyB = FPhysicsBodyReference2D::FromRigidBody(Replacement);
	Fixture.Joint.Get()->RequestConnect(Settings);
	REQUIRE(Fixture.Tick());
	REQUIRE(Fixture.Joint.Get()->GetJointId());
}
TEST("Joint component 2D supports explicit ids and rejects wrong world static pair and nonfinite")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture2D Fixture;
	REQUIRE(Fixture.Tick());
	const auto Original = Fixture.Joint.Get()->GetJointId();
	const auto Good = Fixture.Joint.Get()->GetDescription();
	auto Explicit = Good;
	Explicit.BodyA = FPhysicsBodyReference2D::FromBodyId(Fixture.A.Get()->GetBodyId());
	Fixture.Joint.Get()->RequestConnect(Explicit);
	REQUIRE(Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() != *Original));
	FPhysicsWorld2D Other;
	const auto Foreign = Other.CreateBody({});
	Explicit.BodyB = FPhysicsBodyReference2D::FromBodyId(Foreign);
	Fixture.Joint.Get()->RequestConnect(Explicit);
	// 操作前の比較値。
	const auto Before = Fixture.Joint.Get()->GetJointId();
	REQUIRE(!Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Before));
	FBodyDescription2D Static;
	Static.Type = EBodyType::Static;
	const auto OtherStatic = Fixture.Scene.GetPhysicsWorld().CreateBody(Static);
	Explicit.BodyB = FPhysicsBodyReference2D::FromBodyId(OtherStatic);
	Fixture.Joint.Get()->RequestConnect(Explicit);
	REQUIRE(!Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Before));
	auto NaN = Good;
	NaN.Joint.Length = Toolbox::Sqrt(-1.0);
	bool bThrew = false;
	try
	{
		Fixture.Joint.Get()->RequestConnect(NaN);
	}
	catch (const Toolbox::FException&)
	{
		bThrew = true;
	}
	REQUIRE(bThrew);
	Fixture.Joint.Get()->RequestConnect(Good);
	REQUIRE(Fixture.Tick());
}
TEST("Joint component 2D connects kinematic mover without duplicate body")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture2D Fixture;
	REQUIRE(Fixture.Tick());
	const auto Object = Fixture.Scene.Spawn<DGameObject>().Value();
	// 登録へ渡す生成条件。
	FKinematicMoverDescription2D Description;
	Description.Pose.Position = {0, 3};
	const auto Mover = Object.Get()->AddComponent<DKinematicMover2DComponent>(Description).Value();
	auto Settings = Fixture.Joint.Get()->GetDescription();
	Settings.BodyA = FPhysicsBodyReference2D::FromKinematicMover(Mover);
	Fixture.Joint.Get()->RequestConnect(Settings);
	REQUIRE(Fixture.Tick());
	REQUIRE(Fixture.Joint.Get()->GetJointId());
	FKinematicPose2D Goal;
	Goal.Position = {0.1f, 3};
	Mover.Get()->SetTarget(Goal);
	REQUIRE(Fixture.Tick());
	REQUIRE(Mover.Get()->GetBodyId());
	REQUIRE(Toolbox::Abs(Fixture.Scene.GetPhysicsWorld().GetPosition(*Mover.Get()->GetBodyId()).X - 0.1f) < 0.001);
	REQUIRE(Toolbox::Abs(Fixture.Joint.Get()->GetObservation()->Error) < 0.03);
	Mover.Get()->Destroy();
	REQUIRE(Fixture.Joint.Get()->GetConnectionState() == EDistanceJointConnection::EndpointLost);
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
}
TEST("Joint component 2D pending reservation honors destroy and component only shutdown")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture2D Fixture;
	// Componentを所有し、終了境界を通すObject。
	const auto Owner = Fixture.Joint.Get()->GetOwner();
	const auto Destroyer = Owner->AddComponent<DDestroyJointEndpoint>(*Fixture.Joint.Get()).Value();
	Destroyer.Get()->SetUpdateOrder(-1);
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get());
	REQUIRE(Fixture.A.Get()->HasBody() && Fixture.B.Get()->HasBody());
}
TEST("Joint component 2D step failure invalidates observation but keeps normal recovery")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture2D Fixture;
	REQUIRE(Fixture.Tick());
	Toolbox::FJobSystem Jobs(2);
	Jobs.Shutdown();
	FPhysicsExecutionSettings Execution;
	Execution.bParallelIntegration = false;
	Execution.bParallelBroadPhase = false;
	Execution.bParallelNarrowPhase = false;
	Execution.bParallelIslandSolver = true;
	Fixture.Scene.GetPhysicsWorld().SetExecutionSettings(Execution);
	Execution.JobSystem = &Jobs;
	Fixture.Scene.GetPhysicsWorld().SetExecutionSettings(Execution);
	// 二つ目の独立島を作り、Job投入拒否を実際に通す。
	FBodyDescription2D Anchor;
	Anchor.Type = EBodyType::Static;
	Anchor.Position = {10, 3};
	// 接続のA側。
	const auto A = Fixture.Scene.GetPhysicsWorld().CreateBody(Anchor);
	// 荷物の生成条件。
	FBodyDescription2D Weight;
	Weight.Position = {10, 1};
	// 接続のB側。
	const auto B = Fixture.Scene.GetPhysicsWorld().CreateBody(Weight);
	// 今回調べる接続Component。
	FDistanceJointDescription2D Joint;
	Joint.Length = 2;
	(void)Fixture.Scene.GetPhysicsWorld().CreateDistanceJoint(A, B, Joint);
	const auto Failed = Fixture.Tick();
	REQUIRE(!Failed);
	REQUIRE(!Fixture.Joint.Get()->GetObservation());
	Execution.JobSystem = nullptr;
	Fixture.Scene.GetPhysicsWorld().SetExecutionSettings(Execution);
	REQUIRE(Fixture.Tick());
	REQUIRE(Fixture.Joint.Get()->GetObservation());
}
namespace
{
// Bodyを別Objectへ配置し、JointはBodyより先に固定更新する。
struct FJointSceneFixture3D
{
	// Worldより長く有効な外部境界。
	FJointSceneEnvironment Environment;
	// 試験対象の実PhysicsScene。
	DPhysicsScene3D Scene;
	// 生存を追跡する世代付きComponent参照。
	TObjectHandle<DRigidBody3DComponent> A;
	// 接続のB側。
	TObjectHandle<DRigidBody3DComponent> B;
	// 今回調べる接続Component。
	TObjectHandle<DDistanceJoint3DComponent> Joint;
	// 毎回独立した所有階層を作る。
	explicit FJointSceneFixture3D(bool bReverse = false, bool bDelayed = false)
	{
		const auto Left = Scene.Spawn<DGameObject>();
		const auto Right = Scene.Spawn<DGameObject>();
		const auto Link = Scene.Spawn<DGameObject>();
		REQUIRE(Left && Right && Link);
		// 支点の生成条件。
		FBodyDescription3D Anchor;
		Anchor.Type = EBodyType::Static;
		Anchor.Position = {0, 3};
		A = Left.Value().Get()->AddComponent<DRigidBody3DComponent>(Anchor).Value();
		// 荷物の生成条件。
		FBodyDescription3D Weight;
		Weight.Position = {0, 1};
		if (bDelayed)
		{
			B = Right.Value().Get()->AddComponent<TDelayedJointBody<DRigidBody3DComponent>>(Weight).Value().template Cast<DRigidBody3DComponent>();
		}
		else
		{
			B = Right.Value().Get()->AddComponent<DRigidBody3DComponent>(Weight).Value();
		}
		// 登録へ渡す生成条件。
		FDistanceJointComponentDescription3D Description;
		Description.BodyA = FPhysicsBodyReference3D::FromRigidBody(A);
		Description.BodyB = FPhysicsBodyReference3D::FromRigidBody(B);
		Description.Joint.Length = 2;
		Joint = Link.Value().Get()->AddComponent<DDistanceJoint3DComponent>(Description).Value();
		Left.Value().Get()->SetUpdateOrder(bReverse ? 3 : 1);
		Right.Value().Get()->SetUpdateOrder(bReverse ? 1 : 3);
		Link.Value().Get()->SetUpdateOrder(0);
		REQUIRE(Scene.Initialize_Internal(Environment.Init()));
	}
	// Worldが破棄される前にComponentを停止する。
	~FJointSceneFixture3D()
	{
		Scene.Shutdown_Internal();
	}
	// 現実の固定更新→予約→Step→成功観察を一回通す。
	TResult<void> Tick(Toolbox::f64 Seconds = 1.0 / 60.0, bool bPaused = false)
	{
		// Applicationと同じ所有境界を明示的に通すCPU fixture。
		Scene.GetChildren_Internal()->FreezeBoundary_Internal();
		const auto Committed = Scene.GetChildren_Internal()->CommitBoundary_Internal(Environment.Init());
		if (!Committed)
		{
			return Committed;
		}
		return Scene.Tick_Internal(Environment.Tick(Seconds, bPaused));
	}
};
} // namespace
TEST("Joint component 3D registers once after all bodies in either order")
{
	for (Toolbox::int32 Order = 0; Order < 2; ++Order)
	{
		FJointSceneFixture3D Fixture(Order != 0);
		REQUIRE(Fixture.Joint.Get()->GetConnectionState() == EDistanceJointConnection::PendingBodies);
		REQUIRE(Fixture.Tick(0));
		REQUIRE(!Fixture.Joint.Get()->GetJointId());
		REQUIRE(Fixture.Tick());
		// 比較に使う世代付きID。
		const auto Id = Fixture.Joint.Get()->GetJointId();
		REQUIRE(Id);
		for (Toolbox::int32 Step = 0; Step < 60; ++Step)
		{
			Fixture.Joint.Get()->RequestConnect(Fixture.Joint.Get()->GetDescription());
			REQUIRE(Fixture.Tick());
			REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Id));
		}
		const auto Observed = Fixture.Joint.Get()->GetObservation();
		REQUIRE(Observed && Observed->SuccessfulStep == 61);
		REQUIRE(Toolbox::Abs(Observed->Error) < 0.03);
	}
}
TEST("Joint component 3D waits for delayed body and pauses requests")
{
	FJointSceneFixture3D Fixture(false, true);
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	REQUIRE(Fixture.Joint.Get()->GetConnectionState() == EDistanceJointConnection::PendingBodies);
	REQUIRE(Fixture.Tick());
	// 比較に使う世代付きID。
	const auto Id = Fixture.Joint.Get()->GetJointId();
	REQUIRE(Id);
	for (Toolbox::int32 Repeat = 0; Repeat < 100; ++Repeat)
	{
		Fixture.Joint.Get()->RequestDisconnect();
	}
	REQUIRE(Fixture.Tick(1.0 / 30.0, true));
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Id));
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Scene.GetPhysicsWorld().IsJointAlive(*Id));
	REQUIRE(Fixture.Joint.Get()->GetConnectionState() == EDistanceJointConnection::Disconnected);
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	Fixture.Joint.Get()->RequestConnect(Fixture.Joint.Get()->GetDescription());
	REQUIRE(Fixture.Tick(1.0 / 30.0));
	REQUIRE(Fixture.Joint.Get()->GetJointId());
	REQUIRE(Fixture.Joint.Get()->GetObservation()->SuccessfulStep == 6);
}
TEST("Joint component 3D applies only last request and preserves old connection on rejection")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture3D Fixture;
	REQUIRE(Fixture.Tick());
	// 比較に使う世代付きID。
	const auto Id = Fixture.Joint.Get()->GetJointId();
	const auto Good = Fixture.Joint.Get()->GetDescription();
	Fixture.Joint.Get()->RequestDisconnect();
	Fixture.Joint.Get()->RequestConnect(Good);
	REQUIRE(Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Id));
	auto Invalid = Good;
	Invalid.BodyB = Invalid.BodyA;
	Fixture.Joint.Get()->RequestConnect(Invalid);
	REQUIRE(!Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Id));
	REQUIRE(!Fixture.Joint.Get()->GetObservation());
	Fixture.Joint.Get()->RequestConnect(Good);
	REQUIRE(Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Id));
	Fixture.Joint.Get()->RequestConnect(Good);
	Fixture.Joint.Get()->RequestDisconnect();
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
}
TEST("Joint component 3D sees external joint loss and requires explicit reconnect")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture3D Fixture;
	REQUIRE(Fixture.Tick());
	// 比較に使う世代付きID。
	const auto Id = Fixture.Joint.Get()->GetJointId();
	REQUIRE(Fixture.Scene.GetPhysicsWorld().DestroyJoint(*Id));
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	REQUIRE(!Fixture.Joint.Get()->GetObservation());
	REQUIRE(Fixture.Joint.Get()->GetConnectionState() == EDistanceJointConnection::EndpointLost);
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	Fixture.Joint.Get()->RequestConnect(Fixture.Joint.Get()->GetDescription());
	REQUIRE(Fixture.Tick());
	// 成功した新しい接続ID。
	const auto New = Fixture.Joint.Get()->GetJointId();
	REQUIRE(New && New->Index == Id->Index && New->Generation != Id->Generation);
}
TEST("Joint component 3D never follows reused endpoint component or body slot")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture3D Fixture;
	REQUIRE(Fixture.Tick());
	// 比較に使う世代付きID。
	const auto Id = Fixture.Joint.Get()->GetJointId();
	const auto Body = Fixture.B.Get()->GetBodyId();
	// Componentを所有し、終了境界を通すObject。
	const auto Owner = Fixture.B.Get()->GetOwner();
	const auto OldReference = FPhysicsBodyReference3D::FromRigidBody(Fixture.B);
	REQUIRE(Owner->RemoveComponent(Fixture.B));
	REQUIRE(!Fixture.B.Get());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Scene.GetPhysicsWorld().IsJointAlive(*Id));
	// 登録へ渡す生成条件。
	FBodyDescription3D Description;
	Description.Position = {0, 1};
	const auto Replacement = Owner->AddComponent<DRigidBody3DComponent>(Description).Value();
	REQUIRE(Fixture.Tick());
	REQUIRE(Replacement.Get()->GetBodyId().Index == Body.Index);
	REQUIRE(Replacement.Get()->GetBodyId().Generation != Body.Generation);
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
	REQUIRE(!OldReference.IsAvailable_Internal(Fixture.Scene.GetPhysicsWorld()));
	auto Settings = Fixture.Joint.Get()->GetDescription();
	Settings.BodyB = FPhysicsBodyReference3D::FromRigidBody(Replacement);
	Fixture.Joint.Get()->RequestConnect(Settings);
	REQUIRE(Fixture.Tick());
	REQUIRE(Fixture.Joint.Get()->GetJointId());
}
TEST("Joint component 3D supports explicit ids and rejects wrong world static pair and nonfinite")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture3D Fixture;
	REQUIRE(Fixture.Tick());
	const auto Original = Fixture.Joint.Get()->GetJointId();
	const auto Good = Fixture.Joint.Get()->GetDescription();
	auto Explicit = Good;
	Explicit.BodyA = FPhysicsBodyReference3D::FromBodyId(Fixture.A.Get()->GetBodyId());
	Fixture.Joint.Get()->RequestConnect(Explicit);
	REQUIRE(Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() != *Original));
	FPhysicsWorld3D Other;
	const auto Foreign = Other.CreateBody({});
	Explicit.BodyB = FPhysicsBodyReference3D::FromBodyId(Foreign);
	Fixture.Joint.Get()->RequestConnect(Explicit);
	// 操作前の比較値。
	const auto Before = Fixture.Joint.Get()->GetJointId();
	REQUIRE(!Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Before));
	FBodyDescription3D Static;
	Static.Type = EBodyType::Static;
	const auto OtherStatic = Fixture.Scene.GetPhysicsWorld().CreateBody(Static);
	Explicit.BodyB = FPhysicsBodyReference3D::FromBodyId(OtherStatic);
	Fixture.Joint.Get()->RequestConnect(Explicit);
	REQUIRE(!Fixture.Tick());
	REQUIRE((Fixture.Joint.Get()->GetJointId() && *Fixture.Joint.Get()->GetJointId() == *Before));
	auto NaN = Good;
	NaN.Joint.Length = Toolbox::Sqrt(-1.0);
	bool bThrew = false;
	try
	{
		Fixture.Joint.Get()->RequestConnect(NaN);
	}
	catch (const Toolbox::FException&)
	{
		bThrew = true;
	}
	REQUIRE(bThrew);
	Fixture.Joint.Get()->RequestConnect(Good);
	REQUIRE(Fixture.Tick());
}
TEST("Joint component 3D connects kinematic mover without duplicate body")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture3D Fixture;
	REQUIRE(Fixture.Tick());
	const auto Object = Fixture.Scene.Spawn<DGameObject>().Value();
	// 登録へ渡す生成条件。
	FKinematicMoverDescription3D Description;
	Description.Pose.Position = {0, 3};
	const auto Mover = Object.Get()->AddComponent<DKinematicMover3DComponent>(Description).Value();
	auto Settings = Fixture.Joint.Get()->GetDescription();
	Settings.BodyA = FPhysicsBodyReference3D::FromKinematicMover(Mover);
	Fixture.Joint.Get()->RequestConnect(Settings);
	REQUIRE(Fixture.Tick());
	REQUIRE(Fixture.Joint.Get()->GetJointId());
	FKinematicPose3D Goal;
	Goal.Position = {0.1f, 3};
	Mover.Get()->SetTarget(Goal);
	REQUIRE(Fixture.Tick());
	REQUIRE(Mover.Get()->GetBodyId());
	REQUIRE(Toolbox::Abs(Fixture.Scene.GetPhysicsWorld().GetPosition(*Mover.Get()->GetBodyId()).X - 0.1f) < 0.001);
	REQUIRE(Toolbox::Abs(Fixture.Joint.Get()->GetObservation()->Error) < 0.03);
	Mover.Get()->Destroy();
	REQUIRE(Fixture.Joint.Get()->GetConnectionState() == EDistanceJointConnection::EndpointLost);
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get()->GetJointId());
}
TEST("Joint component 3D pending reservation honors destroy and component only shutdown")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture3D Fixture;
	// Componentを所有し、終了境界を通すObject。
	const auto Owner = Fixture.Joint.Get()->GetOwner();
	const auto Destroyer = Owner->AddComponent<DDestroyJointEndpoint>(*Fixture.Joint.Get()).Value();
	Destroyer.Get()->SetUpdateOrder(-1);
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Joint.Get());
	REQUIRE(Fixture.A.Get()->HasBody() && Fixture.B.Get()->HasBody());
}
TEST("Joint component 3D step failure invalidates observation but keeps normal recovery")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture3D Fixture;
	REQUIRE(Fixture.Tick());
	Toolbox::FJobSystem Jobs(2);
	Jobs.Shutdown();
	FPhysicsExecutionSettings Execution;
	Execution.bParallelIntegration = false;
	Execution.bParallelBroadPhase = false;
	Execution.bParallelNarrowPhase = false;
	Execution.bParallelIslandSolver = true;
	Fixture.Scene.GetPhysicsWorld().SetExecutionSettings(Execution);
	Execution.JobSystem = &Jobs;
	Fixture.Scene.GetPhysicsWorld().SetExecutionSettings(Execution);
	// 二つ目の独立島を作り、Job投入拒否を実際に通す。
	FBodyDescription3D Anchor;
	Anchor.Type = EBodyType::Static;
	Anchor.Position = {10, 3};
	// 接続のA側。
	const auto A = Fixture.Scene.GetPhysicsWorld().CreateBody(Anchor);
	// 荷物の生成条件。
	FBodyDescription3D Weight;
	Weight.Position = {10, 1};
	// 接続のB側。
	const auto B = Fixture.Scene.GetPhysicsWorld().CreateBody(Weight);
	// 今回調べる接続Component。
	FDistanceJointDescription3D Joint;
	Joint.Length = 2;
	(void)Fixture.Scene.GetPhysicsWorld().CreateDistanceJoint(A, B, Joint);
	const auto Failed = Fixture.Tick();
	REQUIRE(!Failed);
	REQUIRE(!Fixture.Joint.Get()->GetObservation());
	Execution.JobSystem = nullptr;
	Fixture.Scene.GetPhysicsWorld().SetExecutionSettings(Execution);
	REQUIRE(Fixture.Tick());
	REQUIRE(Fixture.Joint.Get()->GetObservation());
}
TEST("Joint component 2D removing only joint or owner preserves other bodies")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture2D Fixture;
	REQUIRE(Fixture.Tick());
	// 比較に使う世代付きID。
	const auto Id = *Fixture.Joint.Get()->GetJointId();
	// 接続のA側。
	const auto A = Fixture.A.Get()->GetBodyId();
	// 接続のB側。
	const auto B = Fixture.B.Get()->GetBodyId();
	Fixture.Joint.Get()->Destroy();
	REQUIRE(!Fixture.Joint.Get());
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Scene.GetPhysicsWorld().IsJointAlive(Id));
	REQUIRE(Fixture.Scene.GetPhysicsWorld().IsAlive(A));
	REQUIRE(Fixture.Scene.GetPhysicsWorld().IsAlive(B));
	Fixture.A.Get()->GetOwner()->Destroy();
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Scene.GetPhysicsWorld().IsAlive(A));
	REQUIRE(Fixture.Scene.GetPhysicsWorld().IsAlive(B));
}
TEST("Joint component 2D expired explicit id never becomes initial pending")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture2D Fixture;
	REQUIRE(Fixture.Tick());
	const auto Good = Fixture.Joint.Get()->GetDescription();
	// 比較に使う世代付きID。
	const auto Id = Fixture.Joint.Get()->GetJointId();
	const auto Temp = Fixture.Scene.GetPhysicsWorld().CreateBody({});
	REQUIRE(Fixture.Scene.GetPhysicsWorld().DestroyBody(Temp));
	// 成功した新しい接続ID。
	const auto New = Fixture.Scene.GetPhysicsWorld().CreateBody({});
	REQUIRE(New.Index == Temp.Index && New.Generation != Temp.Generation);
	auto Expired = Good;
	Expired.BodyB = FPhysicsBodyReference2D::FromBodyId(Temp);
	Fixture.Joint.Get()->RequestConnect(Expired);
	REQUIRE(!Fixture.Tick());
	REQUIRE(*Fixture.Joint.Get()->GetJointId() == *Id);
	Fixture.Joint.Get()->RequestConnect(Good);
	REQUIRE(Fixture.Tick());
}
TEST("Joint component 2D rejects nonphysics scene fixed context")
{
	// Sceneの初期化・更新に渡す試験窓口。
	FJointSceneEnvironment Environment;
	DGameObject Owner;
	// 今回調べる接続Component。
	const auto Joint = Owner.AddComponent<DDistanceJoint2DComponent>(FDistanceJointComponentDescription2D{}).Value();
	REQUIRE(Owner.Initialize_Internal(Environment.Init()));
	// 未配達の入力を保持する列。
	Toolbox::TVector<FInputSnapshot> Pending;
	FFixedTickContext Context{Environment.Input.GetSnapshot(), Pending};
	REQUIRE(!Owner.FixedTick_Internal(Context));
	REQUIRE(!Joint.Get()->GetJointId());
	Owner.Shutdown_Internal();
}
TEST("Joint component 2D rotated anchors and rendered endpoints use body interpolation")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture2D Fixture;
	REQUIRE(Fixture.Tick());
	auto Settings = Fixture.Joint.Get()->GetDescription();
	Settings.Joint.LocalAnchorA = {0.4f, 0};
	Settings.Joint.LocalAnchorB = {0.1f, 0.2f};
	Settings.Joint.Length = 2;
	Fixture.A.Get()->Teleport({0, 3}, 1.57079632679f);
	Fixture.Joint.Get()->RequestConnect(Settings);
	REQUIRE(Fixture.Tick());
	const auto Value = Fixture.Joint.Get()->GetObservation();
	REQUIRE(Value);
	// 接続のA側。
	Toolbox::FVector2 A;
	// 接続のB側。
	Toolbox::FVector2 B;
	REQUIRE(Fixture.Joint.Get()->GetRenderAnchors(A, B));
	const auto ExpectedA = Toolbox::FVector2{0, 3.4f};
	REQUIRE((Toolbox::Abs(Value->AnchorA.X - ExpectedA.X) + Toolbox::Abs(Value->AnchorA.Y - ExpectedA.Y)) < 0.0001);
	REQUIRE((Toolbox::Abs(A.X - ExpectedA.X) + Toolbox::Abs(A.Y - ExpectedA.Y)) < 0.0001);
	const auto RenderB = Settings.BodyB.RenderAnchor_Internal(Fixture.Scene.GetPhysicsWorld(), Settings.Joint.LocalAnchorB);
	REQUIRE(B == RenderB);
	const auto ReadBefore = Fixture.Joint.Get()->GetObservation()->SuccessfulStep;
	for (Toolbox::int32 Read = 0; Read < 20; ++Read)
	{
		REQUIRE(Fixture.Joint.Get()->GetJointId());
		REQUIRE(Fixture.Joint.Get()->GetRenderAnchors(A, B));
	}
	REQUIRE(Fixture.Joint.Get()->GetObservation()->SuccessfulStep == ReadBefore);
}
TEST("Joint component 3D removing only joint or owner preserves other bodies")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture3D Fixture;
	REQUIRE(Fixture.Tick());
	// 比較に使う世代付きID。
	const auto Id = *Fixture.Joint.Get()->GetJointId();
	// 接続のA側。
	const auto A = Fixture.A.Get()->GetBodyId();
	// 接続のB側。
	const auto B = Fixture.B.Get()->GetBodyId();
	Fixture.Joint.Get()->Destroy();
	REQUIRE(!Fixture.Joint.Get());
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Scene.GetPhysicsWorld().IsJointAlive(Id));
	REQUIRE(Fixture.Scene.GetPhysicsWorld().IsAlive(A));
	REQUIRE(Fixture.Scene.GetPhysicsWorld().IsAlive(B));
	Fixture.A.Get()->GetOwner()->Destroy();
	REQUIRE(Fixture.Tick());
	REQUIRE(!Fixture.Scene.GetPhysicsWorld().IsAlive(A));
	REQUIRE(Fixture.Scene.GetPhysicsWorld().IsAlive(B));
}
TEST("Joint component 3D expired explicit id never becomes initial pending")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture3D Fixture;
	REQUIRE(Fixture.Tick());
	const auto Good = Fixture.Joint.Get()->GetDescription();
	// 比較に使う世代付きID。
	const auto Id = Fixture.Joint.Get()->GetJointId();
	const auto Temp = Fixture.Scene.GetPhysicsWorld().CreateBody({});
	REQUIRE(Fixture.Scene.GetPhysicsWorld().DestroyBody(Temp));
	// 成功した新しい接続ID。
	const auto New = Fixture.Scene.GetPhysicsWorld().CreateBody({});
	REQUIRE(New.Index == Temp.Index && New.Generation != Temp.Generation);
	auto Expired = Good;
	Expired.BodyB = FPhysicsBodyReference3D::FromBodyId(Temp);
	Fixture.Joint.Get()->RequestConnect(Expired);
	REQUIRE(!Fixture.Tick());
	REQUIRE(*Fixture.Joint.Get()->GetJointId() == *Id);
	Fixture.Joint.Get()->RequestConnect(Good);
	REQUIRE(Fixture.Tick());
}
TEST("Joint component 3D rejects nonphysics scene fixed context")
{
	// Sceneの初期化・更新に渡す試験窓口。
	FJointSceneEnvironment Environment;
	DGameObject Owner;
	// 今回調べる接続Component。
	const auto Joint = Owner.AddComponent<DDistanceJoint3DComponent>(FDistanceJointComponentDescription3D{}).Value();
	REQUIRE(Owner.Initialize_Internal(Environment.Init()));
	// 未配達の入力を保持する列。
	Toolbox::TVector<FInputSnapshot> Pending;
	FFixedTickContext Context{Environment.Input.GetSnapshot(), Pending};
	REQUIRE(!Owner.FixedTick_Internal(Context));
	REQUIRE(!Joint.Get()->GetJointId());
	Owner.Shutdown_Internal();
}
TEST("Joint component 3D rotated anchors and rendered endpoints use body interpolation")
{
	// この試行の実SceneとComponent。
	FJointSceneFixture3D Fixture;
	REQUIRE(Fixture.Tick());
	auto Settings = Fixture.Joint.Get()->GetDescription();
	Settings.Joint.LocalAnchorA = {0.4f, 0, 0.13f};
	Settings.Joint.LocalAnchorB = {0.1f, 0.2f, 0.06f};
	Settings.Joint.Length = 2;
	const auto Rotation = Toolbox::FQuaternion::FromAxisAngle({1, 2, 3}, 0.73f);
	Fixture.A.Get()->Teleport({0, 3, 0}, Rotation);
	Fixture.Joint.Get()->RequestConnect(Settings);
	REQUIRE(Fixture.Tick());
	const auto Value = Fixture.Joint.Get()->GetObservation();
	REQUIRE(Value);
	// 接続のA側。
	Toolbox::FVector3 A;
	// 接続のB側。
	Toolbox::FVector3 B;
	REQUIRE(Fixture.Joint.Get()->GetRenderAnchors(A, B));
	// Quaternionの製品回転関数と独立したRodriguesの式で三成分を求める。
	const Toolbox::f64 Unit = 1.0 / Toolbox::Sqrt(14.0);
	const Toolbox::f64 Cosine = Toolbox::Cos(0.73f);
	const Toolbox::f64 Sine = Toolbox::Sin(0.73f);
	const auto Local = Settings.Joint.LocalAnchorA;
	const Toolbox::f64 Dot = Unit * (Local.X + 2 * Local.Y + 3 * Local.Z);
	const Toolbox::FVector3 ExpectedA{static_cast<Toolbox::f32>(Local.X * Cosine + Unit * (2 * Local.Z - 3 * Local.Y) * Sine + Unit * Dot * (1 - Cosine)), static_cast<Toolbox::f32>(3 + Local.Y * Cosine + Unit * (3 * Local.X - Local.Z) * Sine + 2 * Unit * Dot * (1 - Cosine)), static_cast<Toolbox::f32>(Local.Z * Cosine + Unit * (Local.Y - 2 * Local.X) * Sine + 3 * Unit * Dot * (1 - Cosine))};
	REQUIRE((Toolbox::Abs(Value->AnchorA.X - ExpectedA.X) + Toolbox::Abs(Value->AnchorA.Y - ExpectedA.Y) + Toolbox::Abs(Value->AnchorA.Z - ExpectedA.Z)) < 0.0001);
	REQUIRE((Toolbox::Abs(A.X - ExpectedA.X) + Toolbox::Abs(A.Y - ExpectedA.Y) + Toolbox::Abs(A.Z - ExpectedA.Z)) < 0.0001);
	const auto RenderB = Settings.BodyB.RenderAnchor_Internal(Fixture.Scene.GetPhysicsWorld(), Settings.Joint.LocalAnchorB);
	REQUIRE(B == RenderB);
	const auto ReadBefore = Fixture.Joint.Get()->GetObservation()->SuccessfulStep;
	for (Toolbox::int32 Read = 0; Read < 20; ++Read)
	{
		REQUIRE(Fixture.Joint.Get()->GetJointId());
		REQUIRE(Fixture.Joint.Get()->GetRenderAnchors(A, B));
	}
	REQUIRE(Fixture.Joint.Get()->GetObservation()->SuccessfulStep == ReadBefore);
}
