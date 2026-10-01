// SPDX-License-Identifier: NOASSERTION
#include "MechanismScenarios.h"
#include "Dxf/RevoluteJointComponent2D.h"
#include "Dxf/RevoluteJointComponent3D.h"
#include "Dxf/FixedJointComponent2D.h"
#include "Dxf/FixedJointComponent3D.h"
#include "Dxf/PrismaticJointComponent2D.h"
#include "Dxf/PrismaticJointComponent3D.h"
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
enum class EMechanismCase
{
	// 拘束もColliderもない対照。
	None,
	Motor,
	LimitStopped,
	Sleeping,
	DriveChange,
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
const char* Name(EMechanismCase Case)
{
	switch (Case)
	{
	case EMechanismCase::Motor:
		return "motor-active";
	case EMechanismCase::LimitStopped:
		return "limit-stopped";
	case EMechanismCase::Sleeping:
		return "sleeping";
	case EMechanismCase::DriveChange:
		return "component-drive-change";
	case EMechanismCase::None:
		return "no-joint";
	case EMechanismCase::IndependentStatic:
		return "independent-static";
	case EMechanismCase::DynamicPairs:
		return "dynamic-pairs";
	case EMechanismCase::SharedStatic:
		return "shared-static";
	case EMechanismCase::SharedKinematic:
		return "shared-kinematic";
	case EMechanismCase::ContactMix:
		return "contact-joint-mix";
	case EMechanismCase::Chain:
		return "long-chain";
	case EMechanismCase::Component:
		return "component-steady";
	case EMechanismCase::Toggle:
		return "component-toggle";
	}
	throw Toolbox::FException("Unknown joint benchmark case");
}
// 2Dの型と、既存ContextへWorldを渡す箇所。
struct FMechanismBench2D
{
	static void Create(FPhysicsWorld2D& World, FBodyId2D A, FBodyId2D B, EJointKind Kind, EMechanismCase Case)
	{
		if (Kind == EJointKind::Distance)
		{
			FDistanceJointDescription2D Description;
			Description.Length = Distance(World.GetPosition(A), World.GetPosition(B));
			World.CreateDistanceJoint(A, B, Description);
		}
		else if (Kind == EJointKind::Fixed)
		{
			const auto Description = World.MakeFixedJointDescription(A, B, World.GetPosition(A));
			World.CreateFixedJoint(A, B, Description);
		}
		else if (Kind == EJointKind::Revolute)
		{
			auto Description = World.MakeRevoluteJointDescription(A, B, World.GetPosition(A));
			Description.Drive = {Case == EMechanismCase::Motor || Case == EMechanismCase::LimitStopped, 1, 10};
			Description.Limits = {Case == EMechanismCase::LimitStopped, 0, 0};
			World.CreateRevoluteJoint(A, B, Description);
		}
		else
		{
			auto Description = World.MakePrismaticJointDescription(A, B, World.GetPosition(A));
			Description.Drive = {Case == EMechanismCase::Motor || Case == EMechanismCase::LimitStopped, 1, 10};
			Description.Limits = {Case == EMechanismCase::LimitStopped, 0, 0};
			World.CreatePrismaticJoint(A, B, Description);
		}
	}

