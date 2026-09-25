// SPDX-License-Identifier: NOASSERTION
// 問い合わせ索引の確保:
// 登録の途中の確保失敗で、索引・生存数・問い合わせの結果が失敗前のままであること（幽霊の葉がない）。
// 登録・移動・取り外し・カテゴリの変更の直後の最初の問い合わせ、姿勢の変更（索引の入れ直し）で確保しないこと。
#include "QueryIndexAllocationFaultTests.h"
#include "QueryIndexTestSupport.h"
#include "../../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
using namespace Toolbox;
using namespace Dxf;
using namespace PhysicsTest::QueryIndexTest;
namespace
{
// 失敗した確認の数。
int32 GFailures = 0;

// 1項目の結果を出力する。
void Check_Internal(bool bOk, const char* Dimension, const char* Name)
{
	printf("%s %s %s\n", bOk ? "PASS" : "FAIL", Dimension, Name);
	fflush(stdout);
	GFailures += bOk ? 0 : 1;
}
// 関数の実行中に確保が起きないか（起きれば注入で失敗する）。
template <typename F> bool WithoutAllocation_Internal(F&& Run)
{
	Testing::SetAllocationFailureCountdown(0);
	bool bOk = true;
	try
	{
		Run();
	}
	catch (const FException&)
	{
		bOk = false;
	}
	bOk = bOk && !Testing::WasAllocationFailureInjected();
	Testing::SetAllocationFailureCountdown(-1);
	return bOk;
}
// 状態の要約（生存数と索引に入っている数）。
template <typename TWorld> bool SameState_Internal(const TWorld& World, const FWorldQueryDiagnostics& Before)
{
	const FWorldQueryDiagnostics Now = World.GetQueryDiagnostics();
	return Now.AliveColliders == Before.AliveColliders && Now.IndexedColliders == Before.IndexedColliders &&
	       Now.UnindexedColliders == Before.UnindexedColliders;
}

template <typename T> void Run_Internal()
{
	typename T::FWorld World;
	typename T::FBodyDescription Fixed;
	Fixed.Type = EBodyType::Static;
	// 線分X=-10～10の上に並べた球（X=2,4,6,8）。
	for (int32 Index = 1; Index <= 4; ++Index)
	{
		const auto Body = World.CreateBody(Fixed);
		T::Place(World, Body, T::At(static_cast<f32>(Index * 2), 0, 0));
		World.AttachCollider(Body, T::Ball(T::At(0, 0, 0), 0.5f));
	}
	const auto Before = World.RaycastClosest(T::At(-10, 0, 0), T::At(10, 0, 0));
	// Colliderの登録: 何回目の確保で失敗しても、索引と結果は失敗前のまま。最後は成功して最も近い球になる。
	const auto Target = World.CreateBody(Fixed);
	T::Place(World, Target, T::At(-5, 0, 0));
	bool bNoGhost = true;
	int32 Failed = 0;
	typename T::FCollider Added;
	for (int64 Countdown = 0; Countdown < 64; ++Countdown)
	{
		const FWorldQueryDiagnostics State = World.GetQueryDiagnostics();
		Testing::SetAllocationFailureCountdown(Countdown);
		bool bThrown = false;
		try
		{
			Added = World.AttachCollider(Target, T::Ball(T::At(0, 0, 0), 0.5f));
		}
		catch (const FException&)
		{
			bThrown = true;
		}
		const bool bInjected = Testing::WasAllocationFailureInjected();
		Testing::SetAllocationFailureCountdown(-1);
		if (!bThrown)
		{
			break;
		}
		++Failed;
		bNoGhost = bNoGhost && bInjected && SameState_Internal(World, State) &&
		           SameSegment(World.RaycastClosest(T::At(-10, 0, 0), T::At(10, 0, 0)), Before);
	}
	const auto After = World.RaycastClosest(T::At(-10, 0, 0), T::At(10, 0, 0));
	Check_Internal(bNoGhost && Failed > 0 && After && After->Collider == Added, T::Name,
	               "collider registration allocation failures leave no ghost in the query index");
	// Bodyの生成: 失敗しても索引の一覧は壊れず、後の登録と問い合わせが正しい。
	bool bBodyOk = true;
	int32 BodyFailures = 0;
	for (int64 Countdown = 0; Countdown < 64; ++Countdown)
	{
		const FWorldQueryDiagnostics State = World.GetQueryDiagnostics();
		Testing::SetAllocationFailureCountdown(Countdown);
		bool bThrown = false;
		typename T::FBody Created;
		try
		{
			Created = World.CreateBody(Fixed);
		}
		catch (const FException&)
		{
			bThrown = true;
		}
		Testing::SetAllocationFailureCountdown(-1);
		if (!bThrown)
		{
			T::Place(World, Created, T::At(-8, 0, 0));
			const auto Ball = World.AttachCollider(Created, T::Ball(T::At(0, 0, 0), 0.5f));
			const auto Hit = World.RaycastClosest(T::At(-10, 0, 0), T::At(10, 0, 0));
			bBodyOk = bBodyOk && Hit && Hit->Collider == Ball;
			break;
		}
		++BodyFailures;
		bBodyOk = bBodyOk && SameState_Internal(World, State);
	}
	Check_Internal(bBodyOk, T::Name, "body creation allocation failures keep the query index consistent");
	// 変更の直後の最初の問い合わせと、姿勢の変更（入れ直し）は確保しない。
	const typename T::FBall Sphere{T::At(-5, 0, 0), 0.4f};
	const typename T::FBall Empty{T::At(0, 100, 0), 1};
	auto Queries = [&]
	{
		(void)World.RaycastClosest(T::At(-10, 0, 0), T::At(10, 0, 0));
		(void)World.SweepClosest(Sphere, T::At(10, 0, 0));
		(void)World.SweepClosestIgnoringInitialContacts(Sphere, T::At(10, 0, 0));
		(void)World.QueryContacts(Sphere, 0.5);
		(void)World.OverlapAll(Empty);
	};
	const auto Moving = World.CreateBody(Fixed);
	const auto MovingId = World.AttachCollider(Moving, T::Ball(T::At(0, 0, 0), 0.5f));
	bool bQueries = WithoutAllocation_Internal(Queries);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		bQueries = bQueries && WithoutAllocation_Internal(
		                           [&]
		                           {
			                           // 余裕を持たせた境界から出る移動（入れ直し）。
			                           T::Place(World, Moving, T::At(static_cast<f32>(Index) * 3 - 30, 0, 0));
		                           });
		bQueries = bQueries && WithoutAllocation_Internal(Queries);
	}
	World.SetColliderQueryCategory(MovingId, 0);
	bQueries = bQueries && WithoutAllocation_Internal(Queries);
	World.SetColliderQueryCategory(MovingId, 1);
	bQueries = bQueries && WithoutAllocation_Internal(Queries);
	// 取り外し自体は空きスロットの記録を追加するため対象外（変更前からの動作）。直後の問い合わせを確かめる。
	(void)World.DetachCollider(MovingId);
	bQueries = bQueries && WithoutAllocation_Internal(Queries);
	Check_Internal(bQueries, T::Name, "queries after changes and index reinsertion do not allocate");
}
} // namespace

Toolbox::int32 PhysicsTest::RunQueryIndexAllocationChecks()
{
	GFailures = 0;
	Run_Internal<F2D>();
	Run_Internal<F3D>();
	return GFailures;
}
