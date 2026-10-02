// SPDX-License-Identifier: NOASSERTION
#include "SaveScenarios.h"
#include "BenchmarkSupport.h"
#include "Dxf/CheckpointRestore.h"
#include "Dxf/CheckpointCodec.h"
#include "Dxf/SaveCodec.h"
#include "Dxf/CheckpointResumeRequest2D.h"
#include "Dxf/CheckpointResumeRequest3D.h"
#include "Dxf/InputProfileCodec.h"
#include "Dxf/InputStateTracker.h"
#include "Toolbox/JsonWriter.h"
#include "Dxf/SceneContentSource.h"
#include "Dxf/AudioPlayer.h"
#include "../../Source/SaveData/Private/Dxf/SaveStorage.h"
#include "../../Tests/Support/FakeBackend.h"
#include <stdio.h>
namespace Dxf::Benchmark
{
namespace
{
using namespace Toolbox;
// 測定境界を製品の私有状態へのアクセスなしで区別する。
constexpr size_t Phases = 25;
const char* Names[Phases] = {"trusted-read-parse", "initial-value-plan", "owner-assets-prepare", "scene-initialize", "first-fixed-ready", "warm-fixed-total", "steady-fixed-total", "safe-capture-only", "checkpoint-encode", "face-encode-checksum", "stage-write-flush-readback", "owner-publish-confirm", "load-verify-decode", "restore-plan", "resume-owner-assets-initialize", "resume-first-fixed-ready", "profile-apply", "profile-update-idle", "save-poll-idle", "resume-poll-idle", "async-submit-worker-wait-deliver", "async-owner-publish", "game-payload-migrate-validate", "resume-worker-wait-deliver", "resume-owner-submit"};
// 本来の成功境界の中だけで採取時間を測る。求解・製品ソースにprobeを追加しない。
template <typename TBase>
class AMeasuredScene : public TBase
{
public:
	using TBase::TBase;
	void RequestMeasurement()
	{
		if (!this->GetCheckpointCapture().RequestCapture())
		{
			throw FException("Save benchmark capture request failed");
		}
		m_bMeasure = true;
	}
	f64 GetCaptureMicroseconds() const noexcept
	{
		return m_Time;
	}
	f64 GetCaptureAllocations() const noexcept
	{
		return m_Alloc;
	}

protected:
	void OnCheckpointStepCompleted(const FFixedTickContext& Context) override
	{
		(void)Context;
		if (!m_bMeasure)
		{
			return;
		}
		const auto Alloc = TotalAllocations();
		const auto Start = NowNanoseconds();
		const auto Captured = this->GetCheckpointCapture().CaptureIfRequested(*this, this->GetGamePayload());
		m_Time = static_cast<f64>(NowNanoseconds() - Start) / 1000;
		m_Alloc = static_cast<f64>(TotalAllocations() - Alloc);
		m_bMeasure = false;
		if (!Captured)
		{
			throw FException("Save benchmark capture was not at a ready boundary");
		}
	}

private:
	// 次の成功固定更新でだけ計測する。
	bool m_bMeasure = false;
	// 採取だけの所要時間。
	f64 m_Time = 0;
	// 採取だけの追加確保。
	f64 m_Alloc = 0;
};
// 個体数以外を揃え、同期Worldと実disk flushの費用を五回別々に記録する。
template <typename TCatalog, typename TRecipe, typename TScene, typename TResume>
void Condition(uint32 Count, int32 Steps, const FPath& RunRoot)
{
	f64 Times[Phases][Repetitions] = {};
	f64 Allocations[Phases][Repetitions] = {};
	f64 DocumentBytes[Repetitions] = {};
	f64 EntryCounts[Repetitions] = {};
	f64 ReadySteps[Repetitions] = {};
	f64 ShutdownTimes[Repetitions] = {};
	f64 ShutdownAllocs[Repetitions] = {};
	const uint32 Dimension = IsSame<TScene, ACheckpointScene2D> ? 2 : 3;
	for (int32 Repeat = 0; Repeat < Repetitions; ++Repeat)
	{
		const auto Measure = [&](size_t Phase, auto&& Work, int32 Repeats = 1)
		{
			const auto Alloc = TotalAllocations();
			const auto Start = NowNanoseconds();
			for (int32 I = 0; I < Repeats; ++I)
			{
				Work();
			}
			Times[Phase][Repeat] = static_cast<f64>(NowNanoseconds() - Start) / 1000 / Repeats;
			Allocations[Phase][Repeat] = static_cast<f64>(TotalAllocations() - Alloc) / Repeats;
		};
		Testing::FFakeBackend Backend;
		FAssetService Assets(Backend, Backend, Backend);
		FAudioPlayer Audio(Backend);
		FSceneNavigator Scenes(Assets, Audio);
		FSceneContentSource Source{FPath(DXF_CONTENT_BENCH_ROOT) / "Tools/PackageConsumer"};
		TCatalog Catalog;
		Measure(0, [&]()
		        {
			        if constexpr (IsSame<TScene, ACheckpointScene2D>)
			        {
				        Catalog.Scene = Source.LoadScene2D("Data/scene2d.dxfscene.json");
			        }
			        else
			        {
				        Catalog.Scene = Source.LoadScene3D("Data/scene3d.dxfscene.json");
			        }
		        });
		TRecipe Recipe;
		Recipe.Key = "mechanism";
		Recipe.Definition = Catalog.Scene.Prefabs[0];
		Catalog.Prefabs.PushBack(Recipe);
		Catalog.Identity.GameId = "save_benchmark";
		Catalog.Identity.ContentRevision = "benchmark-v1";
		Catalog.Identity.SceneKey = "course";
		Catalog.Identity.CheckpointKey = "start";
		Catalog.Identity.Dimension = Dimension;
		const auto DefaultPlacement = Catalog.Scene.Placements[0];
		Catalog.Scene.Instances.Clear();
		Catalog.Scene.Prefabs.Clear();
		Catalog.Scene.Placements.Clear();
		Catalog.Scene.Connections.Clear();
		Catalog.Scene.Gravity = {};
		for (uint32 I = 0; I < Count; ++I)
		{
			Catalog.Scene.Instances.PushBack(FString("instance") + ToString(I));
			Catalog.Scene.Prefabs.PushBack(Recipe.Definition);
			auto Placement = DefaultPlacement;
			Placement.Position.X = static_cast<f32>(I) * 4;
			Catalog.Scene.Placements.PushBack(Placement);
			Catalog.InitialPrefabKeys.PushBack("mechanism");
		}
		auto Initial = FCheckpointRestore::MakeInitial(Catalog, "{\"score\":42}");
		auto Plan = FCheckpointRestore::Build(Initial, Catalog);
		Measure(1, [&]()
		        {
			        Initial = FCheckpointRestore::MakeInitial(Catalog, "{\"score\":42}");
			        Plan = FCheckpointRestore::Build(Initial, Catalog);
		        });
		auto Prepared = PrepareScene(Plan.Scene, Assets);
		Measure(2, [&]()
		        {
			        Prepared = PrepareScene(Plan.Scene, Assets);
		        });
		auto Owned = MakeUnique<AMeasuredScene<TScene>>(Move(Prepared), Initial);
		auto* Current = Owned.Get();
		Measure(3, [&]()
		        {
			        if (!Scenes.RequestChange(Move(Owned)) || !Scenes.Commit())
			        {
				        throw FException("Save benchmark initialization failed");
			        }
		        });
		FInputStateTracker Input;
		FFrameTime Time;
		Time.DeltaSeconds = StepSeconds;
		Time.UnscaledDeltaSeconds = StepSeconds;
		const auto Tick = [&]()
		{
			if (!Scenes.CommitObjects() || !Scenes.Tick(Time, Input.GetSnapshot()))
			{
				throw FException("Save benchmark normal update failed");
			}
		};
		Measure(4, Tick);
		if (Current->GetResumeTicket().GetState() != ECheckpointSceneState::Ready)
		{
			throw FException("Save benchmark first normal Step was not Ready");
		}
		ReadySteps[Repeat] = static_cast<f64>(Current->GetCompletedPhysicsStepCount());
		Measure(5, Tick, 30);
		Measure(6, Tick, Steps);
		Current->RequestMeasurement();
		Tick();
		Times[7][Repeat] = Current->GetCaptureMicroseconds();
		Allocations[7][Repeat] = Current->GetCaptureAllocations();
		FSaveDocument Document;
		Measure(8, [&]()
		        {
			        Document = FCheckpointCodec::Encode(*Current->GetCheckpointCapture().GetLastCapture());
		        });
		DocumentBytes[Repeat] = static_cast<f64>(Document.Payload.Size());
		EntryCounts[Repeat] = static_cast<f64>(Current->GetCheckpointCapture().GetLastCapture()->Joints.Size());
		Measure(9, [&]()
		        {
			        (void)EncodeSaveFace(Document, {});
		        });
		const auto Root = RunRoot / FPath(FString("dimension") + ToString(Dimension) + "-count" + ToString(Count) + "-repeat" + ToString(Repeat));
		FSaveStore Store(Root);
		TOptional<FSaveTransaction> Transaction;
		Measure(10, [&]()
		        {
			        Transaction = Store.Stage("checkpoint", Document);
		        });
		Measure(11, [&]()
		        {
			        if (Transaction->Publish() != 1)
			        {
				        throw FException("Save benchmark new slot generation differs");
			        }
		        });
		auto Restored = Initial;
		Measure(12, [&]()
		        {
			        const auto Loaded = Store.Load("checkpoint");
			        if (!Loaded.Document)
			        {
				        throw FException("Save benchmark load failed");
			        }
			        if constexpr (IsSame<TScene, ACheckpointScene2D>)
			        {
				        Restored = FCheckpointCodec::Decode2D(*Loaded.Document);
			        }
			        else
			        {
				        Restored = FCheckpointCodec::Decode3D(*Loaded.Document);
			        }
		        });
		Measure(13, [&]()
		        {
			        Plan = FCheckpointRestore::Build(Restored, Catalog);
		        });
		FJobSystem Jobs(2);
		FTaskDispatcher Tasks(Jobs);
		FGameSaveRequest Requests(Store, &Tasks);
		TResume Resume(Requests, Scenes, Assets, Catalog, [](const auto& Value)
		               {
			               return Value;
		               });
		FGameSaveTicket Ticket;
		Measure(24, [&]()
		        {
			        Ticket = Resume.RequestLoad("checkpoint");
		        });
		Measure(23, [&]()
		        {
			        Tasks.WaitForPrepares();
			        Tasks.PumpCommits();
			        if (Ticket.GetState() != EGameSaveRequestState::Prepared)
			        {
				        throw FException("Save benchmark CPU resume plan failed");
			        }
		        });
		Measure(14, [&]()
		        {
			        Resume.Poll();
			        if (Ticket.GetState() != EGameSaveRequestState::TransitionRequested || !Scenes.Commit())
			        {
				        throw FException("Save benchmark resume preparation failed");
			        }
		        });
		Measure(15, [&]()
		        {
			        Tick();
			        Resume.Poll();
			        if (Ticket.GetState() != EGameSaveRequestState::Completed)
			        {
				        throw FException("Save benchmark resume was not completed after Ready");
			        }
		        });
		FInputMap Map;
		FInputProfile Profile;
		FInputBindingDescription Binding;
		Binding.Key = EKey::K;
		Profile.Actions.PushBack({"Jump", {Binding}});
		FInputProfilePolicy Policy;
		Policy.AllowedActions.PushBack("Jump");
		Measure(16, [&]()
		        {
			        FInputProfileCodec::Apply(Profile, Policy, Map, Input.GetSnapshot());
		        });
		Measure(17, [&]()
		        {
			        Map.Update(Input.GetSnapshot());
		        },
		        Steps);
		Measure(18, [&]()
		        {
			        Requests.Poll();
		        },
		        Steps);
		Measure(19, [&]()
		        {
			        Resume.Poll();
		        },
		        Steps);
		FGameSaveRequest Async(Store, &Tasks);
		FGameSaveTicket AsyncTicket;
		Measure(20, [&]()
		        {
			        AsyncTicket = Async.RequestSave("async", Document);
			        Tasks.WaitForPrepares();
			        Tasks.PumpCommits();
			        if (AsyncTicket.GetState() != EGameSaveRequestState::Prepared)
			        {
				        throw FException("Save benchmark Worker preparation failed");
			        }
		        });
		Measure(21, [&]()
		        {
			        Async.Poll();
			        if (AsyncTicket.GetState() != EGameSaveRequestState::Committed)
			        {
				        throw FException("Save benchmark owner publication failed");
			        }
		        });
		// 独立した小さいゲーム値の明示移行。Content個体の復元計画とは分ける。
		FSaveDocument Legacy = Document;
		Legacy.Payload = "{\"coins\":42}";
		Legacy.GameVersion = 1;
		FSaveDocument Expected = Legacy;
		Expected.GameVersion = 2;
		FSavePayloadMigration Migration;
		Migration.FromVersion = 1;
		Migration.ToVersion = 2;
		Migration.Convert = [](FStringView Bytes)
		{
			const auto Json = FJsonDocument::Parse(Bytes);
			const auto Index = Json.Find(0, "coins");
			if (Json.Size() != 2 || Index < 0 || Json.Get(Index).Kind != EJsonKind::Number)
			{
				throw FException("Invalid benchmark legacy payload");
			}
			return FString("{\"score\":") + FJsonWriter::Unsigned(FJsonWriter::ParseUnsigned(Json.Get(Index).Text)) + "}";
		};
		Measure(22, [&]()
		        {
			        const auto Migrated = FSaveCodec::Migrate(Legacy, Expected, Migration);
			        if (Migrated.GameVersion != 2 || Migrated.Payload != "{\"score\":42}" || Legacy.GameVersion != 1)
			        {
				        throw FException("Save benchmark migration differs");
			        }
		        });
		if (!Async.Retire() || !Resume.Retire())
		{
			throw FException("Save benchmark retirement failed");
		}
		const auto ShutdownAlloc = TotalAllocations();
		const auto ShutdownStart = NowNanoseconds();
		Scenes.Shutdown();
		ShutdownTimes[Repeat] = static_cast<f64>(NowNanoseconds() - ShutdownStart) / 1000;
		ShutdownAllocs[Repeat] = static_cast<f64>(TotalAllocations() - ShutdownAlloc);
	}
	const auto Print = [&](const char* Phase, const f64(&T)[Repetitions], const f64(&A)[Repetitions])
	{
		const auto Time = Summarize(T);
		const auto Alloc = Summarize(A);
		printf("save,%u,%u,%u,%u,%.0f,%s,%.0f,%.0f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", Dimension, Count, Count * 2, Count * 4, Summarize(EntryCounts).Median, Phase, Summarize(DocumentBytes).Median, Summarize(ReadySteps).Median, Time.Median, Time.Min, Time.Max, Alloc.Median, Alloc.Min, Alloc.Max);
	};
	for (size_t I = 0; I < Phases; ++I)
	{
		Print(Names[I], Times[I], Allocations[I]);
	}
	Print("scene-shutdown", ShutdownTimes, ShutdownAllocs);
}
} // namespace
void RunSaveSeries(Toolbox::int32 Steps, Toolbox::uint32 Dimension, Toolbox::uint32 Instances)
{
	const auto Root = Toolbox::TemporaryDirectory() / Toolbox::FPath(Toolbox::FString("dxf-save-benchmark-") + Toolbox::ToString(Toolbox::MonotonicNanoseconds()));
	printf("save-root,%s\n", Root.ToUtf8().CStr());
	printf("series,dimension,instances,bodies,joints,mechanism_entries,phase,document_bytes,ready_steps,median_us,min_us,max_us,median_alloc,min_alloc,max_alloc\n");
	for (const Toolbox::uint32 Count : {16u, 64u, 256u})
	{
		if (Instances != 0 && Instances != Count)
		{
			continue;
		}
		if (Dimension == 0 || Dimension == 2)
		{
			Condition<FCheckpointCatalog2D, FCheckpointPrefabRecipe2D, ACheckpointScene2D, FCheckpointResumeRequest2D>(Count, Steps, Root);
		}
		if (Dimension == 0 || Dimension == 3)
		{
			Condition<FCheckpointCatalog3D, FCheckpointPrefabRecipe3D, ACheckpointScene3D, FCheckpointResumeRequest3D>(Count, Steps, Root);
		}
	}
}
} // namespace Dxf::Benchmark
