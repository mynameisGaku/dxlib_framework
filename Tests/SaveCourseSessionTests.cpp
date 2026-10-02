// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/ContentRenderBackend.h"
#include "Support/ContentModelBackend.h"
#include "SaveCourseGameInstance.h"
#include "SaveCourseScene2D.h"
#include "SaveCourseScene3D.h"
#include "Dxf/Application.h"
#include "Dxf/SaveStore.h"
#include "Toolbox/Thread.h"
#include "Toolbox/JsonWriter.h"
#include "Toolbox/FileLock.h"
using namespace Dxf;
using namespace Dxf::GameplaySample;
using namespace Toolbox;
namespace
{
// 通知を模倣せず、試験が明示した配置から本体Sensorを動かす。
template <typename TScene, typename TBody>
void PlacePlayer(TScene& Scene, TBody Body, f32 X, f32 Y)
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
// 実Applicationが所有するGameInstanceから保存と通常Navigator再開を通す。
template <typename TScene>
void RunSession(uint32 Dimension, bool bAsync)
{
	const FPath Parent(DXF_SAVE_COURSE_TEST_ROOT);
	(void)CreateDirectory(Parent);
	const FPath Root = Parent / FPath(FString("Session") + FJsonWriter::Unsigned(MonotonicNanoseconds()));
	(void)CreateDirectory(Root);
	Testing::FFakeBackend Backend;
	Testing::FContentRenderBackend Renderer(Backend);
	Testing::FContentModelBackend Models;
	FApplicationSettings Settings;
	Settings.ProjectRoot = DXF_CONTENT_SAMPLE_ROOT;
	Settings.ExecutionThreadCount = 2;
	auto Owner = MakeUnique<ASaveCourseGameInstance>(Root, FSceneContentSource{FPath(DXF_CONTENT_SAMPLE_ROOT)}, bAsync);
	auto* Game = Owner.Get();
	FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer, &Models}, Settings, Move(Owner));
	REQUIRE(App.Start(MakeUnique<DScene>()));
	REQUIRE(App.Step(0));
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
	// CPU完了の待機では追加World.Stepを作らない。
	auto Wait = [&](bool bSettings, EGameSaveRequestState Expected)
	{
		// Workerへ移したCPU準備を更新回数ではなく、有限の実時間で待つ。
		const auto Started = MonotonicNanoseconds();
		while (MonotonicNanoseconds() - Started < 5000000000ULL)
		{
			const auto Ticket = bSettings ? Game->GetSettingsTicket() : Game->GetSaveTicket();
			if (Ticket.GetState() == Expected)
			{
				return;
			}
			if (Ticket.GetState() == EGameSaveRequestState::Failed && Expected != EGameSaveRequestState::Failed)
			{
				throw FException(Ticket.GetReason());
			}
			Tick(0);
			FThread::Yield();
		}
		const auto Ticket = bSettings ? Game->GetSettingsTicket() : Game->GetSaveTicket();
		Err << "SESSION_WAIT expected=" << static_cast<uint32>(Expected) << " actual=" << static_cast<uint32>(Ticket.GetState()) << " elapsedNs=" << MonotonicNanoseconds() - Started << '\n';
		throw FException("Save session request did not reach expected terminal boundary within five seconds");
	};
	Game->RequestNewGame(Dimension);
	Tick(0);
	Tick(.02);
	Tick();
	auto* Current = App.GetScenes().GetCurrent()->TryCast<TScene>();
	REQUIRE(Current != nullptr && Current->GetResumeTicket().GetState() == ECheckpointSceneState::Ready);
	const auto OldPlayer = Current->GetPrefab("player").Get()->GetRigidBody("body");
	const auto Body = OldPlayer.Get()->GetBodyId();
	PlacePlayer(*Current, Body, -5, 2);
	Tick();
	REQUIRE(Current->GetProgress().Score == 10);
	PlacePlayer(*Current, Body, 0, 5);
	Tick();
	Tick();
	Wait(false, EGameSaveRequestState::Committed);
	FSaveStore Store(Root);
	REQUIRE(Store.Load("autosave").Document->Generation == 1);
	for (uint32 Index = 0; Index < 4; ++Index)
	{
		Tick();
	}
	REQUIRE(Store.Load("autosave").Document->Generation == 1);
	// Pauseでは最後の正常採取を使い、保存のために物理を進めない。
	Current->GetClock().SetPaused(true);
	const auto Steps = Current->GetCompletedPhysicsStepCount();
	Game->RequestSave("manual1");
	Tick();
	Wait(false, EGameSaveRequestState::Committed);
	REQUIRE(Current->GetCompletedPhysicsStepCount() == Steps);
	REQUIRE(Store.Load("manual1").Document->CheckpointKey == "north");
	auto Changed = FSaveCourseInput::Defaults();
	Changed.Volume = .35f;
	Changed.UiScale = 1.5f;
	for (auto& Action : Changed.Input.Actions)
	{
		if (Action.Action == "Jump")
		{
			Action.Bindings[0].Key = EKey::K;
		}
	}
	Game->RequestSettings(Changed);
	Game->RequestSettingsSave();
	Tick();
	REQUIRE(Game->GetSettings().Volume == .35f && Game->IsSettingsDirty());
	Wait(true, EGameSaveRequestState::Committed);
	Tick(0);
	REQUIRE(!Game->IsSettingsDirty());
	REQUIRE(Store.Load("settings").Document->Kind == ESaveDocumentKind::Settings);
	// 異次元の準備失敗では旧Sceneと現在設定を保持する。
	Game->RequestLoad(Dimension == 2 ? 3 : 2, "manual1");
	Tick(0);
	Wait(false, EGameSaveRequestState::Failed);
	REQUIRE(App.GetScenes().GetCurrent() == Current);
	REQUIRE(Game->GetSettings().UiScale == 1.5f);
	// 受付直後の取消しは候補Sceneも保存面も採用しない。
	Game->RequestLoad(Dimension, "manual1");
	Tick(0);
	const auto Canceling = Game->GetSaveTicket();
	Game->RequestCancel();
	Tick(0);
	REQUIRE(Canceling.GetState() == EGameSaveRequestState::Canceled);
	REQUIRE(App.GetScenes().GetCurrent() == Current);
	// AのCPU完了をPollする前にBを受付し、古いAでSceneを切り替えない。
	Game->RequestLoad(Dimension, "manual1");
	Tick(0);
	const auto Superseded = Game->GetSaveTicket();
	Game->RequestLoad(Dimension == 2 ? 3 : 2, "manual1");
	Tick(0);
	Wait(false, EGameSaveRequestState::Failed);
	REQUIRE(Superseded.GetState() == EGameSaveRequestState::Superseded);
	REQUIRE(App.GetScenes().GetCurrent() == Current);
	Game->RequestNewGame(Dimension);
	Tick(0);
	Tick();
	Current = App.GetScenes().GetCurrent()->TryCast<TScene>();
	REQUIRE(Current != nullptr && Current->GetProgress().Score == 0 && !OldPlayer);
	Game->RequestLoad(Dimension, "manual1");
	Tick(0);
	Wait(false, EGameSaveRequestState::TransitionRequested);
	// RequestChangeの受付と、次のApplication境界のScene採用は別。
	REQUIRE(App.GetScenes().GetCurrent() == Current);
	Tick(0);
	Current = App.GetScenes().GetCurrent()->TryCast<TScene>();
	REQUIRE(Current != nullptr && Current->GetResumeTicket().GetState() == ECheckpointSceneState::PendingPhysics);
	REQUIRE(Game->GetSaveTicket().GetState() != EGameSaveRequestState::Completed);
	Tick();
	Wait(false, EGameSaveRequestState::Completed);
	REQUIRE(Current->GetProgress().Score == 10 && Current->GetProgress().Checkpoint == "north");
	REQUIRE(Game->GetSettings().Volume == .35f && Game->GetSettings().UiScale == 1.5f);
	Tick();
	REQUIRE(Store.Load("autosave").Document->Generation == 1);
