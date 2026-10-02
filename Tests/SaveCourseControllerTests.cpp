// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/ContentRenderBackend.h"
#include "Support/ContentModelBackend.h"
#include "SaveCourseRecipes.h"
#include "SaveCourseScene2D.h"
#include "SaveCourseScene3D.h"
#include "Dxf/Application.h"
#include "Dxf/InputStateTracker.h"
using namespace Dxf;
using namespace Dxf::GameplaySample;
using namespace Toolbox;
namespace
{
// UIの入力境界だけを置換する。実ApplicationとPhysicsは置換しない。
class FBlockingRouter final : public IInputRouter
{
public:
	FInputSnapshot RouteInput(const FTickContext& Context) override
	{
		return bBlocked ? FInputSnapshot{} : Context.Input;
	}
	// Modalと、その閉じたFrameに相当する消費。
	bool bBlocked = true;
};
// Sensorへ重ねる配置は試験自身が明示する。製品ControllerはTeleportしない。
template <typename TScene, typename TBodyId>
void PlacePlayer(TScene& Scene, TBodyId Body, f32 X, f32 Y)
{
	if constexpr (IsSame<TScene, ASaveCourseScene2D>)
	{
		Scene.GetPhysicsWorld().SetBodyTransform(Body, Toolbox::FVector2{X, Y}, 0);
	}
	else
	{
		Scene.GetPhysicsWorld().SetBodyTransform(Body, Toolbox::FVector3{X, Y, 0}, {});
	}
	Scene.GetPhysicsWorld().SetVelocity(Body, {});
}
// ゲームを操作し、模倣通知を使わずに本体Sensorから進行を採取する。
template <typename TCatalog, typename TScene>
void RunController(const TCatalog& Catalog)
{
	Testing::FFakeBackend Backend;
	Testing::FContentRenderBackend Renderer(Backend);
	Testing::FContentModelBackend Models;
	FApplicationSettings Settings;
	Settings.ProjectRoot = DXF_CONTENT_SAMPLE_ROOT;
	Settings.ExecutionThreadCount = 1;
	FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer, &Models}, Settings);
	REQUIRE(App.Start(MakeUnique<DScene>()));
	REQUIRE(App.Step(0));
	const auto Initial = FSaveCourseRecipes::NewGame(Catalog);
	const auto Plan = FCheckpointRestore::Build(Initial, Catalog);
	auto Scene = MakeUnique<TScene>(PrepareScene(Plan.Scene, App.GetAssets()), Initial, PreparePrefab(Catalog.Prefabs.Back().Definition, App.GetAssets()));
	auto* Current = Scene.Get();
	REQUIRE(App.GetScenes().RequestChange(Move(Scene)));
	REQUIRE(App.Step(0));
	REQUIRE(Backend.GetTrace().Clones == 0);
	// 物理登録待ちと、Scene採用後の成功Readyを分ける。
	f64 Now = 0;
	auto Tick = [&](f64 Seconds = 1.0 / 60.0)
	{
		Now += Seconds;
		const auto Result = App.Step(Now);
		if (!Result)
		{
			throw FException(Result.Error().Message);
		}
		REQUIRE(Result.Value());
	};
	Tick(.02);
	REQUIRE(Current->GetResumeTicket().GetState() == ECheckpointSceneState::Ready);
	REQUIRE(!Current->RequestDoor(0, true));
	const auto Player = Current->GetPrefab("player").Get()->GetRigidBody("body");
	const auto Body = Player.Get()->GetBodyId();
	const auto Key = Current->GetPrefab("northKey");
	const auto OtherDoor = Current->GetPrefab("doorB");
	const auto OtherId = *OtherDoor.Get()->GetPrismaticJoint("doorDrive").Get()->GetJointId();
	PlacePlayer(*Current, Body, -5, 2);
	Tick();
	REQUIRE(Current->GetProgress().Score == 10);
	REQUIRE(Current->GetProgress().bDoorUnlocked[0] && !Current->GetProgress().bDoorUnlocked[1]);
	REQUIRE(!Key && OtherDoor);
	REQUIRE(Backend.GetTrace().Clones == 1);
	Tick();
	REQUIRE(Current->GetProgress().Score == 10 && Backend.GetTrace().Clones == 1);
	// 取得済みと除去履歴が同じ成功境界の文書へ入る。
	const auto& Taken = Current->GetCheckpointCapture().GetLastCapture();
	REQUIRE(Taken && FSaveCourseProgressCodec::Decode(Taken->GamePayload).Score == 10);
	REQUIRE(!Taken->Instances[3].bPresent);
	// CheckpointのStayでは同じrevisionの要求を繰り返さない。
	PlacePlayer(*Current, Body, 0, 5);
	Tick();
	REQUIRE(Current->GetProgress().Checkpoint == "north");
	const auto Revision = Current->GetProgress().Revision;
	const auto Captured = Current->GetCheckpointCapture().GetCapturedStep();
	Tick();
	REQUIRE(Current->GetProgress().Revision == Revision);
	REQUIRE(Current->GetCheckpointCapture().GetCapturedStep() == Captured);
	// 公開先から片側だけ動かす。描画のView数で更新や生成を増やさない。
	PlacePlayer(*Current, Body, -6, -2);
	REQUIRE(Current->RequestDoor(0, true));
	Current->SetViews(Current->GetDefinition().Views[0], Current->GetDefinition().Views[1]);
	Current->RequestCargo();
	Tick();
	Tick();
	REQUIRE(Current->GetProgress().SpawnCounter == 1);
	REQUIRE(Current->GetPrefab("cargo1").Get()->GetState() == EPrefabInstanceState::Ready);
	for (uint32 Index = 0; Index < 240; ++Index)
	{
		Tick();
	}
	REQUIRE(Current->GetProgress().bDoorOpen[0] && !Current->GetProgress().bDoorOpen[1]);
	REQUIRE(*OtherDoor.Get()->GetPrismaticJoint("doorDrive").Get()->GetJointId() == OtherId);
	REQUIRE(Abs(OtherDoor.Get()->GetPrismaticJoint("doorDrive").Get()->GetObservation()->State.Translation) < .001);
	// 固定更新0回のFrameでは、表示数を変えてもWorld・進行・採取を進めない。
	auto Drift = Current->GetPhysicsWorld().GetVelocity(Body);
	Drift.X = .25f;
	Drift.Y = .1f;
	Current->GetPhysicsWorld().SetVelocity(Body, Drift);
	const auto Position = Current->GetPhysicsWorld().GetPosition(Body);
	const auto DrawStep = Current->GetCompletedPhysicsStepCount();
	const auto DrawRevision = Current->GetProgress().Revision;
	const auto DrawCapture = Current->GetCheckpointCapture().GetCapturedStep();
	Current->SetViews(Current->GetDefinition().Views[0]);
	Tick(0);
	REQUIRE(Current->GetPhysicsWorld().GetPosition(Body).X == Position.X);
	Current->SetViews(Current->GetDefinition().Views[0], Current->GetDefinition().Views[1]);
	Tick(0);
	REQUIRE(Current->GetPhysicsWorld().GetPosition(Body).X == Position.X);
	REQUIRE(Current->GetCompletedPhysicsStepCount() == DrawStep);
	REQUIRE(Current->GetProgress().Revision == DrawRevision);
	REQUIRE(Current->GetCheckpointCapture().GetCapturedStep() == DrawCapture);
	REQUIRE(Current->GetProgress().SpawnCounter == 1);
	// JumpをKへ変えて、適用時の押し続けと旧Spaceが実Bodyへ漏れないことを確認する。
	auto Changed = FSaveCourseInput::Defaults();
	for (auto& Action : Changed.Input.Actions)
	{
		if (Action.Action == "Jump")
		{
			Action.Bindings[0].Key = EKey::K;
		}
	}
	Changed.Volume = .4f;
	FRawInput Held;
	Held.Keys[static_cast<size_t>(EKey::K)] = true;
	FInputStateTracker Tracker;
	Tracker.Advance(Held);
	Current->ApplySettings(Changed, Tracker.GetSnapshot());
	PlacePlayer(*Current, Body, -6, -2);
	Backend.GetTrace().Input = Held;
	Tick();
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y) < .001);
	for (const auto& [Handle, Volume] : Backend.GetTrace().Volumes)
	{
		(void)Handle;
		REQUIRE(Volume == .4f);
	}
	Backend.GetTrace().Input = {};
	Tick();
	Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::Space)] = true;
	Tick();
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y) < .001);
	Backend.GetTrace().Input = {};
	Tick();
	// 0固定更新で押して離したKを次の複数固定更新で一度だけ消費する。
	Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::K)] = true;
	Tick(0);
	Backend.GetTrace().Input = {};
	Tick(0);
	Tick(3.0 / 60.0);
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y - 2) < .02);
	// UIが消費した押し続けは、Modalが閉じても解放するまで無効。
	FBlockingRouter Router;
	Current->SetInputRouter(&Router);
	PlacePlayer(*Current, Body, -6, -2);
	Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::K)] = true;
	Tick();
	Router.bBlocked = false;
	Tick();
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y) < .001);
	Backend.GetTrace().Input = {};
	Tick();
	Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::K)] = true;
	Tick();
	REQUIRE(Current->GetPhysicsWorld().GetVelocity(Body).Y > 1.9f);
	// 同じゲームActionをマウスへ替え、旧Kでは操作できないことを実Bodyで確認する。
	Current->SetInputRouter(nullptr);
	Backend.GetTrace().Input = {};
	for (auto& Action : Changed.Input.Actions)
	{
		if (Action.Action == "Jump")
		{
			Action.Bindings[0].Device = EInputBindingDevice::Mouse;
			Action.Bindings[0].MouseButton = EMouseButton::Right;
		}
	}
	Current->ApplySettings(Changed, {});
	PlacePlayer(*Current, Body, -6, -2);
	Tick();
	Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::K)] = true;
	Tick();
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y) < .001);
	Backend.GetTrace().Input = {};
	Tick();
	Backend.GetTrace().Input.MouseButtons[static_cast<size_t>(EMouseButton::Right)] = true;
	Tick();
	REQUIRE(Current->GetPhysicsWorld().GetVelocity(Body).Y > 1.9f);
	// Pad4の16番ボタン。固定更新を待つ間の切断で保留押下を捨てる。
	for (auto& Action : Changed.Input.Actions)
	{
		if (Action.Action == "Jump")
		{
			Action.Bindings[0].Device = EInputBindingDevice::Gamepad;
			Action.Bindings[0].PadSlot = 3;
			Action.Bindings[0].PadButton = 15;
		}
	}
	Backend.GetTrace().Input = {};
	Backend.GetTrace().Input.Pads[3].bConnected = true;
	Tracker.Advance(Backend.GetTrace().Input);
	Current->ApplySettings(Changed, Tracker.GetSnapshot());
	PlacePlayer(*Current, Body, -6, -2);
	Tick();
	Backend.GetTrace().Input.Pads[3].Buttons[15] = true;
	Tick(0);
	Backend.GetTrace().Input.Pads[3].bConnected = false;
	Tick();
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y) < .001);
	Backend.GetTrace().Input.Pads[3].bConnected = true;
	Tick();
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y) < .001);
	Backend.GetTrace().Input.Pads[3].Buttons[15] = false;
	Tick();
	Backend.GetTrace().Input.Pads[3].Buttons[15] = true;
	Tick();
	REQUIRE(Current->GetPhysicsWorld().GetVelocity(Body).Y > 1.9f);
	// フォーカスを失うまでに保留した押下も、後からゲームへ配送しない。
	PlacePlayer(*Current, Body, -6, -2);
	Backend.GetTrace().Input.Pads[3].Buttons[15] = false;
	Tick();
	Backend.GetTrace().Input.Pads[3].Buttons[15] = true;
	Tick(0);
	Backend.GetTrace().Input.bFocused = false;
	Tick();
	Backend.GetTrace().Input.bFocused = true;
	Tick();
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y) < .001);
	const auto Saved = *Current->GetCheckpointCapture().GetLastCapture();
	REQUIRE(FSaveCourseProgressCodec::Decode(Saved.GamePayload).SpawnCounter == 1);
	// 新しいSceneとWorldへ戻す。同一プロセス試験であり別プロセス証拠にはしない。
	const auto Restored = FSaveCourseRecipes::Restore(Saved);
	const auto Resume = FCheckpointRestore::Build(Restored, Catalog);
	auto Next = MakeUnique<TScene>(PrepareScene(Resume.Scene, App.GetAssets()), Restored, PreparePrefab(Catalog.Prefabs.Back().Definition, App.GetAssets()), Changed);
	auto* Resumed = Next.Get();
	REQUIRE(App.GetScenes().RequestChange(Move(Next)));
	Backend.GetTrace().Input = {};
	Tick(0);
	Tick();
	Tick();
	REQUIRE(!Player && !OtherDoor);
	REQUIRE(Resumed->GetProgress().Score == 10 && Resumed->GetProgress().Checkpoint == "north");
	REQUIRE(Resumed->GetProgress().Revision == FSaveCourseProgressCodec::Decode(Saved.GamePayload).Revision);
	REQUIRE(Resumed->GetPrefab("cargo1").Get()->GetState() == EPrefabInstanceState::Ready);
	REQUIRE(Backend.GetTrace().Clones == 1);
	App.GetScenes().RequestQuit();
	const auto Quit = App.Step(Now);
	REQUIRE(Quit && !Quit.Value());
	REQUIRE(Backend.GetTrace().Sounds.IsEmpty());
}
} // namespace
TEST("Save course 2D controller captures real sensors drives spawns and consumes remapped actions once")
{
	FSceneContentSource Source{FPath(DXF_CONTENT_SAMPLE_ROOT)};
	RunController<FCheckpointCatalog2D, ASaveCourseScene2D>(FSaveCourseRecipes::Load2D(Source));
}
TEST("Save course 3D controller captures real sensors drives spawns and consumes remapped actions once")
{
	FSceneContentSource Source{FPath(DXF_CONTENT_SAMPLE_ROOT)};
	RunController<FCheckpointCatalog3D, ASaveCourseScene3D>(FSaveCourseRecipes::Load3D(Source));
}