	using FWorld = FPhysicsWorld2D;
	using FBody = FBodyDescription2D;
	using FBodyId = FBodyId2D;
	using FJoint = FPrismaticJointDescription2D;
	using FComponentDescription = FPrismaticJointComponentDescription2D;
	using FComponent = TBenchJoint<DPrismaticJoint2DComponent>;
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
struct FMechanismBench3D
{
	static void Create(FPhysicsWorld3D& World, FBodyId3D A, FBodyId3D B, EJointKind Kind, EMechanismCase Case)
	{
		if (Kind == EJointKind::Distance)
		{
			FDistanceJointDescription3D Description;
			Description.Length = Distance(World.GetPosition(A), World.GetPosition(B));
			World.CreateDistanceJoint(A, B, Description);
		}
		else if (Kind == EJointKind::Fixed)
		{
			const auto Description = World.MakeFixedJointDescription(A, B, World.GetPosition(A));
			World.CreateFixedJoint(A, B, Description);
		}
		else if (Kind == EJointKind::Revolute)
		{
			auto Description = World.MakeRevoluteJointDescription(A, B, World.GetPosition(A));
			Description.Drive = {Case == EMechanismCase::Motor || Case == EMechanismCase::LimitStopped, 1, 10};
			Description.Limits = {Case == EMechanismCase::LimitStopped, 0, 0};
			World.CreateRevoluteJoint(A, B, Description);
		}
		else
		{
			auto Description = World.MakePrismaticJointDescription(A, B, World.GetPosition(A));
			Description.Drive = {Case == EMechanismCase::Motor || Case == EMechanismCase::LimitStopped, 1, 10};
			Description.Limits = {Case == EMechanismCase::LimitStopped, 0, 0};
			World.CreatePrismaticJoint(A, B, Description);
		}
	}