#if defined(_WIN32)
	// 設定は有効に適用しても、公開を確認するまでは保存済みへ変えない。
	TVector<uint8> ProtectedBytes;
	REQUIRE(ReadFileBytes(Root / FPath("settings.a"), ProtectedBytes, 8 * 1024 * 1024));
	const FString Protected(reinterpret_cast<const char*>(ProtectedBytes.Data()), ProtectedBytes.Size());
	auto Unsaved = Changed;
	Unsaved.Volume = .6f;
	Game->RequestSettings(Unsaved);
	Game->RequestSettingsSave();
	Tick(0);
	if (bAsync)
	{
		App.GetTaskDispatcher().WaitForPrepares();
		App.GetTaskDispatcher().PumpCommits();
	}
	REQUIRE(Game->GetSettingsTicket().GetState() == EGameSaveRequestState::Prepared);
	{
		// Stageの読取・write完了後、公開先だけを排他にして読戻しを失敗させる。
		FFileLock LockedDestination(Root / FPath("settings.b"));
		Wait(true, EGameSaveRequestState::PublicationUnknown);
		REQUIRE(Game->GetSettings().Volume == .6f && Game->IsSettingsDirty());
		REQUIRE(!Game->GetSettingsTicket().GetReason().IsEmpty());
	}
	// 結果不明のTransactionはスロット排他を保持する。保護面だけを直接比較する。
	TVector<uint8> RetainedBytes;
	REQUIRE(ReadFileBytes(Root / FPath("settings.a"), RetainedBytes, 8 * 1024 * 1024));
	REQUIRE(FString(reinterpret_cast<const char*>(RetainedBytes.Data()), RetainedBytes.Size()) == Protected);
	Tick(0);
	REQUIRE(Game->GetSettingsTicket().GetState() == EGameSaveRequestState::PublicationUnknown);
	REQUIRE(Game->IsSettingsDirty());
	Game->RequestPublicationRetry(true);
	Tick(0);
	Wait(true, EGameSaveRequestState::Committed);
	Tick(0);
	REQUIRE(!Game->IsSettingsDirty());
	REQUIRE(Store.Load("settings").Document->Generation == 2);
	REQUIRE(FUserSettingsCodec::Decode(*Store.Load("settings").Document, FSaveCourseInput::Policy(), "dxfGameplaySave").Volume == .6f);
