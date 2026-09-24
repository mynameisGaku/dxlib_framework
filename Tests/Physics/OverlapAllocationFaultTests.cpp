// SPDX-License-Identifier: NOASSERTION
// 範囲問い合わせの結果配列の確保失敗。故障注入はこの隔離した実行ファイルだけにリンクする。
// 確認: 空結果は確保しない／最初と途中の確保失敗で部分結果を返さない／以前の結果とWorldを変えない／
// 注入解除後に同じWorldで全件取得へ戻る／この区間の一時配列に由来する未解放が増えない。
// あわせて、SweepClosestの通常経路（ヒット・非交差・静止・マスク0・半径0）が確保しないことを確認する。
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "../../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
#include <stdio.h>
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 失敗した確認の数。
int32 GFailures = 0;

// 1項目の結果を出力する。
void Check_Internal(bool bOk, const char* Name)
{
	printf("%s %s\n", bOk ? "PASS" : "FAIL", Name);
	fflush(stdout);
	GFailures += bOk ? 0 : 1;
}

// 2つのID配列が同じ値の並びか調べる。
template <typename TId> bool Same_Internal(const TVector<TId>& A, const TVector<TId>& B)
{
	if (A.Size() != B.Size())
	{
		return false;
	}
	for (size_t Index = 0; Index < A.Size(); ++Index)
	{
		if (!(A[Index] == B[Index]))
		{
			return false;
		}
	}
	return true;
}

// 指定回目の確保を失敗させて問い合わせる。Previousは失敗時に変わらないはずの以前の結果。
// 戻り値: 0=注入されず成功、1=注入されて失敗、2=契約違反。
template <typename TWorld, typename TArea, typename TId>
int32 Attempt_Internal(const TWorld& World, const TArea& Area, int64 Countdown, TVector<TId>& Previous)
{
	bool bThrown = false;
	Testing::SetAllocationFailureCountdown(Countdown);
	try
	{
		Previous = World.OverlapAll(Area);
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	const bool bInjected = Testing::WasAllocationFailureInjected();
	Testing::SetAllocationFailureCountdown(-1);
	if (bInjected != bThrown)
	{
		return 2;
	}
	return bInjected ? 1 : 0;
}

// 空結果では注入しても確保が起きないこと。
template <typename TWorld, typename TArea>
bool EmptyWithoutAllocation_Internal(const TWorld& World, const TArea& Area, const FWorldQueryFilter& Filter)
{
	Testing::SetAllocationFailureCountdown(0);
	bool bOk = true;
	try
	{
		bOk = World.OverlapAll(Area, {}, Filter).IsEmpty();
	}
	catch (const FException&)
	{
		bOk = false;
	}
	bOk = bOk && !Testing::WasAllocationFailureInjected();
	Testing::SetAllocationFailureCountdown(-1);
	return bOk;
}

// 一つの次元について、確保の各位置を順に失敗させる。
template <typename TWorld, typename TArea, typename TColliderId>
void Run_Internal(const char* Name, TWorld& World, const TArea& Area, const TArea& EmptyArea, TColliderId Probe)
{
	char Label[256];
	// 注入前に期待値と以前の結果を用意する。
	const TVector<TColliderId> Expected = World.OverlapAll(Area);
	snprintf(Label, sizeof(Label), "%s expected result has several ids", Name);
	Check_Internal(Expected.Size() == 6, Label);
	FWorldQueryFilter None;
	None.IncludeCategories = 0u;
	snprintf(Label, sizeof(Label), "%s empty results do not allocate", Name);
	Check_Internal(EmptyWithoutAllocation_Internal(World, EmptyArea, FWorldQueryFilter{}) &&
	                   EmptyWithoutAllocation_Internal(World, Area, None),
	               Label);
	TVector<TColliderId> Previous;
	Previous.PushBack(Probe);
	const uint32 CategoryBefore = World.GetColliderQueryCategory(Probe);
	int32 Injected = 0;
	bool bContract = true;
	bool bPreserved = true;
	bool bBalanced = true;
	bool bRecovered = false;
	for (int64 Position = 0; Position < 64 && !bRecovered; ++Position)
	{
		const uint64 Before = Testing::GetOutstandingTestAllocations();
		const int32 Result = Attempt_Internal(World, Area, Position, Previous);
		if (Result == 2)
		{
			bContract = false;
			break;
		}
		if (Result == 1)
		{
			++Injected;
			// 失敗時は以前の結果を部分配列へ差し替えない。
			bPreserved = bPreserved && Previous.Size() == 1 && Previous[0] == Probe;
			bBalanced = bBalanced && Testing::GetOutstandingTestAllocations() == Before;
			continue;
		}
		// 注入位置が確保回数を超えると、同じWorldで全件取得に成功する。
		bRecovered = Same_Internal(Previous, Expected);
	}
	snprintf(Label, sizeof(Label), "%s injection reached first and growth allocations", Name);
	Check_Internal(bContract && Injected >= 2, Label);
	snprintf(Label, sizeof(Label), "%s failed query keeps the previous result", Name);
	Check_Internal(bPreserved, Label);
	snprintf(Label, sizeof(Label), "%s failed query leaves no tracked allocation", Name);
	Check_Internal(bBalanced, Label);
	snprintf(Label, sizeof(Label), "%s recovers the full result after injection", Name);
	Check_Internal(bRecovered, Label);
	snprintf(Label, sizeof(Label), "%s world ids and categories unchanged", Name);
	Check_Internal(World.IsColliderAlive(Probe) && World.GetColliderQueryCategory(Probe) == CategoryBefore &&
	                   Same_Internal(World.OverlapAll(Area), Expected),
	               Label);
}
// 呼出し中に確保が起きないこと（次の確保を失敗させても注入されず、例外にならない）。
template <typename F> bool WithoutAllocation_Internal(F&& Run)
{
	Testing::SetAllocationFailureCountdown(0);
	bool bOk = true;
	try
	{
		bOk = Run();
	}
	catch (const FException&)
	{
		bOk = false;
	}
	bOk = bOk && !Testing::WasAllocationFailureInjected();
	Testing::SetAllocationFailureCountdown(-1);
	return bOk;
}

// SweepClosestの通常経路（ヒット・非交差・静止・マスク0・半径0）は確保しない。
template <typename TWorld, typename TProbe, typename TVector>
void SweepWithoutAllocation_Internal(const char* Name, const TWorld& World, const TProbe& Probe, TVector End,
                                     TVector Away)
{
	char Label[256];
	FWorldQueryFilter None;
	None.IncludeCategories = 0u;
	TProbe Point = Probe;
	Point.Radius = 0;
	snprintf(Label, sizeof(Label), "%s sweep normal paths do not allocate", Name);
	Check_Internal(WithoutAllocation_Internal(
	                   [&]
	                   {
		                   return static_cast<bool>(World.SweepClosest(Probe, End));
	                   }) &&
	                   WithoutAllocation_Internal(
	                       [&]
	                       {
		                       return !World.SweepClosest(Probe, Away);
	                       }) &&
	                   WithoutAllocation_Internal(
	                       [&]
	                       {
		                       return !World.SweepClosest(Probe, Probe.Center);
	                       }) &&
	                   WithoutAllocation_Internal(
	                       [&]
	                       {
		                       return !World.SweepClosest(Probe, End, {}, None);
	                       }) &&
	                   WithoutAllocation_Internal(
	                       [&]
	                       {
		                       return static_cast<bool>(World.SweepClosest(Point, End));
	                       }),
	               Label);
}
} // namespace

