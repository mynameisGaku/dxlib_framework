// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Support/CheckpointFixture.h"
#include "Dxf/CheckpointResumeRequest2D.h"
#include "Dxf/CheckpointResumeRequest3D.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/AssetService.h"
#include "Toolbox/ConditionVariable.h"
#include "Toolbox/Thread.h"
using namespace Dxf;
using namespace Toolbox;
using namespace TestCheckpointFixture;
namespace
{
// 構築の手作り対照を残し、再開中の旧Scene生存を観察する。
class APreviousScene : public DScene
{
};
template <typename TScene>
class TRejectedCheckpointScene : public TScene
{
public:
	using TScene::TScene;

protected:
	TResult<void> OnInitialize(const FInitContext& Context) override
	{
		const auto Base = TScene::OnInitialize(Context);
		if (!Base)
		{
			return Base;
		}
		return TResult<void>::Failure(EErrorCode::InitializationFailed, "Injected checkpoint initialization failure");
	}
};
void NavigatorTick(FSceneNavigator& Scenes, f64 Delta)
{
	FInputStateTracker Input;
	FFrameTime Time;
	Time.DeltaSeconds = Delta;
	Time.UnscaledDeltaSeconds = Delta;
	REQUIRE(Scenes.CommitObjects());
	REQUIRE(Scenes.Tick(Time, Input.GetSnapshot()));
}
template <typename TResume, typename TScene, typename TCatalog>
void ResumeBoundary(TCatalog Catalog, const char* Slot)
{
	Dxf::Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FAudioPlayer Audio(Backend);
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<APreviousScene>());
	REQUIRE(Scenes.Commit());
	const auto* Old = Scenes.GetCurrent();
	auto Saved = Document(Catalog);
	FSaveStore Store{FPath(DXF_CHECKPOINT_TEST_ROOT)};
	Store.Save(Slot, FCheckpointCodec::Encode(Saved));
	FGameSaveRequest Saves(Store);
	bool Validated = false;
	TScene* Current = nullptr;
	TResume Resume(Saves, Scenes, Assets, Catalog, [&Validated](const auto& Input)
	               {
		               // ゲーム意味の検査は明示codecの責任。別の新Worldをここで作らない。
		               REQUIRE(Input.GamePayload == "{\"collected\":[\"keyNorth\"],\"score\":7}");
		               Validated = true;
		               return Input;
	               },
	               [&Current](auto Prepared, auto Input)
	               {
		               auto Scene = MakeUnique<TScene>(Move(Prepared), Move(Input));
		               Current = Scene.Get();
		               return Scene;
	               });
	const auto Ticket = Resume.RequestLoad(Slot);
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Prepared);
	REQUIRE(Validated && Current == nullptr && Scenes.GetCurrent() == Old);
	Resume.Poll();
	REQUIRE(Validated && Current != nullptr && Scenes.GetCurrent() == Old);
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::TransitionRequested);
	REQUIRE(Current->GetResumeTicket().GetState() == ECheckpointSceneState::Pending);
	REQUIRE(Current->GetGamePayload() == Saved.GamePayload);
	REQUIRE(!Resume.Cancel());
	REQUIRE(Scenes.Commit());
	Resume.Poll();
	REQUIRE(Scenes.GetCurrent() == Current);
	REQUIRE(Current->GetResumeTicket().GetState() == ECheckpointSceneState::PendingPhysics);
	NavigatorTick(Scenes, 0);
	Resume.Poll();
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::TransitionRequested);
	REQUIRE(!Current->GetCheckpointCapture().GetLastCapture());
	NavigatorTick(Scenes, 1.0 / 60.0);
	Resume.Poll();
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Completed);
	REQUIRE(Current->GetCompletedPhysicsStepCount() == 1);
	REQUIRE(Current->GetCheckpointCapture().GetCapturedStep() == 1);
	REQUIRE(Current->GetCheckpointCapture().GetLastCapture()->Instances.Size() == 4);
	REQUIRE(Current->GetCheckpointCapture().GetLastCapture()->GamePayload == Saved.GamePayload);
	REQUIRE(Current->GetPrefab("cargo1").Get()->GetState() == EPrefabInstanceState::Ready);
	const auto SceneTicket = Current->GetResumeTicket();
	const auto OldPayload = Current->GetGamePayload();
	bool Rejected = false;
	try
	{
		Current->SetGamePayload("{\"score\":1e400}");
	}
	catch (const FException&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected && Current->GetGamePayload() == OldPayload);
	Scenes.Shutdown();
	REQUIRE(SceneTicket.GetState() == ECheckpointSceneState::Ended);
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Completed);
	REQUIRE(Resume.Retire());
}
template <typename TResume, typename TScene, typename TPrepared, typename TDocument, typename TCatalog>
void ResumeFailures(TCatalog Catalog, const char* Slot)
{
	Dxf::Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FAudioPlayer Audio(Backend);
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<APreviousScene>());
	REQUIRE(Scenes.Commit());
	const auto* Old = Scenes.GetCurrent();
	auto Saved = Document(Catalog);
	FSaveStore Store{FPath(DXF_CHECKPOINT_TEST_ROOT)};
	Store.Save(Slot, FCheckpointCodec::Encode(Saved));
	FGameSaveRequest Saves(Store);
	// CPUゲーム変換失敗では切替要求を出さず旧Sceneを維持する。
	TResume Invalid(Saves, Scenes, Assets, Catalog, [](const TDocument&) -> TDocument
	                {
		                throw FException("Injected game progress validation failure");
	                });
	const auto Failed = Invalid.RequestLoad(Slot);
	Invalid.Poll();
	REQUIRE(Failed.GetState() == EGameSaveRequestState::Failed);
	REQUIRE(Scenes.GetCurrent() == Old);
	REQUIRE(!Scenes.Commit().Value());
	REQUIRE(!Failed.GetReason().IsEmpty());
	TResume Candidate(Saves, Scenes, Assets, Catalog, [](const TDocument& D)
	                  {
		                  return D;
	                  },
	                  [](TPrepared P, TDocument D) -> TUniquePtr<TScene>
	                  {
		                  return MakeUnique<TRejectedCheckpointScene<TScene>>(Move(P), Move(D));
	                  });
	const auto Initialization = Candidate.RequestLoad(Slot);
	Candidate.Poll();
	REQUIRE(Initialization.GetState() == EGameSaveRequestState::TransitionRequested);
	REQUIRE(!Scenes.Commit());
	Candidate.Poll();
	REQUIRE(Initialization.GetState() == EGameSaveRequestState::Failed);
	REQUIRE(Scenes.GetCurrent() == Old);
	// 元の読込成功値と保存文書は失敗した候補に巻き込まれない。
	REQUIRE(Saves.GetLoaded().Document->Payload == Store.Load(Slot).Document->Payload);
	const auto Cancel = Candidate.RequestLoad(Slot);
	REQUIRE(Candidate.Cancel());
	Candidate.Poll();
	REQUIRE(Cancel.GetState() == EGameSaveRequestState::Canceled);
	REQUIRE(Scenes.GetCurrent() == Old);
	const auto Retired = Candidate.RequestLoad(Slot);
	Candidate.Poll();
	REQUIRE(Candidate.Retire());
	REQUIRE(Retired.GetState() == EGameSaveRequestState::Failed);
	Scenes.Shutdown();
}
template <typename TResume, typename TScene, typename TCatalog>
void ResumeTarget(TCatalog Catalog, const char* Slot)
{
	// 単独の直動自由度で駆動を確認し、同じBodyへの別拘束で結果を隠さない。
	auto Saved = Document(Catalog);
	auto Instance = Saved.Instances[3];
	Saved.Instances.Clear();
	Saved.Instances.PushBack(Instance);
	Saved.Joints.Clear();
	FCheckpointJointState State;
	State.Instance = Instance.Key;
	State.Export = "prismatic";
	State.Kind = EJointKind::Prismatic;
	State.LinearLimits = {true, -1, 2};
	FLinearJointTargetSettings Target;
	Target.TargetTranslation = 1.5;
	Target.MaxForce = 12;
	State.LinearTarget = Target;
	Saved.Joints.PushBack(State);
	auto& Prefab = Catalog.Prefabs[0].Definition;
	const auto Joint = Prefab.Joints[3];
	Prefab.Joints.Clear();
	Prefab.Joints.PushBack(Joint);
	Prefab.Exports.Clear();
	Prefab.Exports.PushBack({"support", EContentExportKind::RigidBody, 0});
	Prefab.Exports.PushBack({"body", EContentExportKind::RigidBody, 1});
	Prefab.Exports.PushBack({"prismatic", EContentExportKind::Prismatic, 0});
	Catalog.Scene.Prefabs.Clear();
	Catalog.Scene.Prefabs.PushBack(Prefab);
	Catalog.Scene.Placements.Clear();
	Catalog.Scene.Placements.PushBack(Instance.Placement);
	Catalog.Scene.Instances.Clear();
	Catalog.Scene.Instances.PushBack(Instance.Key);
	Catalog.Scene.Connections.Clear();
	Catalog.InitialPrefabKeys.Clear();
	Catalog.InitialPrefabKeys.PushBack(Instance.Prefab);
	Dxf::Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FAudioPlayer Audio(Backend);
	FSceneNavigator Scenes(Assets, Audio);
	FSaveStore Store{FPath(DXF_CHECKPOINT_TEST_ROOT)};
	Store.Save(Slot, FCheckpointCodec::Encode(Saved));
	FGameSaveRequest Saves(Store);
	TScene* Current = nullptr;
	TResume Resume(Saves, Scenes, Assets, Catalog, [](const auto& D)
	               {
		               return D;
	               },
	               [&Current](auto Prepared, auto D)
	               {
		               auto Scene = MakeUnique<TScene>(Move(Prepared), Move(D));
		               Current = Scene.Get();
		               return Scene;
	               });
	const auto Ticket = Resume.RequestLoad(Slot);
	Resume.Poll();
	REQUIRE(Scenes.Commit());
	NavigatorTick(Scenes, 1.0 / 60.0);
	Resume.Poll();
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Completed);
	const auto Handle = Current->GetPrefab(Instance.Key).Get()->GetPrismaticJoint("prismatic");
	const auto First = Handle.Get()->GetObservation();
	REQUIRE(First && Abs(First->State.Translation - 1) < 0.0001);
	REQUIRE(Handle.Get()->HasPendingRequest());
	const auto Id = Handle.Get()->GetJointId();
	NavigatorTick(Scenes, 1.0 / 60.0);
	const auto Second = Handle.Get()->GetObservation();
	REQUIRE(Second && Second->State.Drive.bEnabled);
	REQUIRE(Second->State.Drive.TargetSpeed > 0 && Second->State.Drive.MaxForce == 12);
	REQUIRE(Second->State.Translation > First->State.Translation);
	REQUIRE(Second->State.Translation < 1.1);
	REQUIRE(Id && Handle.Get()->GetJointId() && *Id == *Handle.Get()->GetJointId());
	Current->GetCheckpointCapture().RequestCapture();
	NavigatorTick(Scenes, 1.0 / 60.0);
	REQUIRE(Current->GetCheckpointCapture().GetLastCapture()->Joints[0].LinearTarget);
	REQUIRE(Current->GetCheckpointCapture().GetLastCapture()->Joints[0].LinearDrive.bEnabled);
}
template <typename TResume, typename TScene, typename TCatalog>
void ResumeCanceledAtConstruction(TCatalog Catalog, const char* Slot)
{
	Dxf::Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FAudioPlayer Audio(Backend);
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<APreviousScene>());
	REQUIRE(Scenes.Commit());
	const auto* Old = Scenes.GetCurrent();
	FSaveStore Store{FPath(DXF_CHECKPOINT_TEST_ROOT)};
	Store.Save(Slot, FCheckpointCodec::Encode(Document(Catalog)));
	FGameSaveRequest Saves(Store);
	TResume Resume(Saves, Scenes, Assets, Catalog, [](const auto& D)
	               {
		               return D;
	               },
	               [&Saves](auto Prepared, auto D)
	               {
		               // 明示構築フックが取消した場合も、最後の公開直前で再照合する。
		               auto Candidate = MakeUnique<TScene>(Move(Prepared), Move(D));
		               REQUIRE(Saves.Cancel());
		               return Candidate;
	               });
	const auto Ticket = Resume.RequestLoad(Slot);
	Resume.Poll();
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Canceled);
	REQUIRE(Scenes.GetCurrent() == Old);
	REQUIRE(!Scenes.Commit().Value());
}
} // namespace
TEST("Checkpoint resume 2D separates Prepared transition and successful required Ready")
{
	ResumeBoundary<FCheckpointResumeRequest2D, ACheckpointScene2D>(Catalog2D(), "resume_boundary_2d");
}
TEST("Checkpoint resume 3D separates Prepared transition and successful required Ready")
{
	ResumeBoundary<FCheckpointResumeRequest3D, ACheckpointScene3D>(Catalog3D(), "resume_boundary_3d");
}
TEST("Checkpoint resume 2D preserves old Scene through recipe and initialization failure")
{
	ResumeFailures<FCheckpointResumeRequest2D, ACheckpointScene2D, FPreparedScene2D, FCheckpointDocument2D>(Catalog2D(), "resume_failure_2d");
}
TEST("Checkpoint resume 3D preserves old Scene through recipe and initialization failure")
{
	ResumeFailures<FCheckpointResumeRequest3D, ACheckpointScene3D, FPreparedScene3D, FCheckpointDocument3D>(Catalog3D(), "resume_failure_3d");
}
TEST("Checkpoint resume 2D uses finite target Motor without teleport or extra Step")
{
	ResumeTarget<FCheckpointResumeRequest2D, ACheckpointScene2D>(Catalog2D(), "resume_target_2d");
}
TEST("Checkpoint resume 3D uses finite target Motor without teleport or extra Step")
{
	ResumeTarget<FCheckpointResumeRequest3D, ACheckpointScene3D>(Catalog3D(), "resume_target_3d");
}
TEST("Checkpoint resume rechecks cancellation at construction before Navigator publication")
{
	ResumeCanceledAtConstruction<FCheckpointResumeRequest2D, ACheckpointScene2D>(Catalog2D(), "resume_canceled_2d");
	ResumeCanceledAtConstruction<FCheckpointResumeRequest3D, ACheckpointScene3D>(Catalog3D(), "resume_canceled_3d");
}

