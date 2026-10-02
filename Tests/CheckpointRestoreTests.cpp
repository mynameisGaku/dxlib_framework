// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/CheckpointRestore.h"

#include "Dxf/SaveError.h"
#include "Dxf/ContentScene2D.h"
#include "Dxf/ContentScene3D.h"
#include "Dxf/InputStateTracker.h"
#include "Support/CheckpointFixture.h"
using namespace Dxf;
using namespace Toolbox;
using namespace TestCheckpointFixture;
namespace
{
template <typename TAction>
void RejectRestore(TAction&& Action, ESaveError Code = ESaveError::InvalidDocument)
{
	bool Rejected = false;
	try
	{
		Action();
	}
	catch (const FSaveError& E)
	{
		REQUIRE(E.GetCode() == Code);
		Rejected = true;
	}
	REQUIRE(Rejected);
}
template <typename TScene, typename TPlan>
void RealScene(const TPlan& Plan)
{
	Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	TScene Scene(PrepareScene(Plan.Scene, Assets));
	struct FShutdown
	{
		TScene& Scene;
		~FShutdown()
		{
			Scene.Shutdown_Internal();
		}
	} Guard{Scene};
	REQUIRE(Scene.Initialize_Internal({Assets}));
	const auto A = Scene.GetPrefab("doorA");
	const auto B = Scene.GetPrefab("doorB");
	const auto Cargo = Scene.GetPrefab("cargo1");
	REQUIRE(A.Get()->GetState() == EPrefabInstanceState::PendingPhysics);
	FInputStateTracker Input;
	FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	REQUIRE(A.Get()->GetState() == EPrefabInstanceState::Ready);
	REQUIRE(B.Get()->GetState() == EPrefabInstanceState::Ready);
	REQUIRE(Cargo.Get()->GetState() == EPrefabInstanceState::Ready);
	// 内部接続と個体間接続の全4種類を、同じ本体Worldで別々に確認する。
	REQUIRE(A.Get()->GetDistanceJoint("distance").Get()->GetJointId());
	REQUIRE(A.Get()->GetRevoluteJoint("revolute").Get()->GetJointId());
	REQUIRE(A.Get()->GetFixedJoint("fixed").Get()->GetJointId());
	REQUIRE(A.Get()->GetPrismaticJoint("prismatic").Get()->GetJointId());
	REQUIRE(Scene.GetDistanceConnection("distance").Get()->GetJointId());
	REQUIRE(Scene.GetRevoluteConnection("revolute").Get()->GetJointId());
	REQUIRE(Scene.GetFixedConnection("fixed").Get()->GetJointId());
	REQUIRE(Scene.GetPrismaticConnection("prismatic").Get()->GetJointId());
	REQUIRE(Scene.GetPhysicsWorld().GetPosition(Cargo.Get()->GetRigidBody("body").Get()->GetBodyId()).X > 30.9f);
	REQUIRE(Scene.GetPhysicsWorld().GetPosition(B.Get()->GetRigidBody("body").Get()->GetBodyId()).X > 10.9f);
	bool Absent = false;
	try
	{
		Scene.GetPrefab("removedKey");
	}
	catch (const FException&)
	{
		Absent = true;
	}
	REQUIRE(Absent);
}
} // namespace
TEST("Checkpoint 2D plan uses logical endpoints after reorder and creates all four Joint kinds")
{
	const auto C = Catalog2D();
	const auto D = Document(C);
	const auto Plan = FCheckpointRestore::Build(D, C);
	REQUIRE(Plan.Scene.Instances.Size() == 3 && Plan.Scene.Instances[2] == "doorA");
	REQUIRE(Plan.Scene.Connections[0].BodyA.Instance == 2);
	REQUIRE(Plan.Scene.Connections[0].BodyB.Instance == 0);
	REQUIRE(C.Scene.Instances[0] == "doorA" && C.Scene.Instances.Size() == 3);
	REQUIRE(Plan.GamePayload == "{\"collected\":[\"keyNorth\"],\"score\":7}");
	RealScene<DContentScene2D>(Plan);
}
TEST("Checkpoint 3D plan recreates dynamic instance and all four internal and inter instance Joint kinds")
{
	const auto C = Catalog3D();
	const auto Plan = FCheckpointRestore::Build(Document(C), C);
	REQUIRE(Plan.Scene.Connections[3].BodyA.Instance == 2);
	RealScene<DContentScene3D>(Plan);
}
TEST("Checkpoint both dimensions reject Content mismatch unknown catalog and wrong Joint kind")
{
	auto C = Catalog2D();
	auto D = Document(C);
	D.DefinitionFingerprint[0] = '0';
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D, C);
	              },
	              ESaveError::IncompatibleContent);
	D = Document(C);
	D.Instances[1].Prefab = "untrusted";
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D, C);
	              },
	              ESaveError::IncompatibleContent);
	D = Document(C);
	D.Joints[0].Kind = EJointKind::Fixed;
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D, C);
	              });
	auto C3 = Catalog3D();
	auto D3 = Document(C3);
	D3.Identity.ContentRevision = "other-bundle";
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D3, C3);
	              },
	              ESaveError::IncompatibleContent);
	D3 = Document(C3);
	D3.Joints[0].Kind = EJointKind::Fixed;
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D3, C3);
	              });
}
TEST("Checkpoint explicit CPU recipe validates parameter result and retains shared definition")
{
	auto C = Catalog2D();
	auto D = Document(C);
	D.Instances[0].Parameters[0].Number = 0.75;
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D, C);
	              },
	              ESaveError::IncompatibleContent);
	const auto Original = C.Prefabs[0].Definition;
	C.Prefabs[0].Resolve = [Original](const TVector<FContentParameterValue>& Values)
	{
		auto P = Original;
		if (Values.Size() != 1 || Values[0].Id != "speed" || Values[0].Kind != EContentParameterKind::Number || Values[0].Number < -1 || Values[0].Number > 1)
		{
			throw FException("Invalid test speed parameter");
		}
		P.Parameters = Values;
		return P;
	};
	const auto Plan = FCheckpointRestore::Build(D, C);
	REQUIRE(Plan.Scene.Prefabs[0].Parameters[0].Number == 0.75);
	REQUIRE(C.Prefabs[0].Definition.Parameters[0].Number == 0);
	D.Instances[0].Parameters[0].Number = 4;
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D, C);
	              });
	C.Prefabs[0].Resolve = [Original](const TVector<FContentParameterValue>& Values)
	{
		auto P = Original;
		P.Parameters = Values;
		P.ContentFingerprint = "changed";
		return P;
	};
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D, C);
	              },
	              ESaveError::IncompatibleContent);
}
TEST("Checkpoint tombstone is explicit and disconnected removed endpoint creates no Body")
{
	const auto C = Catalog2D();
	auto D = Document(C);
	D.Instances[3].bPresent = false;
	TVector<FCheckpointJointState> Kept;
	for (auto S : D.Joints)
	{
		if (S.Instance == "doorA")
		{
			continue;
		}
		if (S.Instance.IsEmpty())
		{
			S.bConnected = false;
		}
		Kept.PushBack(S);
	}
	D.Joints = Move(Kept);
	const auto Plan = FCheckpointRestore::Build(D, C);
	REQUIRE(Plan.Scene.Instances.Size() == 2 && Plan.Scene.Connections.IsEmpty());
	D.Joints[D.Joints.Size() - 1].bConnected = true;
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D, C);
	              });
	D = Document(C);
	D.Instances[3].bDynamic = true;
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D, C);
	              });
	D = Document(C);
	D.Joints.PushBack({"doorB", "unknown", EJointKind::Fixed});
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D, C);
	              });
	FSceneContentLimits L;
	L.MaxParts = 5;
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(Document(C), C, {}, L);
	              });
}
TEST("Checkpoint exported Joint aliases require consistent logical state")
{
	auto C = Catalog2D();
	C.Prefabs[0].Definition.Exports.PushBack({"railAlias", EContentExportKind::Prismatic, 3});
	auto D = Document(C);
	for (const auto& S : D.Instances)
	{
		if (S.bPresent)
		{
			auto Alias = D.Joints[3];
			Alias.Instance = S.Key;
			Alias.Export = "railAlias";
			D.Joints.PushBack(Alias);
		}
	}
	REQUIRE(FCheckpointRestore::Build(D, C).Scene.Prefabs.Size() == 3);
	D.Joints[D.Joints.Size() - 1].LinearDrive.TargetSpeed = 0.5;
	RejectRestore([&]
	              {
		              FCheckpointRestore::Build(D, C);
	              });
}
TEST("Checkpoint uses smaller caller catalog limit before CPU callbacks")
{
	auto C = Catalog3D();
	const auto D = Document(C);
	auto Other = C.Prefabs[0];
	Other.Key = "other";
	C.Prefabs.PushBack(Other);
	FCheckpointLimits L;
	L.MaxCatalogEntries = 1;
	const auto Action = [&]
	{
		FCheckpointRestore::Build(D, C, L);
	};
	RejectRestore(Action);
}
