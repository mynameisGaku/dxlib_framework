// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Support/CheckpointFixture.h"
#include "Dxf/CheckpointResumeRequest2D.h"
#include "Dxf/CheckpointResumeRequest3D.h"
#include "Dxf/InputStateTracker.h"
#include "Toolbox/JsonWriter.h"
#include "Toolbox/Thread.h"
#include "Toolbox/Log.h"
#include "../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
using namespace Dxf;
using namespace Toolbox;
using namespace TestCheckpointFixture;
namespace
{
// 新しい候補の失敗から保護する、初期化済みの旧Scene。
class AProtectedScene : public DScene
{
};
// 同じ保存から一地点ずつ失敗させ、最初の非注入試行まで記録する。
template <typename TResume, typename TScene, typename TCatalog>
void SweepResume(TCatalog Catalog, const char* Dimension)
{
	// 初回ログが作る全プロセス共有Mutexは、候補Sceneの保有数へ数えない。
	(void)GetLogLevel();
	Catalog.Scene.Instances.Resize(1);
	Catalog.Scene.Prefabs.Resize(1);
	Catalog.Scene.Placements.Resize(1);
	Catalog.InitialPrefabKeys.Resize(1);
	Catalog.Scene.Connections.Clear();
	// 再開準備の確保を小さい構成で全探索する。Joint登録・採取は別の故障群が扱う。
	auto& Prefab = Catalog.Prefabs[0].Definition;
	Prefab.Parts.Resize(1);
	Prefab.Parts[0].Body.Type = EBodyType::Dynamic;
	Prefab.Joints.Clear();
	Prefab.Exports.Clear();
	Prefab.Exports.PushBack({"body", EContentExportKind::RigidBody, 0});
	Catalog.Scene.Prefabs[0] = Prefab;
	const FPath Root = FPath(DXF_CHECKPOINT_TEST_ROOT) / FPath(FString("resume-fault-") + Dimension + FJsonWriter::Unsigned(MonotonicNanoseconds()));
	FSaveStore Store(Root);
	const auto Document = FCheckpointRestore::MakeInitial(Catalog, "{\"score\":7}");
	Store.Save("manual", FCheckpointCodec::Encode(Document));
	// 同じ入力の無故障処理で、探索を有限の確保数へ限定する。
	uint64 AllocationCount = 0;
	bool Finished = false;
	for (int64 Countdown = -1; Countdown <= static_cast<int64>(AllocationCount); ++Countdown)
	{
		const auto BeforeEnvironment = Toolbox::Testing::GetOutstandingTestAllocations();
		{
			Dxf::Testing::FFakeBackend Backend;
			FAssetService Assets{Backend, Backend, Backend};
			FAudioPlayer Audio(Backend);
			FSceneNavigator Scenes(Assets, Audio);
			REQUIRE(Scenes.RequestChange<AProtectedScene>());
			REQUIRE(Scenes.Commit());
			const auto* Old = Scenes.GetCurrent();
			FGameSaveRequest Saves(Store);
			TResume Resume(Saves, Scenes, Assets, Catalog, [](const auto& Value)
			               {
				               return Value;
			               });
			const auto Ticket = Resume.RequestLoad("manual");
			REQUIRE(Ticket.GetState() == EGameSaveRequestState::Prepared);
			const auto Before = Toolbox::Testing::GetTotalTestAllocations();
			Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
			Resume.Poll();
			const bool Actual = Toolbox::Testing::WasAllocationFailureInjected();
			Toolbox::Testing::SetAllocationFailureCountdown(-1);
			const auto Used = Toolbox::Testing::GetTotalTestAllocations() - Before;
			REQUIRE(Scenes.GetCurrent() == Old);
			REQUIRE(Store.Load("manual").Document->Generation == 1);
			if (Countdown < 0)
			{
				AllocationCount = Used;
				Out << FString("CHECKPOINT_RESUME_BASELINE ") + Dimension + " allocations=" + ToString(AllocationCount) << '\n';
				REQUIRE(AllocationCount > 0 && AllocationCount < 32768);
				REQUIRE(!Actual && Ticket.GetState() == EGameSaveRequestState::TransitionRequested);
			}
			else if (Actual)
			{
				REQUIRE(Ticket.GetState() == EGameSaveRequestState::Failed);
				REQUIRE(!Ticket.GetReason().IsEmpty());
				REQUIRE(!Scenes.Commit().Value());
				// 次の明示要求では同じ保存を正常に使える。
				const auto Retry = Resume.RequestLoad("manual");
				Resume.Poll();
				REQUIRE(Retry.GetState() == EGameSaveRequestState::TransitionRequested);
			}
			else
			{
				REQUIRE(Ticket.GetState() == EGameSaveRequestState::TransitionRequested);
				Finished = true;
			}
			if (Countdown >= 0)
			{
				Out << FString("CHECKPOINT_RESUME_FAULT ") + Dimension + " countdown=" + ToString(Countdown) + " injected=" + ToString(Actual ? 1 : 0) << '\n';
			}
			REQUIRE(Scenes.Commit());
			FInputStateTracker Input;
			FFrameTime Time;
			Time.DeltaSeconds = 1.0 / 60.0;
			Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
			REQUIRE(Scenes.CommitObjects());
			REQUIRE(Scenes.Tick(Time, Input.GetSnapshot()));
			Resume.Poll();
			const auto* Current = Scenes.GetCurrent()->TryCast<TScene>();
			REQUIRE(Current != nullptr && Current->GetCompletedPhysicsStepCount() == 1);
			REQUIRE(Current->GetResumeTicket().GetState() == ECheckpointSceneState::Ready);
			REQUIRE(Resume.Retire());
			Scenes.Shutdown();
		}
		// 確保した候補・診断・資源参照を終了後に残さない。
		Out << FString("CHECKPOINT_RESUME_OWNERS ") + Dimension + " countdown=" + ToString(Countdown) + " before=" + ToString(BeforeEnvironment) + " after=" + ToString(Toolbox::Testing::GetOutstandingTestAllocations()) << '\n';
		REQUIRE(Toolbox::Testing::GetOutstandingTestAllocations() == BeforeEnvironment);
		if (Finished)
		{
			break;
		}
	}
	REQUIRE(Finished);
}
} // namespace
#if DXF_CHECKPOINT_RESUME_FAULT_DIMENSION == 2
TEST("Checkpoint resume faults 2D preserve old Scene and retry through first non injected attempt")
{
	SweepResume<FCheckpointResumeRequest2D, ACheckpointScene2D>(Catalog2D(), "2D");
}
#else
TEST("Checkpoint resume faults 3D preserve old Scene and retry through first non injected attempt")
{
	SweepResume<FCheckpointResumeRequest3D, ACheckpointScene3D>(Catalog3D(), "3D");
}
#endif