namespace
{
// Workerで止めた旧要求を、試験終了時にも必ず解放する。
struct FResumeGate
{
	// 待機と解放の状態を同時に変更させない。
	FMutex Mutex;
	// 所有側の解放操作をWorkerへ通知する。
	FConditionVariable Changed;
	// Workerが純粋な変換へ到達した印。
	TAtomic<uint32> Started;
	// 旧Workerを終了まで進めてよいか。
	bool Released = false;
};
// Assertで中断しても、要求の終了待機より先に旧Workerを解放する。
struct FReleaseResumeGate
{
	// この試行の待機状態だけを共有する。
	TSharedPtr<FResumeGate> Gate;
	~FReleaseResumeGate()
	{
		FScopedLock Lock(Gate->Mutex);
		Gate->Released = true;
		Gate->Changed.NotifyAll();
	}
};
bool AwaitResume(const TAtomic<uint32>& Flag)
{
	const auto Start = MonotonicNanoseconds();
	while (!Flag.Load() && MonotonicNanoseconds() - Start < 5000000000ULL)
	{
		FThread::Yield();
	}
	return Flag.Load() != 0;
}
template <typename TResume, typename TScene, typename TCatalog>
void WorkerResume(TCatalog Catalog, const char* Slot)
{
	Dxf::Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FAudioPlayer Audio(Backend);
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<APreviousScene>());
	REQUIRE(Scenes.Commit());
	const auto* Old = Scenes.GetCurrent();
	FSaveStore Store{FPath(DXF_CHECKPOINT_TEST_ROOT)};
	Store.Save(Slot, FCheckpointCodec::Encode(Document(Catalog)));
	FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FGameSaveRequest Saves(Store, &Tasks);
	TAtomic<uint64> PreparedThread;
	uint64 CreatedThread = 0;
	const auto Owner = FThread::CurrentThreadId();
	TResume Resume(Saves, Scenes, Assets, Catalog, [&PreparedThread](const auto& Value)
	               {
		               PreparedThread.Store(FThread::CurrentThreadId());
		               return Value;
	               },
	               [&CreatedThread](auto Prepared, auto Value)
	               {
		               CreatedThread = FThread::CurrentThreadId();
		               return MakeUnique<TScene>(Move(Prepared), Move(Value));
	               });
	const auto Ticket = Resume.RequestLoad(Slot);
	Tasks.WaitForPrepares();
	REQUIRE(PreparedThread.Load() != 0 && PreparedThread.Load() != Owner);
	REQUIRE(CreatedThread == 0 && Scenes.GetCurrent() == Old);
	Tasks.PumpCommits();
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Prepared);
	REQUIRE(CreatedThread == 0 && Scenes.GetCurrent() == Old);
	Resume.Poll();
	REQUIRE(CreatedThread == Owner && Scenes.GetCurrent() == Old);
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::TransitionRequested);
	REQUIRE(Scenes.Commit());
	NavigatorTick(Scenes, 1.0 / 60.0);
	Resume.Poll();
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Completed);
	REQUIRE(Resume.Retire());
	Scenes.Shutdown();
}
template <typename TResume, typename TScene, typename TCatalog>
void RetiredWorkerResume(TCatalog Catalog, const char* Slot)
{
	Dxf::Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FAudioPlayer Audio(Backend);
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<APreviousScene>());
	REQUIRE(Scenes.Commit());
	const auto* Old = Scenes.GetCurrent();
	FSaveStore Store{FPath(DXF_CHECKPOINT_TEST_ROOT)};
	Store.Save(Slot, FCheckpointCodec::Encode(Document(Catalog)));
	FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FGameSaveRequest Saves(Store, &Tasks);
	const auto Gate = MakeShared<FResumeGate>();
	auto Resume = MakeUnique<TResume>(Saves, Scenes, Assets, Catalog, [Gate](const auto& Value)
	                                  {
		                                  FScopedLock Lock(Gate->Mutex);
		                                  Gate->Started.Store(1);
		                                  Gate->Changed.NotifyAll();
		                                  while (!Gate->Released)
		                                  {
			                                  Gate->Changed.Wait(Gate->Mutex);
		                                  }
		                                  return Value;
	                                  });
	// この後のAssert失敗でも、Savesの終了待機より先に通知する。
	FReleaseResumeGate Release{Gate};
	const auto A = Resume->RequestLoad(Slot);
	REQUIRE(AwaitResume(Gate->Started));
	REQUIRE(Resume->Retire());
	Resume.Reset();
	REQUIRE(A.GetState() == EGameSaveRequestState::Canceled);
	// 旧adapterを破棄しても、Workerは自分のCPU値だけを最後まで扱う。
	TResume Current(Saves, Scenes, Assets, Catalog, [](const auto& Value)
	                {
		                return Value;
	                });
	const auto B = Current.RequestLoad(Slot);
	{
		FScopedLock Lock(Gate->Mutex);
		Gate->Released = true;
		Gate->Changed.NotifyAll();
	}
	Tasks.WaitForPrepares();
	Tasks.PumpCommits();
	Current.Poll();
	REQUIRE(A.GetState() == EGameSaveRequestState::Canceled);
	REQUIRE(B.GetState() == EGameSaveRequestState::TransitionRequested);
	REQUIRE(Saves.GetAcceptedSequence() == B.GetSequence());
	REQUIRE(Scenes.GetCurrent() == Old && Scenes.Commit());
	NavigatorTick(Scenes, 1.0 / 60.0);
	Current.Poll();
	REQUIRE(B.GetState() == EGameSaveRequestState::Completed);
	REQUIRE(Current.Retire());
	Scenes.Shutdown();
}
} // namespace
TEST("Checkpoint CPU plans use Worker and Scene construction stays on owner in both dimensions")
{
	WorkerResume<FCheckpointResumeRequest2D, ACheckpointScene2D>(Catalog2D(), "resume_worker_2d");
	WorkerResume<FCheckpointResumeRequest3D, ACheckpointScene3D>(Catalog3D(), "resume_worker_3d");
}
TEST("Checkpoint retired adapter leaves late Worker values isolated from new request")
{
	RetiredWorkerResume<FCheckpointResumeRequest2D, ACheckpointScene2D>(Catalog2D(), "resume_retired_worker_2d");
	RetiredWorkerResume<FCheckpointResumeRequest3D, ACheckpointScene3D>(Catalog3D(), "resume_retired_worker_3d");
}
