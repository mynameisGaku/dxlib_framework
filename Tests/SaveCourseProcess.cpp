// SPDX-License-Identifier: NOASSERTION
#include "Support/ContentRenderBackend.h"
#include "Support/ContentModelBackend.h"
#include "SaveCourseGameInstance.h"
#include "SaveCourseScene2D.h"
#include "SaveCourseScene3D.h"
#include "Dxf/Application.h"
#include "Dxf/SaveStore.h"
#include "Toolbox/Thread.h"
#include "Toolbox/JsonWriter.h"
using namespace Dxf;
using namespace Dxf::GameplaySample;
using namespace Toolbox;
namespace
{
// 各プロセスの期待値を独立した失敗として返す。
void Check(bool Condition, const char* Reason)
{
	if (!Condition)
	{
		throw FException(Reason);
	}
}
// 生存構成の定義を確認する。未知IDのgetterを失効handleとして扱わない。
template <typename TScene>
bool HasInstance(const TScene& Scene, FStringView Key)
{
	for (const auto& Id : Scene.GetDefinition().Instances)
	{
		if (Id == Key)
		{
			return true;
		}
	}
	return false;
}
// 進行を起こす固定入力の試験配置。製品はSensorとControllerの通常経路を使う。
template <typename TScene, typename TBody>
void Place(TScene& Scene, TBody Body, f32 X, f32 Y)
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
// 全所有者をこの新プロセス内で作る。前プロセスのメモリを参照しない。
template <typename TScene>
void Run(FStringView Mode, uint32 Dimension, const FPath& Root, const FPath& Project)
{
	Testing::FFakeBackend Backend;
	Testing::FContentRenderBackend Renderer(Backend);
	Testing::FContentModelBackend Models;
	FApplicationSettings Settings;
	Settings.ProjectRoot = Project.ToUtf8();
	Settings.ExecutionThreadCount = 2;
	auto Owner = MakeUnique<ASaveCourseGameInstance>(Root, FSceneContentSource(Project));
	auto* Game = Owner.Get();
	FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer, &Models}, Settings, Move(Owner));
	Check(static_cast<bool>(App.Start(MakeUnique<DScene>())), "Application start failed");
	f64 Now = 0;
	auto Tick = [&](f64 Seconds = 1.0 / 60.0)
	{
		Now += Seconds;
		const auto Result = App.Step(Now);
		if (!Result)
		{
			throw FException(Result.Error().Message);
		}
		Check(Result.Value(), "Unexpected Application stop");
	};
	Tick(0);
	auto Wait = [&](bool bSettings, EGameSaveRequestState State)
	{
		for (uint32 Attempt = 0; Attempt < 10000; ++Attempt)
		{
			const auto Ticket = bSettings ? Game->GetSettingsTicket() : Game->GetSaveTicket();
			if (Ticket.GetState() == State)
			{
				return;
			}
			if (Ticket.GetState() == EGameSaveRequestState::Failed)
			{
				throw FException(Ticket.GetReason());
			}
			Tick(0);
			FThread::Yield();
		}
		throw FException("Process request did not reach expected boundary");
	};
	if (Mode == "write")
	{
		Game->RequestNewGame(Dimension);
		Tick(0);
		Tick(.02);
		Tick();
		auto* Scene = App.GetScenes().GetCurrent()->TryCast<TScene>();
		Check(Scene != nullptr && Scene->GetResumeTicket().GetState() == ECheckpointSceneState::Ready, "New Game not Ready");
		const auto Body = Scene->GetPrefab("player").Get()->GetRigidBody("body").Get()->GetBodyId();
		Check(Scene->GetProgress().Score == 0 && Scene->GetProgress().SpawnCounter == 0, "Unexpected initial progress");
		Place(*Scene, Body, -5, 2);
		Tick();
		Check(Scene->GetProgress().Score == 10 && Scene->GetProgress().CollectedKeys.Size() == 1, "Key was not collected");
		Place(*Scene, Body, -6, -2);
		Tick();
		// ゲームActionのGで荷物を生成する。UIや描画からは生成しない。
		Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::G)] = true;
		Tick();
		Backend.GetTrace().Input = {};
		Tick();
		Check(Scene->RequestDoor(0, true), "Unlocked door cannot open");
		for (uint32 Step = 0; Step < 240; ++Step)
		{
			Tick();
		}
		Check(Scene->GetProgress().bDoorOpen[0] && !Scene->GetProgress().bDoorOpen[1], "Independent door state failed");
		Place(*Scene, Body, 0, 5);
		Tick();
		Tick();
		Wait(false, EGameSaveRequestState::Committed);
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
		Wait(true, EGameSaveRequestState::Committed);
		Tick(0);
		Check(!Game->IsSettingsDirty(), "Settings reported unsaved after commit");
		FSaveStore Store(Root);
		Check(Store.Load("autosave").Document->Generation == 1, "Checkpoint Stay wrote duplicate saves");
		Out << "PROCESS_A initialScore=0 savedScore=10 checkpoint=north cargo=1 doorA=open doorB=closed jump=K\n";
	}
	else if (Mode == "read")
	{
		Check(Game->GetSettings().Volume == .35f && Game->GetSettings().UiScale == 1.5f && !Game->IsSettingsDirty(), "Fresh process settings did not persist");
		Game->RequestLoad(Dimension, "autosave");
		Tick(0);
		Wait(false, EGameSaveRequestState::TransitionRequested);
		Tick(0);
		auto* Scene = App.GetScenes().GetCurrent()->TryCast<TScene>();
		Check(Scene != nullptr && Scene->GetResumeTicket().GetState() == ECheckpointSceneState::PendingPhysics, "Load claimed Ready before physics registration");
		Tick();
		Wait(false, EGameSaveRequestState::Completed);
		const auto& Progress = Scene->GetProgress();
		Check(Progress.Score == 10 && Progress.Checkpoint == "north" && Progress.SpawnCounter == 1, "Fresh process progress did not persist");
		Check(Progress.bDoorUnlocked[0] && Progress.bDoorOpen[0] && !Progress.bDoorUnlocked[1] && !Progress.bDoorOpen[1], "Mechanism state applied to wrong instance");
		Check(!HasInstance(*Scene, "northKey") && HasInstance(*Scene, "southKey"), "Collected object resurrected");
		Check(Scene->GetPrefab("cargo1").Get()->GetState() == EPrefabInstanceState::Ready, "Dynamic cargo missing");
		const auto DoorA = Scene->GetPrefab("doorA").Get();
		const auto DoorB = Scene->GetPrefab("doorB").Get();
		Check(Abs(DoorA->GetPrismaticJoint("doorDrive").Get()->GetObservation()->State.Translation - 2) < .002, "Door is not open at first normal Step");
		Check(Abs(DoorB->GetPrismaticJoint("doorDrive").Get()->GetObservation()->State.Translation) < .002, "Other door changed");
		Check(DoorA->GetDistanceJoint("hanger") && DoorA->GetRevoluteJoint("armDrive") && DoorA->GetFixedJoint("assembly"), "Joint logical reconstruction failed");
		Check(Backend.GetTrace().Clones == 0, "Resume replayed acquisition sound");
		const auto Body = Scene->GetPrefab("player").Get()->GetRigidBody("body").Get()->GetBodyId();
		const auto Position = Scene->GetPhysicsWorld().GetPosition(Body);
		Check(Abs(Position.X) < .002 && Abs(Position.Y - 5) < .002, "Player did not resume at checkpoint");
		const auto Revision = Progress.Revision;
		Tick();
		Check(Scene->GetProgress().Score == 10 && Scene->GetProgress().Revision == Revision && Backend.GetTrace().Clones == 0, "Resume sensor duplicated progress or sound");
		Place(*Scene, Body, -6, -2);
		Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::Space)] = true;
		Tick();
		Check(Abs(Scene->GetPhysicsWorld().GetVelocity(Body).Y) < .001, "Old Jump binding still active");
		Backend.GetTrace().Input = {};
		Tick();
		Backend.GetTrace().Input.Keys[static_cast<size_t>(EKey::K)] = true;
		Tick();
		Check(Scene->GetPhysicsWorld().GetVelocity(Body).Y > 1.9f, "Persisted Jump binding did not operate real Body");
		Check(FSaveStore(Root).Load("autosave").Document->Generation == 1, "Continue rewrote same checkpoint");
		Out << "PROCESS_B score=10 checkpoint=north cargo=1 doorA=open doorB=closed jump=K active\n";
	}
	else
	{
		throw FException("Unknown process mode");
	}
	App.GetScenes().RequestQuit();
	const auto Quit = App.Step(Now);
	Check(Quit && !Quit.Value(), "Application shutdown failed");
	Check(Backend.GetTrace().Sounds.IsEmpty(), "Scene audio scope leaked");
	Out << "SAVE_COURSE_PROCESS_PASSED dimension=" << Dimension << " mode=" << FString(Mode.Data(), Mode.Size()) << '\n';
}
// 起動ごとに同じC++入口から次元を確定する。
int32 Entry(FString Mode, FString Dimension, FPath Root, FPath Project)
{
	if (Dimension == "2")
	{
		Run<ASaveCourseScene2D>(Mode, 2, Root, Project);
	}
	else if (Dimension == "3")
	{
		Run<ASaveCourseScene3D>(Mode, 3, Root, Project);
	}
	else
	{
		throw FException("Process dimension must be 2 or 3");
	}
	return 0;
}
} // namespace
#if defined(_WIN32)
// WindowsのUnicode引数をToolboxへ渡す。intは実行入口のABIに限る。
int wmain(int Count, wchar_t** Arguments)
{
	try
	{
		if (Count != 5)
		{
			throw FException("Use write/read, dimension, absolute SaveRoot, absolute ProjectRoot");
		}
		return Entry(FromWide(FWideString(Arguments[1])), FromWide(FWideString(Arguments[2])), FPath(Arguments[3]), FPath(Arguments[4]));
	}
	catch (const FException& Error)
	{
		Err << Error.What() << '\n';
		return 1;
	}
}
#else
// 同じ文書をPOSIX入口でも検査する。
int main(int Count, char** Arguments)
{
	try
	{
		if (Count != 5)
		{
			throw FException("Use write/read, dimension, absolute SaveRoot, absolute ProjectRoot");
		}
		return Entry(FString(Arguments[1]), FString(Arguments[2]), FPath(Arguments[3]), FPath(Arguments[4]));
	}
	catch (const FException& Error)
	{
		Err << Error.What() << '\n';
		return 1;
	}
}
#endif
