// SPDX-License-Identifier: NOASSERTION
#include "CheckpointConsumer.h"
#include "Dxf/CheckpointRestore.h"
#include "Dxf/CheckpointResumeRequest2D.h"
#include "Dxf/CheckpointResumeRequest3D.h"
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
template <typename TCatalog, typename TRecipe, typename TDocument, typename TInstance, typename TScene, typename TResume, typename TDefinition, typename TPrefab>
void Test(FAssetService& Assets, TDefinition Definition, TPrefab Prefab, uint32 Dimension, const FPath& SaveRoot, FAudioPlayer& Audio)
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
	// 公開入口から初期個体とJoint状態を作り、ゲーム進行だけを明示する。
	TDocument Document = FCheckpointRestore::MakeInitial(Catalog, "{\"collected\":[\"keyNorth\"],\"score\":42}");
	for (size_t I = 0; I < Document.Instances.Size(); ++I)
	{
		Document.Instances[I].bPresent = I == 0;
	}
	auto Cargo = Document.Instances[0];
	Cargo.Key = "cargo";
	Cargo.bDynamic = true;
	Cargo.Placement.Position.X = 9;
	Document.Instances.PushBack(Cargo);
	// 除去と追加を反映したゲーム側の論理接続を選ぶ。
	Document.Joints.Clear();
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
	FSceneNavigator Scenes(Assets, Audio);
	FSaveStore Store(SaveRoot);
	Store.Save("checkpoint", FCheckpointCodec::Encode(Document));
	FGameSaveRequest Saves(Store);
	TScene* Current = nullptr;
	TResume Resume(Saves, Scenes, Assets, Catalog, [](const TDocument& Input)
	               {
		               Check(Input.GamePayload == "{\"collected\":[\"keyNorth\"],\"score\":42}");
		               return Input;
	               },
	               [&Current](auto Prepared, TDocument Input)
	               {
		               auto Scene = MakeUnique<TScene>(Move(Prepared), Move(Input));
		               Current = Scene.Get();
		               return Scene;
	               });
	const auto Ticket = Resume.RequestLoad("checkpoint");
	Resume.Poll();
	Check(Ticket.GetState() == EGameSaveRequestState::TransitionRequested && Scenes.GetCurrent() == nullptr);
	Check(static_cast<bool>(Scenes.Commit()));
	Resume.Poll();
	Check(Ticket.GetState() == EGameSaveRequestState::TransitionRequested);
	FInputStateTracker Input;
	FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	Check(static_cast<bool>(Scenes.Tick(Time, Input.GetSnapshot())));
	Resume.Poll();
	Check(Ticket.GetState() == EGameSaveRequestState::Completed);
	auto& Scene = *Current;
	auto& Capture = Scene.GetCheckpointCapture();
	Check(Capture.GetCapturedStep() == 1 && Capture.GetLastCapture() && Capture.GetLastCapture()->GamePayload == Document.GamePayload);
	Check(Capture.GetLastCapture()->Instances.Size() == 3 && !Capture.GetLastCapture()->Instances[1].bPresent);
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
void RunCheckpointConsumer(Dxf::FAssetService& Assets, const Toolbox::FPath& Root, const Toolbox::FPath& SaveRoot, Dxf::FAudioPlayer& Audio)
{
	Dxf::FSceneContentSource Source(Root);
	Test<Dxf::FCheckpointCatalog2D, Dxf::FCheckpointPrefabRecipe2D, Dxf::FCheckpointDocument2D, Dxf::FCheckpointInstance2D, Dxf::ACheckpointScene2D, Dxf::FCheckpointResumeRequest2D>(Assets, Source.LoadScene2D("Data/scene2d.dxfscene.json"), Source.LoadPrefab2D("Data/prefab2d.dxfprefab.json"), 2, SaveRoot / "2D", Audio);
	Test<Dxf::FCheckpointCatalog3D, Dxf::FCheckpointPrefabRecipe3D, Dxf::FCheckpointDocument3D, Dxf::FCheckpointInstance3D, Dxf::ACheckpointScene3D, Dxf::FCheckpointResumeRequest3D>(Assets, Source.LoadScene3D("Data/scene3d.dxfscene.json"), Source.LoadPrefab3D("Data/prefab3d.dxfprefab.json"), 3, SaveRoot / "3D", Audio);
}