int main()
{
	{
		FPhysicsWorld2D World;
		const FBodyId2D Body = World.CreateBody({});
		FColliderDescription2D Description;
		FColliderId2D Probe;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Description.Shape = FCircle2D{{static_cast<f32>(Index), 0}, 0.5f};
			Probe = World.AttachCollider(Body, Description);
		}
		Run_Internal("2D", World, FCircle2D{{2, 0}, 10}, FCircle2D{{0, 100}, 1}, Probe);
		SweepWithoutAllocation_Internal("2D", World, FCircle2D{{-5, 0}, 0.25f}, FVector2{10, 0}, FVector2{-5, 50});
	}
	{
		FPhysicsWorld3D World;
		const FBodyId3D Body = World.CreateBody({});
		FColliderDescription3D Description;
		FColliderId3D Probe;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Description.Shape = FSphere{{static_cast<f32>(Index), 0, 0}, 0.5f};
			Probe = World.AttachCollider(Body, Description);
		}
		Run_Internal("3D", World, FSphere{{2, 0, 0}, 10}, FSphere{{0, 100, 0}, 1}, Probe);
		SweepWithoutAllocation_Internal("3D", World, FSphere{{-5, 0, 0}, 0.25f}, FVector3{10, 0, 0},
		                                FVector3{-5, 50, 0});
	}
	printf("RESULT %s failures=%d\n", GFailures == 0 ? "PASS" : "FAIL", GFailures);
	return GFailures == 0 ? 0 : 1;
}