#endif
	if (!bAsync)
	{
		// 候補Sceneの構築と採用の間に設定を変更しても、最新の操作へ切り替える。
		Game->RequestLoad(Dimension, "manual1");
		Tick(0);
		auto Latest = Changed;
		for (auto& Action : Latest.Input.Actions)
		{
			if (Action.Action == "Jump")
			{
				Action.Bindings[0].Key = EKey::L;
			}
		}
		Latest.Volume = .8f;
		Game->RequestSettings(Latest);
		Tick(0);
		REQUIRE(Game->GetSaveTicket().GetState() == EGameSaveRequestState::TransitionRequested);
		Tick(0);
		Tick();
		Wait(false, EGameSaveRequestState::Completed);
		Current = App.GetScenes().GetCurrent()->TryCast<TScene>();
		REQUIRE(Current != nullptr && Game->GetSettings().Volume == .8f && Game->IsSettingsDirty());
		const auto NewBody = Current->GetPrefab("player").Get()->GetRigidBody("body").Get()->GetBodyId();
		PlacePlayer(*Current, NewBody, -6, -3);
		Tick();
		Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::K)] = true;
		Tick(.06);
		REQUIRE(Current->GetPhysicsWorld().GetVelocity(NewBody).Y == 0);
		Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::K)] = false;
		Tick(0);
		Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::L)] = true;
		Tick(.06);
		REQUIRE(Current->GetPhysicsWorld().GetVelocity(NewBody).Y > 1.9f);
	}
	App.GetScenes().RequestQuit();
	const auto Quit = App.Step(Now);
	REQUIRE(Quit && !Quit.Value());
}
} // namespace
TEST("Save course 2D session keeps settings separate and completes after physics readiness")
{
	RunSession<ASaveCourseScene2D>(2, false);
	RunSession<ASaveCourseScene2D>(2, true);
}
TEST("Save course 3D session keeps settings separate and completes after physics readiness")
{
	RunSession<ASaveCourseScene3D>(3, false);
	RunSession<ASaveCourseScene3D>(3, true);
}
