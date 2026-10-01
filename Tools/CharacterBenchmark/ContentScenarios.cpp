// SPDX-License-Identifier: NOASSERTION
#include "ContentScenarios.h"
#include "BenchmarkSupport.h"
#include "Dxf/SceneContentSource.h"
#include "Dxf/SceneContentValidation.h"
#include "Dxf/ContentScene2D.h"
#include "Dxf/ContentScene3D.h"
#include "Dxf/InputStateTracker.h"
#include "../../Tests/Support/FakeBackend.h"
#include <stdio.h>
namespace Dxf::Benchmark
{
namespace
{
// 行を観察できるよう全工程を同じ単位へ揃える。World単独は追加Stepを測る比較系列。
constexpr Toolbox::size_t Phases = 9;
const char* PhaseNames[Phases] = {
    "read-parse-expand",
    "pure-validation",
    "owner-prepare",
    "spawn-accept",
    "initialize",
    "first-fixed-ready",
    "fixed-total",
    "content-state-only",
    "world-only",
};
template <typename TScene, typename TInstance>
void Condition(Toolbox::uint32 Count, bool Joints, Toolbox::int32 Steps, bool SharedDefinition = true, bool Resources = false, bool SharedResources = true)
{
	Toolbox::f64 Times[Phases][Repetitions] = {};
	Toolbox::f64 Allocations[Phases][Repetitions] = {};
	Toolbox::f64 ShutdownTime[Repetitions] = {};
	Toolbox::f64 ShutdownAlloc[Repetitions] = {};
	Toolbox::f64 OwnerLoads[Repetitions] = {};
	for (Toolbox::int32 Repeat = 0; Repeat < Repetitions; ++Repeat)
	{
		Testing::FFakeBackend Backend;
		FAssetService Assets(Backend, Backend, Backend);
		if (!Assets.SetProjectRoot(Toolbox::FPath(DXF_CONTENT_BENCH_ROOT)))
		{
			throw Toolbox::FException("Benchmark ProjectRoot setup failed");
		}
		FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_BENCH_ROOT)};
		auto Definition = [&]()
		{
			if constexpr (Toolbox::IsSame<TScene, DPhysicsScene2D>)
			{
				return FPrefabDefinition2D{};
			}
			else
			{
				return FPrefabDefinition3D{};
			}
		}();
		const auto Measure = [&](Toolbox::size_t Phase, auto&& Work, Toolbox::int32 Repeats = 1)
		{
			const auto Alloc = TotalAllocations();
			const auto Start = NowNanoseconds();
			for (Toolbox::int32 I = 0; I < Repeats; ++I)
			{
				Work();
			}
			Times[Phase][Repeat] = static_cast<Toolbox::f64>(NowNanoseconds() - Start) / 1000 / Repeats;
			Allocations[Phase][Repeat] = static_cast<Toolbox::f64>(TotalAllocations() - Alloc) / Repeats;
		};
		Measure(0,
		        [&]()
		        {
			        if constexpr (Toolbox::IsSame<TScene, DPhysicsScene2D>)
			        {
				        Definition = Source.LoadPrefab2D("Tools/PackageConsumer/Data/prefab2d.dxfprefab.json");
			        }
			        else
			        {
				        Definition = Source.LoadPrefab3D("Tools/PackageConsumer/Data/prefab3d.dxfprefab.json");
			        }
		        });
		if (!Joints)
		{
			Definition.Joints.Clear();
			Definition.Exports.Clear();
		}
		Measure(1,
		        [&]()
		        {
			        ValidatePrefabDefinition(Definition);
		        });
		auto Prepared = [&]()
		{
			if constexpr (Toolbox::IsSame<TScene, DPhysicsScene2D>)
			{
				return FPreparedPrefab2D{};
			}
			else
			{
				return FPreparedPrefab3D{};
			}
		}();
		Toolbox::TVector<decltype(Prepared)> Variants;
		Measure(2, [&]()
		        {
			        const auto Definitions = SharedDefinition ? 1u : Count;
			        Variants.Reserve(Definitions);
			        for (Toolbox::uint32 I = 0; I < Definitions; ++I)
			        {
				        auto Value = Definition;
				        if (!SharedDefinition)
				        {
					        Value.Parts[1].DisplayName = Toolbox::FString("Variant-") + Toolbox::ToString(I);
				        }
				        if (Resources)
				        {
					        FContentAssetDefinition Image;
					        Image.Id = "image";
					        Image.Path = SharedResources ? Toolbox::FString("Assets/image.png") : Toolbox::FString("Assets/image-") + Toolbox::ToString(I) + ".png";
					        Value.Assets.PushBack(Toolbox::Move(Image));
				        }
				        Variants.PushBack(PreparePrefab(Toolbox::Move(Value), Assets));
			        }
			        Prepared = Variants[0];
			        OwnerLoads[Repeat] = Backend.GetTrace().TextureLoads;
		        });
		TScene Scene;
		Toolbox::TVector<TObjectHandle<TInstance>> Instances;
		Instances.Reserve(Count);
		Measure(3,
		        [&]()
		        {
			        for (Toolbox::uint32 I = 0; I < Count; ++I)
			        {
				        auto Placement = [&]()
				        {
					        if constexpr (Toolbox::IsSame<TScene, DPhysicsScene2D>)
					        {
						        return FPrefabSpawnOptions2D{};
					        }
					        else
					        {
						        return FPrefabSpawnOptions3D{};
					        }
				        }();
				        Placement.Position.X = static_cast<Toolbox::f32>(I) * 4;
				        const auto Spawned = Scene.template Spawn<TInstance>(SharedDefinition ? Prepared : Variants[I], Placement);
				        if (!Spawned)
				        {
					        throw Toolbox::FException(Spawned.Error().Message);
				        }
				        Instances.PushBack(Spawned.Value());
			        }
		        });
		Measure(4,
		        [&]()
		        {
			        const auto Initialized = Scene.Initialize_Internal({Assets});
			        if (!Initialized)
			        {
				        throw Toolbox::FException(Initialized.Error().Message);
			        }
		        });
		FInputStateTracker Input;
		FFrameTime Time;
		Time.DeltaSeconds = StepSeconds;
		Time.UnscaledDeltaSeconds = StepSeconds;
		const auto Tick = [&]()
		{
			const auto Ticked = Scene.Tick_Internal({Input.GetSnapshot(), Time});
			if (!Ticked)
			{
				throw Toolbox::FException(Ticked.Error().Message);
			}
		};
		Measure(5, Tick);
		for (const auto& Instance : Instances)
		{
			if (Instance.Get()->GetState() != EPrefabInstanceState::Ready)
			{
				throw Toolbox::FException("Benchmark Prefab is not Ready");
			}
		}
		for (Toolbox::int32 I = 0; I < 30; ++I)
		{
			Tick();
		}
		Measure(6, Tick, Steps);
		Measure(
		    7,
		    [&]()
		    {
			    for (const auto& Instance : Instances)
			    {
				    if (Instance.Get()->GetState() != EPrefabInstanceState::Ready)
				    {
					    throw Toolbox::FException("Benchmark Prefab lost endpoint");
				    }
			    }
		    },
		    Steps);
		Measure(
		    8,
		    [&]()
		    {
			    Scene.GetPhysicsWorld().Step(StepSeconds);
		    },
		    Steps);
		const auto Alloc = TotalAllocations();
		const auto Start = NowNanoseconds();
		Scene.Shutdown_Internal();
		ShutdownTime[Repeat] = static_cast<Toolbox::f64>(NowNanoseconds() - Start) / 1000;
		ShutdownAlloc[Repeat] = static_cast<Toolbox::f64>(TotalAllocations() - Alloc);
	}
	const auto Print = [&](const char* Phase, const Toolbox::f64(&TimesForPhase)[Repetitions], const Toolbox::f64(&AllocForPhase)[Repetitions])
	{
		const auto T = Summarize(TimesForPhase);
		const auto A = Summarize(AllocForPhase);
		const auto Loads = Summarize(OwnerLoads);
		printf("content,%d,%u,%u,%u,%s,%s,%s,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.0f\n", Toolbox::IsSame<TScene, DPhysicsScene2D> ? 2 : 3, Count, 2 * Count, Joints ? 4 * Count : 0, SharedDefinition ? "shared" : "distinct", Resources ? "fake-owner-texture" : "none", SharedResources ? "shared" : "distinct", Phase, T.Median, T.Min, T.Max, A.Median, A.Min, A.Max, Loads.Median);
	};
	for (Toolbox::size_t I = 0; I < Phases; ++I)
	{
		Print(PhaseNames[I], Times[I], Allocations[I]);
	}
	Print("shutdown", ShutdownTime, ShutdownAlloc);
}
} // namespace
void RunContentSeries(Toolbox::int32 Steps)
{
	printf("series,dimension,instances,bodies,joints,definitions,resource_boundary,resources,phase,median_us,min_us,max_us,median_alloc,min_alloc,max_alloc,owner_loads\n");
	for (const Toolbox::uint32 Count : {1u, 32u, 256u})
	{
		for (const bool Joints : {false, true})
		{
			Condition<DPhysicsScene2D, DPrefabInstance2D>(Count, Joints, Steps);
			Condition<DPhysicsScene3D, DPrefabInstance3D>(Count, Joints, Steps);
			Condition<DPhysicsScene2D, DPrefabInstance2D>(Count, Joints, Steps, true, true, true);
			Condition<DPhysicsScene3D, DPrefabInstance3D>(Count, Joints, Steps, true, true, true);
			Condition<DPhysicsScene2D, DPrefabInstance2D>(Count, Joints, Steps, false, true, true);
			Condition<DPhysicsScene3D, DPrefabInstance3D>(Count, Joints, Steps, false, true, true);
			Condition<DPhysicsScene2D, DPrefabInstance2D>(Count, Joints, Steps, false, true, false);
			Condition<DPhysicsScene3D, DPrefabInstance3D>(Count, Joints, Steps, false, true, false);
		}
	}
}
} // namespace Dxf::Benchmark
