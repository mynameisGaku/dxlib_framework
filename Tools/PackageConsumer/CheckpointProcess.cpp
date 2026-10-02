// SPDX-License-Identifier: NOASSERTION
#include "CheckpointProcess.h"
#include "Dxf/CheckpointRestore.h"
#include "Dxf/CheckpointCodec.h"
#include "Dxf/CheckpointResumeRequest2D.h"
#include "Dxf/CheckpointResumeRequest3D.h"
#include "Dxf/SceneContentSource.h"
#include "Dxf/UserSettingsCodec.h"
#include "Dxf/InputProfileCodec.h"
#include "Dxf/InputStateTracker.h"
namespace
{
using namespace Dxf;
using namespace Toolbox;
// 独立した期待値との不一致を、外部プロセスの終了失敗へ伝える。
void Check(bool Value, const char* Reason)
{
	if (!Value)
	{
		throw FException(Reason);
	}
}
// このゲームが許可する操作。保存コアへゲーム規則を入れない。
FInputProfilePolicy Policy()
{
	FInputProfilePolicy Value;
	Value.AllowedActions = {"Jump", "Interact", "PadAction"};
	return Value;
}
// 利用者が変更した操作環境の独立した期待値。
FUserSettings Settings()
{
	FUserSettings Value;
	FInputBindingDescription Jump;
	Jump.Key = EKey::K;
	FInputBindingDescription Interact;
	Interact.Device = EInputBindingDevice::Mouse;
	Interact.MouseButton = EMouseButton::Right;
	FInputBindingDescription Pad;
	Pad.Device = EInputBindingDevice::Gamepad;
	Pad.PadSlot = 3;
	Pad.PadButton = 15;
	Value.Input.Actions = {{"Jump", {Jump}}, {"Interact", {Interact}}, {"PadAction", {Pad}}};
	Value.Volume = 0.4f;
	Value.UiScale = 1.75f;
	return Value;
}
// 新しいSceneへ保存値を採用し、最初の通常固定更新のReadyまで観察する。
template <typename TCatalog, typename TScene, typename TResume>
void Play(TCatalog Catalog, FAssetService& Assets, FSceneNavigator& Scenes, const TFunction<void()>& Step, const TFunction<void(DScene&, const char*)>& Observe, const FPath& SaveRoot, FStringView Mode)
{
	FSaveStore Store(SaveRoot);
	FGameSaveRequest Saves(Store);
	TResume Resume(Saves, Scenes, Assets, Catalog, [](const auto& Value)
	               {
		               return Value;
	               });
	const auto TickReady = [&]()
	{
		for (uint32 I = 0; I < 8; ++I)
		{
			Step();
			Resume.Poll();
			auto* Current = Scenes.GetCurrent()->TryCast<TScene>();
			if (Current && Current->GetResumeTicket().GetState() == ECheckpointSceneState::Ready)
			{
				return Current;
			}
		}
		throw FException("External checkpoint did not reach mandatory Ready");
	};
	if (Mode == "write")
	{
		auto Initial = FCheckpointRestore::MakeInitial(Catalog, "{\"collected\":[],\"score\":0,\"spawnCounter\":0,\"unlocked\":false}");
		const auto Plan = FCheckpointRestore::Build(Initial, Catalog);
		Check(static_cast<bool>(Scenes.RequestChange(MakeUnique<TScene>(PrepareScene(Plan.Scene, Assets), Initial))), "External New Game request failed");
		auto* Current = TickReady();
		Check(Current->GetGamePayload() == Initial.GamePayload, "External initial progress differs");
		Check(Current->GetPrefab("key").Get()->GetState() == EPrefabInstanceState::Ready, "External initial pickup missing");
		if (Observe)
		{
			Observe(*Current, "before");
		}
		// 取得済みと解錠済みをゲーム側で反映し、実際の生成・破棄を経由する。
		Current->SetGamePayload("{\"collected\":[\"key\"],\"score\":42,\"spawnCounter\":1,\"unlocked\":true}");
		auto& Capture = Current->GetCheckpointCapture();
		Check(Capture.Destroy(*Current, "key"), "External pickup removal failed");
		auto Cargo = Initial.Instances[0];
		Cargo.Key = "cargo";
		Cargo.bDynamic = true;
		Cargo.Placement.Position.X = 8;
		Check(static_cast<bool>(Capture.Spawn(*Current, Cargo, PreparePrefab(Catalog.Prefabs[0].Definition, Assets))), "External cargo spawn failed");
		Current->GetPrefab("left").Get()->GetRevoluteJoint("revolute").Get()->RequestDisconnect();
		Current->GetPrefab("left").Get()->GetPrismaticJoint("drive").Get()->RequestDrive({true, 0.4, 12});
		Current->GetPrefab("right").Get()->GetPrismaticJoint("drive").Get()->RequestDrive({true, -0.2, 7});
		Check(Capture.RequestCapture("north"), "External checkpoint request failed");
		for (uint32 I = 0; I < 4; ++I)
		{
			Step();
		}
		Check(Capture.GetLastCapture() && !Capture.HasCaptureRequest(), "External checkpoint capture incomplete");
		Check(Capture.GetLastCapture()->Identity.CheckpointKey == "north", "External checkpoint key differs");
		Check(Store.Save("manual", FCheckpointCodec::Encode(*Capture.GetLastCapture())) == 1, "External new slot not generation one");
		const auto SavedBytes = Store.Load("manual").Document->Payload;
		Check(Store.Save("settings", FUserSettingsCodec::Encode(Settings(), Policy(), "consumer_checkpoint")) == 1, "External settings commit failed");
		if (Observe)
		{
			Observe(*Current, "saved");
		}
		Current->SetGamePayload("{\"collected\":[\"key\"],\"score\":99,\"spawnCounter\":2,\"unlocked\":false}");
		Cargo.Key = "cargoChanged";
		Cargo.Placement.Position.X = -4;
		Check(static_cast<bool>(Capture.Spawn(*Current, Cargo, PreparePrefab(Catalog.Prefabs[0].Definition, Assets))), "External changed cargo spawn failed");
		Check(Capture.RequestCapture(), "External changed capture request failed");
		Step();
		Check(Capture.GetLastCapture()->GamePayload == "{\"collected\":[\"key\"],\"score\":99,\"spawnCounter\":2,\"unlocked\":false}" && Store.Load("manual").Document->Payload == SavedBytes, "External changed progress overwrote the saved checkpoint");
		Check(Capture.GetLastCapture()->Instances.Size() == 5, "External changed instance count differs");
		if (Observe)
		{
			Observe(*Current, "changed");
		}
	}
	else if (Mode == "read")
	{
		const auto SettingsDocument = Store.Load("settings");
		Check(SettingsDocument.Document && SettingsDocument.Document->Generation == 1, "External settings missing");
		const auto Profile = FUserSettingsCodec::Decode(*SettingsDocument.Document, Policy(), "consumer_checkpoint");
		Check(Profile.Volume == 0.4f && Profile.UiScale == 1.75f, "External restored operation environment differs");
		const auto Ticket = Resume.RequestLoad("manual");
		Resume.Poll();
		Check(Ticket.GetState() == EGameSaveRequestState::TransitionRequested, "External preparation mislabeled or failed");
		auto* Current = TickReady();
		Check(Ticket.GetState() == EGameSaveRequestState::Completed, "External resume published before Ready or not completed");
		Check(Current->GetGamePayload() == "{\"collected\":[\"key\"],\"score\":42,\"spawnCounter\":1,\"unlocked\":true}", "External independent restored progress differs");
		const auto& Captured = *Current->GetCheckpointCapture().GetLastCapture();
		Check(Captured.Identity.CheckpointKey == "north" && Captured.Instances.Size() == 4, "External checkpoint identity or history differs");
		Check(Captured.Instances[0].Parameters[0].Number == .1 && Captured.Instances[1].Parameters[0].Number == -.1 && Captured.Instances[3].Parameters[0].Number == 0, "External per-instance parameters changed");
		Check(!Captured.Instances[2].bPresent && Captured.Instances[3].Key == "cargo" && Captured.Instances[3].bDynamic, "External removed or dynamic instance differs");
		const auto Left = Current->GetPrefab("left");
		const auto Right = Current->GetPrefab("right");
		Check(!Left.Get()->GetRevoluteJoint("revolute").Get()->GetJointId(), "External disconnected Joint resurrected");
		Check(Left.Get()->GetDistanceJoint("distance").Get()->GetJointId() && Left.Get()->GetFixedJoint("fixed").Get()->GetJointId() && Left.Get()->GetPrismaticJoint("drive").Get()->GetJointId(), "External internal Joint missing");
		Check(Current->GetDistanceConnection("crossDistance").Get()->GetJointId() && Current->GetRevoluteConnection("crossRevolute").Get()->GetJointId() && Current->GetFixedConnection("crossFixed").Get()->GetJointId() && Current->GetPrismaticConnection("crossPrismatic").Get()->GetJointId(), "External four inter-instance connections missing");
		Check(Left.Get()->GetPrismaticJoint("drive").Get()->GetDescription().Joint.Drive.TargetSpeed == 0.4 && Right.Get()->GetPrismaticJoint("drive").Get()->GetDescription().Joint.Drive.TargetSpeed == -0.2, "External Joint state applied to another individual");
		Check(Current->GetPhysicsWorld().GetPosition(Current->GetPrefab("cargo").Get()->GetRigidBody("body").Get()->GetBodyId()).X > 7.9f, "External dynamic placement differs");
		// 読み戻したMapを実操作に使用する。保存ファイルがあるだけでは成功としない。
		FInputMap Map;
		FInputStateTracker Tracker;
		FRawInput Raw;
		Raw.Pads[3].bConnected = true;
		Tracker.Advance(Raw);
		FInputProfileCodec::Apply(Profile.Input, Policy(), Map, Tracker.GetSnapshot());
		Raw.Keys[static_cast<size_t>(EKey::Space)] = true;
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		Check(!Map.IsDown("Jump"), "External old Jump key still active");
		Raw.Keys[static_cast<size_t>(EKey::K)] = true;
		Raw.MouseButtons[static_cast<size_t>(EMouseButton::Right)] = true;
		Raw.Pads[3].Buttons[15] = true;
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		Check(Map.WasPressed("Jump") && Map.WasPressed("Interact") && Map.WasPressed("PadAction"), "External restored bindings do not operate");
		if (Map.WasPressed("Jump"))
		{
			auto Impulse = Current->GetPhysicsWorld().GetVelocity(Current->GetPrefab("cargo").Get()->GetRigidBody("body").Get()->GetBodyId());
			Impulse = {};
			Impulse.Y = 2;
			Current->GetPhysicsWorld().ApplyLinearImpulse(Current->GetPrefab("cargo").Get()->GetRigidBody("body").Get()->GetBodyId(), Impulse);
		}
		Check(Current->GetPhysicsWorld().GetVelocity(Current->GetPrefab("cargo").Get()->GetRigidBody("body").Get()->GetBodyId()).Y > 1.9f, "External remapped Jump did not reach the real Body");
		if (Observe)
		{
			Observe(*Current, "continued");
		}
	}
	else
	{
		throw FException("External checkpoint mode must be write or read");
	}
	Check(Resume.Retire(), "External resume retirement failed");
	Out << "CHECKPOINT_PROCESS_PASSED " << (Mode == "write" ? "write" : "read") << " dimension=" << Catalog.Identity.Dimension << '\n';
}
// 信頼する配布ファイルだけから許可カタログを構築する。
template <typename TCatalog, typename TRecipe, typename TDefinition, typename TPrefab>
TCatalog Catalog(TDefinition Definition, TPrefab Prefab, uint32 Dimension)
{
	TCatalog Value;
	Value.Identity.GameId = "consumer_checkpoint";
	Value.Identity.ContentRevision = "checkpoint-bundle-v1";
	Value.Identity.SceneKey = "course";
	Value.Identity.CheckpointKey = "start";
	Value.Identity.Dimension = Dimension;
	Value.Scene = Move(Definition);
	TRecipe Recipe;
	Recipe.Key = "mechanism";
	Recipe.Definition = Move(Prefab);
	// 許可した単一パラメーターを不変定義のコピーへ反映する。後からファイルを読み直さない。
	Recipe.Resolve = [Base = Recipe.Definition](const TVector<FContentParameterValue>& Values)
	{
		Check(Values.Size() == 1 && Values[0].Id == "speed" && Values[0].Kind == EContentParameterKind::Number && IsFinite(Values[0].Number) && Values[0].Number >= -2 && Values[0].Number <= 2, "External speed override invalid");
		auto Resolved = Base;
		Resolved.Parameters = Values;
		Resolved.Joints[3].Prismatic.Drive.TargetSpeed = Values[0].Number;
		return Resolved;
	};
	Value.Prefabs.PushBack(Move(Recipe));
	for (const auto& Id : Value.Scene.Instances)
	{
		(void)Id;
		Value.InitialPrefabKeys.PushBack("mechanism");
	}
	return Value;
}
} // namespace
void RunCheckpointProcess(FAssetService& Assets, FSceneNavigator& Scenes, const TFunction<void()>& Step, const TFunction<void(DScene&, const char*)>& Observe, const FPath& Root, const FPath& SaveRoot, FStringView Mode, uint32 Dimension, bool NativeAssets)
{
	FSceneContentSource Source(Root);
	const FString Prefix = NativeAssets ? "Data/nativecheckpoint" : "Data/checkpoint";
	if (Dimension == 2)
	{
		Play<FCheckpointCatalog2D, ACheckpointScene2D, FCheckpointResumeRequest2D>(Catalog<FCheckpointCatalog2D, FCheckpointPrefabRecipe2D>(Source.LoadScene2D(Prefix + "2d.dxfscene.json"), Source.LoadPrefab2D(Prefix + "2d.dxfprefab.json"), 2), Assets, Scenes, Step, Observe, SaveRoot, Mode);
	}
	else if (Dimension == 3)
	{
		Play<FCheckpointCatalog3D, ACheckpointScene3D, FCheckpointResumeRequest3D>(Catalog<FCheckpointCatalog3D, FCheckpointPrefabRecipe3D>(Source.LoadScene3D(Prefix + "3d.dxfscene.json"), Source.LoadPrefab3D(Prefix + "3d.dxfprefab.json"), 3), Assets, Scenes, Step, Observe, SaveRoot, Mode);
	}
	else
	{
		throw FException("External checkpoint dimension must be 2 or 3");
	}
}
