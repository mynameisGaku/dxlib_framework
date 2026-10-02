// SPDX-License-Identifier: NOASSERTION
#include "CheckpointConsumer.h"
#include "Dxf/CheckpointRestore.h"
#include "Dxf/SceneContentSource.h"
#include "Dxf/ContentScene2D.h"
#include "Dxf/ContentScene3D.h"
#include "Dxf/InputStateTracker.h"
namespace
{
using namespace Dxf;
using namespace Toolbox;
void Check(bool Condition)
{
	if (!Condition)
	{
		throw FException("External checkpoint plan contract failed");
	}
}
template <typename TCatalog, typename TRecipe, typename TDocument, typename TInstance, typename TScene, typename TDefinition, typename TPrefab>
void Test(FAssetService& Assets, TDefinition Definition, TPrefab Prefab, uint32 Dimension)
{
	TCatalog Catalog;
	Catalog.Identity.GameId = "consumer_game";
	Catalog.Identity.ContentRevision = "consumer-assets-v1";
	Catalog.Identity.SceneKey = "course";
	Catalog.Identity.CheckpointKey = "north";
	Catalog.Identity.Dimension = Dimension;
	Catalog.Scene = Move(Definition);
	TRecipe Recipe;
	Recipe.Key = "mechanism";
	Recipe.Definition = Move(Prefab);
	Catalog.Prefabs.PushBack(Recipe);
	for (const auto& Id : Catalog.Scene.Instances)
	{
		(void)Id;
		Catalog.InitialPrefabKeys.PushBack("mechanism");
	}
	TDocument Document;
	Document.Identity = Catalog.Identity;
	Document.DefinitionFingerprint = Catalog.Scene.ContentFingerprint;
	Document.GamePayload = "{\"collected\":[\"keyNorth\"],\"score\":42}";
	for (size_t I = 0; I < Catalog.Scene.Instances.Size(); ++I)
	{
		TInstance Instance;
		Instance.Key = Catalog.Scene.Instances[I];
		Instance.Prefab = Recipe.Key;
		Instance.DefinitionFingerprint = Recipe.Definition.ContentFingerprint;
		Instance.Placement = Catalog.Scene.Placements[I];
		Instance.Parameters = Recipe.Definition.Parameters;
		Instance.bPresent = I == 0;
		Document.Instances.PushBack(Instance);
	}
	auto Cargo = Document.Instances[0];
	Cargo.Key = "cargo";
	Cargo.bDynamic = true;
	Cargo.Placement.Position.X = 9;
	Document.Instances.PushBack(Cargo);
	for (const auto& Instance : Document.Instances)
	{
		if (Instance.bPresent)
		{
			FCheckpointJointState State;
			State.Instance = Instance.Key;
			State.Export = "drive";
			State.Kind = EJointKind::Prismatic;
			State.bConnected = Instance.Key == "cargo";
			State.LinearDrive = {true, 0, 12};
			Document.Joints.PushBack(State);
		}
	}
	// 最小公開経路。新しいWorldのIDは既存Componentが登録する。
	const auto Plan = FCheckpointRestore::Build(Document, Catalog);
	Check(Plan.Scene.Instances.Size() == 2 && Plan.Scene.Instances[1] == "cargo");
	Check(Catalog.Scene.Instances.Size() == 2 && Catalog.Scene.Instances[1] == "right");
	TScene Scene(PrepareScene(Plan.Scene, Assets));
	struct FShutdown
	{
		TScene& Scene;
		~FShutdown()
		{
			Scene.Shutdown_Internal();
		}
	} Guard{Scene};
	Check(static_cast<bool>(Scene.Initialize_Internal({Assets})));
	FInputStateTracker Input;
	FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	Check(static_cast<bool>(Scene.Tick_Internal({Input.GetSnapshot(), Time})));
	const auto Left = Scene.GetPrefab("left");
	const auto Dynamic = Scene.GetPrefab("cargo");
	Check(Left.Get()->GetState() == EPrefabInstanceState::Ready && Dynamic.Get()->GetState() == EPrefabInstanceState::Ready);
	Check(!Left.Get()->GetPrismaticJoint("drive").Get()->GetJointId());
	Check(static_cast<bool>(Dynamic.Get()->GetPrismaticJoint("drive").Get()->GetJointId()));
	Check(Scene.GetPhysicsWorld().GetPosition(Dynamic.Get()->GetRigidBody("body").Get()->GetBodyId()).X > 8.9f);
	bool Removed = false;
	try
	{
		Scene.GetPrefab("right");
	}
	catch (const FException&)
	{
		Removed = true;
	}
	Check(Removed);
}
} // namespace
void RunCheckpointConsumer(Dxf::FAssetService& Assets, const Toolbox::FPath& Root)
{
	Dxf::FSceneContentSource Source(Root);
	Test<Dxf::FCheckpointCatalog2D, Dxf::FCheckpointPrefabRecipe2D, Dxf::FCheckpointDocument2D, Dxf::FCheckpointInstance2D, Dxf::DContentScene2D>(Assets, Source.LoadScene2D("Data/scene2d.dxfscene.json"), Source.LoadPrefab2D("Data/prefab2d.dxfprefab.json"), 2);
	Test<Dxf::FCheckpointCatalog3D, Dxf::FCheckpointPrefabRecipe3D, Dxf::FCheckpointDocument3D, Dxf::FCheckpointInstance3D, Dxf::DContentScene3D>(Assets, Source.LoadScene3D("Data/scene3d.dxfscene.json"), Source.LoadPrefab3D("Data/prefab3d.dxfprefab.json"), 3);
}