	using FWorld = FPhysicsWorld3D;
	using FBody = FBodyDescription3D;
	using FBodyId = FBodyId3D;
	using FJoint = FPrismaticJointDescription3D;
	using FComponentDescription = FPrismaticJointComponentDescription3D;
	using FComponent = TBenchJoint<DPrismaticJoint3DComponent>;
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
void Measure(EJointKind Kind, EMechanismCase Case, Toolbox::int32 Count, Toolbox::uint32 Lanes, Toolbox::int32 Warmup, Toolbox::int32 Steps)
{
	// 区間ごとの慣らし後時間。
	Toolbox::f64 Times[4][Repetitions]{};
	// 区間ごとの慣らし後確保数。
	Toolbox::f64 Allocations[4][Repetitions]{};
	// 初回の区間時間。
	Toolbox::f64 Initial[4][Repetitions]{};
	// 初回の区間確保数。
	Toolbox::f64 InitialAllocations[4][Repetitions]{};
	// 初回を除く慣らし区間の平均。
	Toolbox::f64 WarmTimes[4][Repetitions]{};
	Toolbox::f64 WarmAllocations[4][Repetitions]{};
	// 最終状態の活動数。
	Toolbox::uint64 Active = 0;
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
		const bool bShared = Case == EMechanismCase::SharedStatic || Case == EMechanismCase::SharedKinematic;
		const bool bChain = Case == EMechanismCase::Chain;
		const bool bComponent = Case == EMechanismCase::Component || Case == EMechanismCase::Toggle || Case == EMechanismCase::DriveChange;
		// 支点の生成条件。
		typename T::FBody Anchor;
		Anchor.Type = Case == EMechanismCase::SharedKinematic ? EBodyType::Kinematic : EBodyType::Static;
		Anchor.Position = T::At(0, 3);
		typename T::FBodyId Shared;
		if (bShared || bChain)
		{
			Shared = World.CreateBody(Anchor);
		}
		if (Case == EMechanismCase::SharedKinematic)
		{
			World.SetVelocity(Shared, T::At(0, 0.1f));
		}
		if (Case == EMechanismCase::ContactMix)
		{
			T::Floor(World);
		}
		for (Toolbox::int32 Index = 0; Index < Count; ++Index)
		{
			// 配置または投影に使う位置。
			const auto Position = T::At(static_cast<Toolbox::f32>(Index * 4), 2.2f);
			Anchor.Position = Position;
			Anchor.Type = Case == EMechanismCase::DynamicPairs || Case == EMechanismCase::ContactMix || Case == EMechanismCase::None ? EBodyType::Dynamic : EBodyType::Static;
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
			Description = World.MakePrismaticJointDescription(A, B, World.GetPosition(A));
			if (Case == EMechanismCase::ContactMix)
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
			else if (Case != EMechanismCase::None)
			{
				T::Create(World, A, B, Case == EMechanismCase::ContactMix ? static_cast<EJointKind>(Index % 4) : Kind, Case);
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
			// 初回・残りの慣らし・定常を別々に集計する。
			const bool bFirst = Step == -Warmup;
			// 休止系列を除いて、静的拘束にも毎Step小さい外力を与える。
			if (Case != EMechanismCase::Sleeping && Case != EMechanismCase::LimitStopped)
			{
				for (const auto Body : Dynamic)
				{
					World.ApplyLinearImpulse(Body, T::At(0.01f, 0));
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
					if (Case == EMechanismCase::DriveChange)
					{
						for (const auto Handle : Components)
						{
							Handle.Get()->RequestDrive({true, Step % 2 == 0 ? 1.0 : -1.0, 10});
						}
					}
					if (Case == EMechanismCase::Component)
					{
						for (const auto Handle : Components)
						{
							Handle.Get()->RequestConnect(Handle.Get()->GetDescription());
						}
					}
					if (Case == EMechanismCase::Toggle && Step >= 0)
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
				if (Step < 0 && !bFirst)
				{
					WarmTimes[Region][Repeat] += static_cast<Toolbox::f64>(Elapsed) / 1e6 / (Warmup - 1);
					WarmAllocations[Region][Repeat] += static_cast<Toolbox::f64>(Allocated) / (Warmup - 1);
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
		Active = Dynamic.Size() - Sleep;
		BodyCount = bShared || bChain ? static_cast<Toolbox::size_t>(Count + 1) : static_cast<Toolbox::size_t>(2 * Count);
		BodyCount += Case == EMechanismCase::ContactMix ? 1 : 0;
		JointCount = Case == EMechanismCase::None ? 0 : static_cast<Toolbox::size_t>(Count);
		Owner.Shutdown_Internal();
	}
	const char* Regions[] = {"component-resolution-enqueue", "component-pre", "world-step", "component-post"};
	// 最大島は既知の配置から導く。実装の内部時間や一般Worldの最大島測定ではない。
	const Toolbox::int32 MaxIsland = Case == EMechanismCase::None ? 0 : (Case == EMechanismCase::Chain ? Count : ((Case == EMechanismCase::DynamicPairs || Case == EMechanismCase::ContactMix) ? 2 : 1));
	// 種類数と基本拘束行数は登録条件から算出し、活動行の実測とは区別する。
	Toolbox::int32 Kinds[4]{};
	for (Toolbox::int32 Index = 0; Index < Count; ++Index)
	{
		++Kinds[static_cast<Toolbox::int32>(Case == EMechanismCase::ContactMix ? static_cast<EJointKind>(Index % 4) : Kind)];
	}
	const Toolbox::int32 BasicRows = Kinds[0] + Kinds[1] * (T::Dimension[0] == '2' ? 2 : 5) + Kinds[2] * (T::Dimension[0] == '2' ? 3 : 6) + Kinds[3] * (T::Dimension[0] == '2' ? 2 : 5);
	const Toolbox::int32 MotorRows = Case == EMechanismCase::Motor || Case == EMechanismCase::LimitStopped || Case == EMechanismCase::DriveChange ? Count : 0;
	const Toolbox::int32 LimitRows = Case == EMechanismCase::LimitStopped ? Count : 0;
	for (Toolbox::int32 Region = 0; Region < 4; ++Region)
	{
		if (Region != 2 && Case != EMechanismCase::Component && Case != EMechanismCase::Toggle && Case != EMechanismCase::DriveChange)
		{
			continue;
		}
		for (Toolbox::int32 Phase = 0; Phase < 3; ++Phase)
		{
			// 同条件5回の時間の分布。
			const auto Spread = Summarize(Phase == 0 ? Initial[Region] : Phase == 1 ? WarmTimes[Region] : Times[Region]);
			// 同じ区間で増えた確保数。
			const auto Allocated = Summarize(Phase == 0 ? InitialAllocations[Region] : Phase == 1 ? WarmAllocations[Region] : Allocations[Region]);
			printf("%s,%d,%s,%d,%u,%s,%s,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%zu,%zu,%llu,%llu,%d,%llu,%llu,%llu,%d,%d,%d,%d,%d,%d,%d,%d\n", T::Dimension, static_cast<Toolbox::int32>(Kind), Name(Case), Count, Lanes, Regions[Region], Phase == 0 ? "first" : Phase == 1 ? "warmup" : "steady", Spread.Median, Spread.Min, Spread.Max, Allocated.Median, Allocated.Min, Allocated.Max, BodyCount, JointCount, Diagnostics.ManifoldCount, Diagnostics.IslandCount, MaxIsland, Diagnostics.SolverIslandCount, Sleep, Active, Kinds[0], Kinds[1], Kinds[2], Kinds[3], BasicRows, MotorRows, LimitRows, BasicRows + MotorRows + LimitRows);
		}
	}
}
} // namespace
void RunMechanismSeries(Toolbox::int32 Warmup, Toolbox::int32 Steps)
{
	printf("# normal product, 5 fresh worlds; ms/Step; topology maximum is derived, diagnostics islands measured; 1 means synchronous nullptr; component rows use prismatic\n");
	printf("dimension,kind,case,count,lanes,region,phase,median_ms,min_ms,max_ms,alloc_median,alloc_min,alloc_max,bodies,joints,contacts,islands,max_island_derived,job_islands,sleeping,active,distance,revolute,fixed,prismatic,configured_basic_rows,configured_motor_rows,configured_limit_rows,configured_total_rows\n");
	const EMechanismCase Cases[] = {EMechanismCase::IndependentStatic, EMechanismCase::SharedStatic, EMechanismCase::SharedKinematic, EMechanismCase::ContactMix, EMechanismCase::Chain, EMechanismCase::Motor, EMechanismCase::LimitStopped, EMechanismCase::Sleeping, EMechanismCase::Component, EMechanismCase::DriveChange, EMechanismCase::Toggle};
	const Toolbox::uint32 Lanes[] = {1, 4};
	for (const auto Case : Cases)
	{
		for (Toolbox::int32 Size = 0; Size < 3; ++Size)
		{
			const Toolbox::int32 Count = Case == EMechanismCase::Chain ? (Size == 0 ? 8 : Size == 1 ? 32 : 128)
			                                                           : (Size == 0 ? 16 : Size == 1 ? 64 : 256);
			for (const auto Threads : Lanes)
			{
				const EJointKind Kinds[] = {EJointKind::Revolute, EJointKind::Fixed, EJointKind::Prismatic};
				for (const auto Kind : Kinds)
				{
					if ((Case == EMechanismCase::Motor || Case == EMechanismCase::LimitStopped) && Kind == EJointKind::Fixed)
					{
						continue;
					}
					if ((Case == EMechanismCase::Component || Case == EMechanismCase::DriveChange || Case == EMechanismCase::Toggle || Case == EMechanismCase::ContactMix) && Kind != EJointKind::Prismatic)
					{
						continue;
					}
					Measure<FMechanismBench2D>(Kind, Case, Count, Threads, Case == EMechanismCase::Sleeping ? 90 : Warmup, Steps);
					Measure<FMechanismBench3D>(Kind, Case, Count, Threads, Case == EMechanismCase::Sleeping ? 90 : Warmup, Steps);
				}
			}
		}
	}
	// 8レーンは代表的な独立島と一本鎖だけで、追加費用を照合する。
	Measure<FMechanismBench2D>(EJointKind::Fixed, EMechanismCase::IndependentStatic, 64, 8, Warmup, Steps);
	Measure<FMechanismBench3D>(EJointKind::Fixed, EMechanismCase::IndependentStatic, 64, 8, Warmup, Steps);
	Measure<FMechanismBench2D>(EJointKind::Prismatic, EMechanismCase::Chain, 32, 8, Warmup, Steps);
	Measure<FMechanismBench3D>(EJointKind::Prismatic, EMechanismCase::Chain, 32, 8, Warmup, Steps);
}
} // namespace Dxf::Benchmark
