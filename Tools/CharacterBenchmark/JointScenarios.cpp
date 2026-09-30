// SPDX-License-Identifier: NOASSERTION
#include "JointScenarios.h"
#include "BenchmarkSupport.h"
#include "Dxf/DistanceJointComponent2D.h"
#include "Dxf/DistanceJointComponent3D.h"
#include "Dxf/AssetService.h"
#include "Dxf/InputStateTracker.h"
#include "../../Tests/Support/FakeBackend.h"
#include <stdio.h>
namespace Dxf::Benchmark
{
namespace
{
// Component自身の固定更新だけを測定入口へ公開する検証側の型。
template <typename TJoint>
class TBenchJoint final : public TJoint
{
public:
	template <typename TDescription>
	explicit TBenchJoint(TDescription Description) : TJoint(Description)
	{
	}
	void Dispatch(const FFixedTickContext& Fixed)
	{
		this->OnFixedTick(Fixed);
	}
};
// 条件の名前と、最大Islandの大きさを決める既知の接続配置。
enum class ECase
{
	// 拘束もColliderもない対照。
	None,
	// 一体ずつ別の静止支点へ接続。
	IndependentStatic,
	// 独立したDynamic二体の接続。
	DynamicPairs,
	// 同じ静止支点を共有。
	SharedStatic,
	// 同じ動く支点を共有。
	SharedKinematic,
	// ContactとJointがあるDynamic対。
	ContactMix,
	// 一つの島になる連結。
	Chain,
	// Component接続を維持。
	Component,
	// Component接続を毎固定更新で切替。
	Toggle
};
const char* Name(ECase Case)
{
	switch (Case)
	{
	case ECase::None:
		return "no-joint";
	case ECase::IndependentStatic:
		return "independent-static";
	case ECase::DynamicPairs:
		return "dynamic-pairs";
	case ECase::SharedStatic:
		return "shared-static";
	case ECase::SharedKinematic:
		return "shared-kinematic";
	case ECase::ContactMix:
		return "contact-joint-mix";
	case ECase::Chain:
		return "long-chain";
	case ECase::Component:
		return "component-steady";
	case ECase::Toggle:
		return "component-toggle";
	}
	throw Toolbox::FException("Unknown joint benchmark case");
}
// 2Dの型と、既存ContextへWorldを渡す箇所。
struct FJointBench2D
{
	using FWorld = FPhysicsWorld2D;
	using FBody = FBodyDescription2D;
	using FBodyId = FBodyId2D;
	using FJoint = FDistanceJointDescription2D;
	using FComponentDescription = FDistanceJointComponentDescription2D;
	using FComponent = TBenchJoint<DDistanceJoint2DComponent>;
	using FReference = FPhysicsBodyReference2D;
	static constexpr const char* Dimension = "2D";
	static Toolbox::FVector2 At(Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {X, Y};
	}
	static void Bind(FFixedTickContext& Fixed, FWorld& World)
	{
		Fixed.Physics2D = &World;
	}
	static void Attach(FWorld& World, FBodyId Body)
	{
		// 登録へ渡す生成条件。
		FColliderDescription2D Description;
		Description.Shape = Toolbox::FCircle2D{{}, 0.2f};
		World.AttachCollider(Body, Description);
	}
	static void Floor(FWorld& World)
	{
		// 登録へ渡す生成条件。
		FBody Description;
		Description.Type = EBodyType::Static;
		Description.Position = At(0, -0.25f);
		// 比較に使う世代付きID。
		const auto Id = World.CreateBody(Description);
		FColliderDescription2D Collider;
		Collider.Shape = Toolbox::FOrientedBox2D{{}, {1100, 0.25f}, 0};
		World.AttachCollider(Id, Collider);
	}
};
// 3Dの型と、既存ContextへWorldを渡す箇所。
struct FJointBench3D
{
	using FWorld = FPhysicsWorld3D;
	using FBody = FBodyDescription3D;
	using FBodyId = FBodyId3D;
	using FJoint = FDistanceJointDescription3D;
	using FComponentDescription = FDistanceJointComponentDescription3D;
	using FComponent = TBenchJoint<DDistanceJoint3DComponent>;
	using FReference = FPhysicsBodyReference3D;
	static constexpr const char* Dimension = "3D";
	static Toolbox::FVector3 At(Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {X, Y, 0};
	}
	static void Bind(FFixedTickContext& Fixed, FWorld& World)
	{
		Fixed.Physics3D = &World;
	}
	static void Attach(FWorld& World, FBodyId Body)
	{
		// 登録へ渡す生成条件。
		FColliderDescription3D Description;
		Description.Shape = Toolbox::FSphere{{}, 0.2f};
		World.AttachCollider(Body, Description);
	}
	static void Floor(FWorld& World)
	{
		// 登録へ渡す生成条件。
		FBody Description;
		Description.Type = EBodyType::Static;
		Description.Position = At(0, -0.25f);
		// 比較に使う世代付きID。
		const auto Id = World.CreateBody(Description);
		FColliderDescription3D Collider;
		Collider.Shape = Toolbox::FOBB{{}, {1100, 0.25f, 4}};
		World.AttachCollider(Id, Collider);
	}
};
// 1条件を5回、同じ数の固定更新で測る。初回と慣らし後は別行。
template <typename T>
void Measure(ECase Case, Toolbox::int32 Count, Toolbox::uint32 Lanes, Toolbox::int32 Warmup, Toolbox::int32 Steps)
{
	// 区間ごとの慣らし後時間。
	Toolbox::f64 Times[4][Repetitions]{};
	// 区間ごとの慣らし後確保数。
	Toolbox::f64 Allocations[4][Repetitions]{};
	// 初回の区間時間。
	Toolbox::f64 Initial[4][Repetitions]{};
	// 初回の区間確保数。
	Toolbox::f64 InitialAllocations[4][Repetitions]{};
	// 最終Stepの実診断値。
	FPhysicsExecutionDiagnostics Diagnostics;
	// 休止中のDynamic Body数。
	Toolbox::uint64 Sleep = 0;
	// この配置で登録したBody数。
	Toolbox::size_t BodyCount = 0;
	// この配置で有効な接続本数。
	Toolbox::size_t JointCount = 0;
	for (Toolbox::int32 Repeat = 0; Repeat < Repetitions; ++Repeat)
	{
		Toolbox::FJobSystem Jobs(Lanes);
		// この試行が所有する実Physics World。
		typename T::FWorld World;
		World.SetGravity({});
		FPhysicsExecutionSettings Execution;
		Execution.JobSystem = Lanes == 1 ? nullptr : &Jobs;
		World.SetExecutionSettings(Execution);
		Testing::FFakeBackend Backend;
		FAssetService Assets(Backend, Backend, Backend);
		DGameObject Owner;
		Toolbox::TVector<TObjectHandle<typename T::FComponent>> Components;
		Toolbox::TVector<typename T::FBodyId> Dynamic;
		const bool bShared = Case == ECase::SharedStatic || Case == ECase::SharedKinematic;
		const bool bChain = Case == ECase::Chain;
		const bool bComponent = Case == ECase::Component || Case == ECase::Toggle;
		// 支点の生成条件。
		typename T::FBody Anchor;
		Anchor.Type = Case == ECase::SharedKinematic ? EBodyType::Kinematic : EBodyType::Static;
		Anchor.Position = T::At(0, 3);
		typename T::FBodyId Shared;
		if (bShared || bChain)
		{
			Shared = World.CreateBody(Anchor);
		}
		if (Case == ECase::SharedKinematic)
		{
			World.SetVelocity(Shared, T::At(0, 0.1f));
		}
		if (Case == ECase::ContactMix)
		{
			T::Floor(World);
		}
		for (Toolbox::int32 Index = 0; Index < Count; ++Index)
		{
			// 配置または投影に使う位置。
			const auto Position = T::At(static_cast<Toolbox::f32>(Index * 4), 2.2f);
			Anchor.Position = Position;
			Anchor.Type = Case == ECase::DynamicPairs || Case == ECase::ContactMix || Case == ECase::None ? EBodyType::Dynamic : EBodyType::Static;
			// 接続のA側。
			const auto A = bShared || bChain ? Shared : World.CreateBody(Anchor);
			if (Anchor.Type == EBodyType::Dynamic && !bShared && !bChain)
			{
				Dynamic.PushBack(A);
			}
			// 荷物の生成条件。
			typename T::FBody Weight;
			Weight.Position = bChain ? T::At(0, static_cast<Toolbox::f32>(1 - Index * 2)) : Position + T::At(0, -2);
			// 接続のB側。
			const auto B = World.CreateBody(Weight);
			Dynamic.PushBack(B);
			// 登録へ渡す生成条件。
			typename T::FJoint Description;
			Description.Length = Distance(World.GetPosition(A), World.GetPosition(B));
			if (Case == ECase::ContactMix)
			{
				T::Attach(World, A);
				T::Attach(World, B);
			}
			if (bComponent)
			{
				// 接続先と長さをまとめたComponentの設定。
				typename T::FComponentDescription Component;
				Component.BodyA = T::FReference::FromBodyId(A);
				Component.BodyB = T::FReference::FromBodyId(B);
				Component.Joint = Description;
				Components.PushBack(Owner.template AddComponent<typename T::FComponent>(Component).Value());
			}
			else if (Case != ECase::None)
			{
				World.CreateDistanceJoint(A, B, Description);
			}
			if (bChain)
			{
				Shared = B;
			}
		}
		if (!Owner.Initialize_Internal({Assets}))
		{
			throw Toolbox::FException("Joint benchmark owner initialize");
		}
		// 固定更新へ渡す入力の保存先。
		FInputStateTracker Input;
		// 未配達の入力を保持する列。
		Toolbox::TVector<FInputSnapshot> Pending;
		// 今回の固定更新の直前予約。
		FPrePhysicsStepQueue Pre;
		// 今回の固定更新の成功後予約。
		FPostPhysicsStepQueue Post;
		// 同じWorldと予約先を持つ固定更新の環境。
		FFixedTickContext Fixed{Input.GetSnapshot(), Pending, StepSeconds, 0, true, false, 0, nullptr, nullptr, nullptr, nullptr, &Pre, &Post};
		T::Bind(Fixed, World);
		for (Toolbox::int32 Step = -Warmup; Step < Steps; ++Step)
		{
			// 初回も測るが、慣らしの残りは集計しない。
			const bool bFirst = Step == -Warmup;
			if (Case == ECase::Toggle && Step >= 0)
			{
				for (const auto& Handle : Components)
				{
					if (Step % 2 == 0)
					{
						Handle.Get()->RequestDisconnect();
					}
					else
					{
						Handle.Get()->RequestConnect(Handle.Get()->GetDescription());
					}
				}
			}
			for (Toolbox::int32 Region = 0; Region < 4; ++Region)
			{
				// 区間開始時の累計確保数。
				const auto AllocationStart = TotalAllocations();
				// 区間計測の開始時刻。
				const auto Start = NowNanoseconds();
				if (Region == 0)
				{
					for (const auto& Handle : Components)
					{
						Handle.Get()->Dispatch(Fixed);
					}
				}
				if (Region == 1)
				{
					Pre.Run_Internal(Fixed);
				}
				if (Region == 2)
				{
					World.Step(StepSeconds);
				}
				if (Region == 3)
				{
					Post.Run_Internal(Fixed);
				}
				// 同じ区間で経過した時間。
				const auto Elapsed = NowNanoseconds() - Start;
				// 同じ区間で増えた確保数。
				const auto Allocated = TotalAllocations() - AllocationStart;
				if (bFirst)
				{
					Initial[Region][Repeat] = static_cast<Toolbox::f64>(Elapsed) / 1e6;
					InitialAllocations[Region][Repeat] = static_cast<Toolbox::f64>(Allocated);
				}
				if (Step >= 0)
				{
					Times[Region][Repeat] += static_cast<Toolbox::f64>(Elapsed) / 1e6 / Steps;
					Allocations[Region][Repeat] += static_cast<Toolbox::f64>(Allocated) / Steps;
				}
			}
		}
		Diagnostics = World.GetExecutionDiagnostics();
		Sleep = 0;
		for (const auto Body : Dynamic)
		{
			Sleep += World.IsSleeping(Body) ? 1 : 0;
		}
		BodyCount = bShared || bChain ? static_cast<Toolbox::size_t>(Count + 1) : static_cast<Toolbox::size_t>(2 * Count);
		BodyCount += Case == ECase::ContactMix ? 1 : 0;
		JointCount = Case == ECase::None ? 0 : static_cast<Toolbox::size_t>(Count);
		Owner.Shutdown_Internal();
	}
	const char* Regions[] = {"component-resolution-enqueue", "component-pre", "world-step", "component-post"};
	// 最大島は既知の配置から導く。実装の内部時間や一般Worldの最大島測定ではない。
	const Toolbox::int32 MaxIsland = Case == ECase::None ? 0 : (Case == ECase::Chain ? Count : ((Case == ECase::DynamicPairs || Case == ECase::ContactMix) ? 2 : 1));
	for (Toolbox::int32 Region = 0; Region < 4; ++Region)
	{
		if (Region != 2 && Case != ECase::Component && Case != ECase::Toggle)
		{
			continue;
		}
		for (Toolbox::int32 Phase = 0; Phase < 2; ++Phase)
		{
			// 同条件5回の時間の分布。
			const auto Spread = Summarize(Phase == 0 ? Initial[Region] : Times[Region]);
			// 同じ区間で増えた確保数。
			const auto Allocated = Summarize(Phase == 0 ? InitialAllocations[Region] : Allocations[Region]);
			printf("%s,%s,%d,%u,%s,%s,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%zu,%zu,%llu,%llu,%d,%llu,%llu\n", T::Dimension, Name(Case), Count, Lanes, Regions[Region], Phase == 0 ? "first" : "warm", Spread.Median, Spread.Min, Spread.Max, Allocated.Median, Allocated.Min, Allocated.Max, BodyCount, JointCount, Diagnostics.ManifoldCount, Diagnostics.IslandCount, MaxIsland, Diagnostics.SolverIslandCount, Sleep);
		}
	}
}
} // namespace
void RunJointSeries(Toolbox::int32 Warmup, Toolbox::int32 Steps)
{
	printf("# joint warmup=%d measured=%d repeats=%d; first/warm distinct; component uses explicit generational Body refs; max-island derived from fixture topology\n", Warmup, Steps, Repetitions);
	printf("dimension,case,count,lanes,region,phase,median_ms,min_ms,max_ms,alloc_median,alloc_min,alloc_max,bodies,joints,contacts,islands,max_island_derived,job_islands,sleeping\n");
	const ECase Cases[] = {ECase::None, ECase::IndependentStatic, ECase::DynamicPairs, ECase::SharedStatic, ECase::SharedKinematic, ECase::ContactMix, ECase::Chain, ECase::Component, ECase::Toggle};
	// 同じ条件を通す実行レーン。
	const Toolbox::uint32 Lanes[] = {1, 4};
	for (const auto Case : Cases)
	{
		for (Toolbox::int32 Size = 0; Size < 3; ++Size)
		{
			// この条件の接続本数。
			const Toolbox::int32 Count = Case == ECase::Chain ? (Size == 0 ? 8 : (Size == 1 ? 32 : 128)) : (Size == 0 ? 16 : (Size == 1 ? 64 : 256));
			for (const auto Threads : Lanes)
			{
				Measure<FJointBench2D>(Case, Count, Threads, Warmup, Steps);
				Measure<FJointBench3D>(Case, Count, Threads, Warmup, Steps);
			}
		}
	}
}
} // namespace Dxf::Benchmark
