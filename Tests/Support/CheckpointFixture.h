// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_CHECKPOINT_FIXTURE_H
#define DXF_TEST_CHECKPOINT_FIXTURE_H
#include "Dxf/CheckpointRestore.h"
namespace TestCheckpointFixture
{
using namespace Dxf;
using namespace Toolbox;
// 手作りCPU値の指紋はこの試験の識別だけ。実ファイル指紋は別のContent試験で確認する。
inline const char* Stamp = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
template <typename TPrefab, typename TPart, typename TJoint>
TPrefab Prefab()
{
	TPrefab P;
	P.Path = "Fixtures/mechanism.json";
	P.ContentFingerprint = Stamp;
	TPart A;
	A.Id = "frame";
	A.bHasBody = true;
	A.Body.Type = EBodyType::Static;
	P.Parts.PushBack(A);
	A.Id = "leaf";
	A.Body.Type = EBodyType::Dynamic;
	A.Body.Position.X = 1;
	P.Parts.PushBack(A);
	P.Exports.PushBack({"support", EContentExportKind::RigidBody, 0});
	P.Exports.PushBack({"body", EContentExportKind::RigidBody, 1});
	const EJointKind Kinds[] = {EJointKind::Distance, EJointKind::Revolute, EJointKind::Fixed, EJointKind::Prismatic};
	const EContentExportKind ExportKinds[] = {EContentExportKind::Distance, EContentExportKind::Revolute, EContentExportKind::Fixed, EContentExportKind::Prismatic};
	const char* Names[] = {"distance", "revolute", "fixed", "prismatic"};
	for (uint32 I = 0; I < 4; ++I)
	{
		TJoint J;
		J.Id = Names[I];
		J.BodyB = 1;
		J.Kind = Kinds[I];
		J.Distance.Length = 1;
		J.Revolute.FrameB.LocalAnchor.X = -1;
		J.Fixed.FrameB.LocalAnchor.X = -1;
		P.Joints.PushBack(J);
		P.Exports.PushBack({Names[I], ExportKinds[I], I});
	}
	FContentParameterValue Speed;
	Speed.Id = "speed";
	Speed.Number = 0;
	P.Parameters.PushBack(Speed);
	return P;
}
template <typename TCatalog, typename TPrefab, typename TPart, typename TJoint, typename TConnection, typename TRecipe>
TCatalog Catalog(uint32 Dimension)
{
	TCatalog C;
	C.Identity.GameId = "resume_test";
	C.Identity.ContentRevision = "bundle-v1";
	C.Identity.SceneKey = "course";
	C.Identity.CheckpointKey = "start";
	C.Identity.Dimension = Dimension;
	C.Scene.Path = "Fixtures/scene.json";
	C.Scene.ContentFingerprint = Stamp;
	TRecipe R;
	R.Key = "mechanism";
	R.Definition = Prefab<TPrefab, TPart, TJoint>();
	C.Prefabs.PushBack(R);
	const char* Ids[] = {"doorA", "doorB", "removedKey"};
	for (size_t I = 0; I < 3; ++I)
	{
		C.Scene.Prefabs.PushBack(R.Definition);
		C.Scene.Placements.PushBack({});
		C.Scene.Placements[I].Position.X = static_cast<f32>(I * 10);
		C.Scene.Instances.PushBack(Ids[I]);
		C.InitialPrefabKeys.PushBack("mechanism");
	}
	for (uint32 I = 0; I < 4; ++I)
	{
		TConnection Connection;
		Connection.BodyA = {0, R.Definition.Exports[0]};
		Connection.BodyB = {1, R.Definition.Exports[1]};
		Connection.Joint = R.Definition.Joints[I];
		Connection.Joint.Distance.LocalAnchorB.X = -10;
		Connection.Joint.Revolute.FrameB.LocalAnchor.X = -11;
		Connection.Joint.Fixed.FrameB.LocalAnchor.X = -11;
		Connection.Joint.Prismatic.FrameB.LocalAnchor.X = -10;
		C.Scene.Connections.PushBack(Connection);
	}
	return C;
}
template <typename TDocument, typename TInstance, typename TCatalog>
TDocument Checkpoint(const TCatalog& C)
{
	TDocument D;
	D.Identity = C.Identity;
	D.DefinitionFingerprint = Stamp;
	D.GamePayload = "{\"collected\":[\"keyNorth\"],\"score\":7}";
	// 初期Sceneと違う生成順にし、動的個体と明示除去を混ぜる。
	const char* Ids[] = {"doorB", "cargo1", "removedKey", "doorA"};
	for (size_t I = 0; I < 4; ++I)
	{
		TInstance S;
		S.Key = Ids[I];
		S.Prefab = "mechanism";
		S.DefinitionFingerprint = Stamp;
		S.bDynamic = I == 1;
		S.bPresent = I != 2;
		S.Placement.Position.X = I == 0 ? 10 : I == 1 ? 30
		                                   : I == 2   ? 20
		                                              : 0;
		S.Parameters = C.Prefabs[0].Definition.Parameters;
		D.Instances.PushBack(S);
		if (S.bPresent)
		{
			for (const auto& J : C.Prefabs[0].Definition.Joints)
			{
				FCheckpointJointState State;
				State.Instance = S.Key;
				State.Export = J.Id;
				State.Kind = J.Kind;
				if (J.Kind == EJointKind::Prismatic)
				{
					State.LinearDrive = {true, 0, 12};
					State.LinearLimits = {true, -1, 2};
				}
				D.Joints.PushBack(State);
			}
		}
	}
	for (const auto& Cn : C.Scene.Connections)
	{
		FCheckpointJointState State;
		State.Export = Cn.Joint.Id;
		State.Kind = Cn.Joint.Kind;
		D.Joints.PushBack(State);
	}
	return D;
}
inline auto Catalog2D()
{
	return Catalog<FCheckpointCatalog2D, FPrefabDefinition2D, FContentPartDefinition2D, FContentJointDefinition2D, FContentSceneConnection2D, FCheckpointPrefabRecipe2D>(2);
}
inline auto Catalog3D()
{
	return Catalog<FCheckpointCatalog3D, FPrefabDefinition3D, FContentPartDefinition3D, FContentJointDefinition3D, FContentSceneConnection3D, FCheckpointPrefabRecipe3D>(3);
}
inline auto Document(const FCheckpointCatalog2D& C)
{
	return Checkpoint<FCheckpointDocument2D, FCheckpointInstance2D>(C);
}
inline auto Document(const FCheckpointCatalog3D& C)
{
	return Checkpoint<FCheckpointDocument3D, FCheckpointInstance3D>(C);
}
} // namespace TestCheckpointFixture
#endif
