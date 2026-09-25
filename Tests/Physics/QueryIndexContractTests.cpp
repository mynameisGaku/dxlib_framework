// SPDX-License-Identifier: NOASSERTION
// World問い合わせの索引の契約: Stepなしの即時反映（移動・回転・カテゴリ・着脱・スロット再利用・複数Collider）、
// Stepの最終姿勢との同期、途中で失敗したStepからの回復、索引に入れられない形状の失敗の互換、座標の範囲外の総当たり、
// 32件を超える接触の保持順、同じ割合のスロット順、キャラクターの軌跡の一致、問い合わせが物理を変えないこと、診断の集計。
// 期待値は配置からの手計算で、同じ入力の総当たり（参照経路）とも一致させる。
#include "QueryIndexTestSupport.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
#include "Toolbox/JobSystem.h"
using namespace Toolbox;
using namespace Dxf;
using namespace PhysicsTest::QueryIndexTest;
namespace
{
// 比較の許容差（座標100以下のf32の丸め）。
constexpr f64 Tolerance = 1e-5;

// 線分の問い合わせを両方の経路で行い、一致を確かめて索引の結果を返す。
template <typename T>
auto RaySame_Internal(typename T::FWorld& World, typename T::FVector A, typename T::FVector B,
                      const TOptional<typename T::FBody>& Excluded = {}, const FWorldQueryFilter& Filter = {})
{
	auto Indexed = World.RaycastClosest(A, B, Excluded, Filter);
	World.SetQueryIndexEnabled_Internal(false);
	const auto Reference = World.RaycastClosest(A, B, Excluded, Filter);
	World.SetQueryIndexEnabled_Internal(true);
	PHYSICS_REQUIRE(SameSegment(Indexed, Reference));
	return Indexed;
}
// 範囲の問い合わせを両方の経路で行い、一致を確かめて索引の結果を返す。
template <typename T> auto OverlapSame_Internal(typename T::FWorld& World, typename T::FVector Center, f32 Radius)
{
	auto Indexed = World.OverlapAll(typename T::FBall{Center, Radius});
	World.SetQueryIndexEnabled_Internal(false);
	const auto Reference = World.OverlapAll(typename T::FBall{Center, Radius});
	World.SetQueryIndexEnabled_Internal(true);
	PHYSICS_REQUIRE(SameIds(Indexed, Reference));
	return Indexed;
}
// Stepを挟まず、生成・移動・回転・カテゴリ・着脱・スロット再利用・複数Collider・破棄が次の問い合わせに反映される。
template <typename T> void ImmediateUpdates_Internal()
{
	typename T::FWorld World;
	typename T::FBodyDescription Fixed;
	Fixed.Type = EBodyType::Static;
	const auto Body = World.CreateBody(Fixed);
	T::Place(World, Body, T::At(20, 0, 0));
	const auto Id = World.AttachCollider(Body, T::Box(T::At(0, 0, 0), 1, 1));
	// 箱X∈[19,21]へ、X=15から25への線分は割合0.4で入る。
	auto Hit = RaySame_Internal<T>(World, T::At(15, 0, 0), T::At(25, 0, 0));
	PHYSICS_REQUIRE(Hit && Hit->Collider == Id && Abs(Hit->Fraction - 0.4) < Tolerance);
	// Static Bodyの移動も、Stepを待たず反映する。
	T::Place(World, Body, T::At(-20, 0, 0));
	PHYSICS_REQUIRE(!RaySame_Internal<T>(World, T::At(15, 0, 0), T::At(25, 0, 0)));
	Hit = RaySame_Internal<T>(World, T::At(-25, 0, 0), T::At(-15, 0, 0));
	PHYSICS_REQUIRE(Hit && Hit->Collider == Id && Abs(Hit->Fraction - 0.4) < Tolerance);
	// 回転: 細い箱（半幅3×0.1）を90度回すと縦になり、Y=52の横線に当たる。
	const auto Rod = World.CreateBody(Fixed);
	T::Place(World, Rod, T::At(0, 50, 0));
	const auto RodId = World.AttachCollider(Rod, T::Box(T::At(0, 0, 0), 3, 0.1f));
	PHYSICS_REQUIRE(!RaySame_Internal<T>(World, T::At(-1, 52, 0), T::At(1, 52, 0)));
	T::PlaceTurned(World, Rod, T::At(0, 50, 0));
	Hit = RaySame_Internal<T>(World, T::At(-1, 52, 0), T::At(1, 52, 0));
	PHYSICS_REQUIRE(Hit && Hit->Collider == RodId && Abs(Hit->Fraction - 0.45) < 1e-4);
	// カテゴリの0化と復帰。
	World.SetColliderQueryCategory(RodId, 0);
	PHYSICS_REQUIRE(!RaySame_Internal<T>(World, T::At(-1, 52, 0), T::At(1, 52, 0)));
	World.SetColliderQueryCategory(RodId, 4);
	PHYSICS_REQUIRE(RaySame_Internal<T>(World, T::At(-1, 52, 0), T::At(1, 52, 0)));
	// 着脱とスロットの再利用: 新しい世代のIDだけが返る。
	PHYSICS_REQUIRE(World.DetachCollider(Id));
	PHYSICS_REQUIRE(!RaySame_Internal<T>(World, T::At(-25, 0, 0), T::At(-15, 0, 0)));
	const auto Reused = World.AttachCollider(Body, T::Box(T::At(0, 0, 0), 1, 1));
	PHYSICS_REQUIRE(Reused.Index == Id.Index && Reused.Generation != Id.Generation && !World.IsColliderAlive(Id));
	Hit = RaySame_Internal<T>(World, T::At(-25, 0, 0), T::At(-15, 0, 0));
	PHYSICS_REQUIRE(Hit && Hit->Collider == Reused);
	// 複数ColliderのBodyの移動は、すべてのColliderへ反映する。
	const auto Group = World.CreateBody(Fixed);
	const auto A = World.AttachCollider(Group, T::Ball(T::At(0, 0, 0), 0.3f));
	const auto B = World.AttachCollider(Group, T::Ball(T::At(1, 0, 0), 0.3f));
	const auto C = World.AttachCollider(Group, T::Ball(T::At(2, 0, 0), 0.3f));
	T::Place(World, Group, T::At(100, 0, 0));
	auto Found = OverlapSame_Internal<T>(World, T::At(101, 0, 0), 2);
	PHYSICS_REQUIRE(Found.Size() == 3 && Found[0] == A && Found[1] == B && Found[2] == C);
	T::Place(World, Group, T::At(-100, 0, 0));
	PHYSICS_REQUIRE(OverlapSame_Internal<T>(World, T::At(101, 0, 0), 2).IsEmpty());
	PHYSICS_REQUIRE(OverlapSame_Internal<T>(World, T::At(-99, 0, 0), 2).Size() == 3);
	// 破棄すると候補に残らない。
	PHYSICS_REQUIRE(World.DestroyBody(Group));
	PHYSICS_REQUIRE(OverlapSame_Internal<T>(World, T::At(-99, 0, 0), 2).IsEmpty());
}

// 正常なStepの最終姿勢（重力の積分・Kinematicの速度）へ同期し、古い位置を返さない。
template <typename T> void StepSync_Internal()
{
	typename T::FWorld World;
	typename T::FBodyDescription Falling;
	Falling.Position = T::At(0, 100, 0);
	const auto Body = World.CreateBody(Falling);
	World.AttachCollider(Body, T::Ball(T::At(0, 0, 0), 0.5f));
	typename T::FBodyDescription Moving;
	Moving.Type = EBodyType::Kinematic;
	Moving.Position = T::At(0, -50, 0);
	const auto Mover = World.CreateBody(Moving);
	const auto MoverId = World.AttachCollider(Mover, T::Ball(T::At(0, 0, 0), 0.5f));
	World.SetVelocity(Mover, T::At(10, 0, 0));
	for (int32 Index = 0; Index < 30; ++Index)
	{
		World.Step(1.0 / 60.0);
	}
	// 落下した球の上端に当たる。
	const auto Position = T::Position(World, Body);
	PHYSICS_REQUIRE(Position.Y < 99);
	const auto Hit = RaySame_Internal<T>(World, T::At(0, 200, 0), T::At(0, 0, 0));
	PHYSICS_REQUIRE(Hit && Abs(Hit->Position.Y - (Position.Y + 0.5)) < 1e-3);
	PHYSICS_REQUIRE(!RaySame_Internal<T>(World, T::At(-1, 100, 0), T::At(1, 100, 0)));
	// Kinematicは速度で5進む。
	const auto Found = OverlapSame_Internal<T>(World, T::At(5, -50, 0), 0.6f);
	PHYSICS_REQUIRE(Found.Size() == 1 && Found[0] == MoverId);
	PHYSICS_REQUIRE(OverlapSame_Internal<T>(World, T::At(0, -50, 0), 0.2f).IsEmpty());
}

// 途中で失敗したStepの後は拒否し、失敗中に移動しても、次の正常なStepで実際の状態へ回復する。
// 失敗はJob Systemの停止で起こす（位置の更新より前に失敗する経路）。
template <typename T> void FailedStep_Internal()
{
	typename T::FWorld World;
	const auto Body = World.CreateBody({});
	World.AttachCollider(Body, T::Ball(T::At(0, 0, 0), 0.5f));
	FJobSystem Jobs(2);
	Jobs.Shutdown();
	FPhysicsExecutionSettings Broken;
	Broken.JobSystem = &Jobs;
	World.SetExecutionSettings(Broken);
	bool bThrew = false;
	try
	{
		World.Step(1.0 / 60.0);
	}
	catch (const FException&)
	{
		bThrew = true;
	}
	PHYSICS_REQUIRE(bThrew);
	T::Place(World, Body, T::At(30, 0, 0));
	bThrew = false;
	try
	{
		(void)World.OverlapAll(typename T::FBall{T::At(30, 0, 0), 1});
	}
	catch (const FException&)
	{
		bThrew = true;
	}
	PHYSICS_REQUIRE(bThrew);
	World.SetExecutionSettings({});
	World.Step(1.0 / 60.0);
	PHYSICS_REQUIRE(OverlapSame_Internal<T>(World, T::At(30, 0, 0), 1).Size() == 1);
	PHYSICS_REQUIRE(OverlapSame_Internal<T>(World, T::At(0, 0, 0), 1).IsEmpty());
}

// 現在の姿勢のWorld形状が無効なColliderは索引に入れない。今回のカテゴリ・自己除外の対象外なら問い合わせは成功し、
// 対象なら総当たりと同じ例外になる。カテゴリの0化・復帰、姿勢の修正、取り外しにも追従する。
template <typename T> void Unindexed_Internal()
{
	typename T::FWorld World;
	World.SetQueryDiagnosticsEnabled(true);
	typename T::FBodyDescription Fixed;
	Fixed.Type = EBodyType::Static;
	const auto Good = World.CreateBody(Fixed);
	World.AttachCollider(Good, T::Ball(T::At(0, 0, 0), 0.5f));
	const auto Bad = World.CreateBody(Fixed);
	typename T::FColliderDescription Invalid;
	if constexpr (sizeof(typename T::FVector) == sizeof(FVector2))
	{
		// 重心と中心の和がf32で表せない円（World座標で無限大）。
		World.SetBodyTransform(Bad, {3e38f, 0}, 0);
		Invalid.Shape = FCircle2D{{3e38f, 0}, 1};
	}
	else
	{
		// 格納した軸が単位長でないOBB（詳細判定が拒否する）。
		FOBB Box{{0, 0, 0}, {1, 1, 1}};
		Box.Axes[0] = {2, 0, 0};
		Invalid.Shape = Box;
	}
	Invalid.QueryCategory = 2;
	const auto BadId = World.AttachCollider(Bad, Invalid);
	PHYSICS_REQUIRE(World.GetQueryDiagnostics().UnindexedColliders == 1);
	FWorldQueryFilter OnlyGood;
	OnlyGood.IncludeCategories = 1;
	// 対象外なら索引で成功する。
	World.ResetQueryDiagnostics();
	PHYSICS_REQUIRE(RaySame_Internal<T>(World, T::At(-5, 0, 0), T::At(5, 0, 0), {}, OnlyGood));
	PHYSICS_REQUIRE(World.GetQueryDiagnostics().FallbackUnindexed == 0);
	PHYSICS_REQUIRE(RaySame_Internal<T>(World, T::At(-5, 0, 0), T::At(5, 0, 0), Bad, {}));
	// 対象なら、両方の経路で同じ例外。
	auto Outcome = [&](bool bReference)
	{
		World.SetQueryIndexEnabled_Internal(!bReference);
		auto Result = Capture<decltype(World.RaycastClosest(T::At(0, 0, 0), T::At(1, 0, 0)))>(
		    [&]
		    {
			    return World.RaycastClosest(T::At(-5, 0, 0), T::At(5, 0, 0));
		    });
		World.SetQueryIndexEnabled_Internal(true);
		return Result;
	};
	const auto Indexed = Outcome(false);
	PHYSICS_REQUIRE(Indexed.bThrew && SameOutcome(Indexed, Outcome(true),
	                                              [](const auto&, const auto&)
	                                              {
		                                              return true;
	                                              }));
	PHYSICS_REQUIRE(World.GetQueryDiagnostics().FallbackUnindexed == 1);
	// カテゴリ0なら対象外、復帰で再び拒否。
	World.SetColliderQueryCategory(BadId, 0);
	PHYSICS_REQUIRE(!Outcome(false).bThrew);
	World.SetColliderQueryCategory(BadId, 2);
	PHYSICS_REQUIRE(Outcome(false).bThrew);
	if constexpr (sizeof(typename T::FVector) == sizeof(FVector2))
	{
		// 姿勢を戻すと有効な形状になり、索引へ入る。
		World.SetBodyTransform(Bad, {0, 10}, 0);
		PHYSICS_REQUIRE(World.GetQueryDiagnostics().UnindexedColliders == 0);
		PHYSICS_REQUIRE(!Outcome(false).bThrew);
	}
	else
	{
		PHYSICS_REQUIRE(World.DetachCollider(BadId));
		PHYSICS_REQUIRE(World.GetQueryDiagnostics().UnindexedColliders == 0);
		PHYSICS_REQUIRE(!Outcome(false).bThrew);
	}
}

// 座標の絶対値が安全な範囲（2^100）を超える登録・問い合わせは総当たりで処理し、結果は同じ。範囲へ戻れば索引を使う。
template <typename T> void RangeFallback_Internal()
{
	typename T::FWorld World;
	World.SetQueryDiagnosticsEnabled(true);
	typename T::FBodyDescription Fixed;
	Fixed.Type = EBodyType::Static;
	const auto Near = World.CreateBody(Fixed);
	World.AttachCollider(Near, T::Ball(T::At(0, 0, 0), 1));
	const auto Far = World.CreateBody(Fixed);
	T::Place(World, Far, T::At(1e31f, 0, 0));
	World.AttachCollider(Far, T::Ball(T::At(0, 0, 0), 1));
	PHYSICS_REQUIRE(RaySame_Internal<T>(World, T::At(-5, 0, 0), T::At(5, 0, 0)));
	PHYSICS_REQUIRE(World.GetQueryDiagnostics().FallbackRange > 0);
	PHYSICS_REQUIRE(World.DestroyBody(Far));
	World.ResetQueryDiagnostics();
	PHYSICS_REQUIRE(RaySame_Internal<T>(World, T::At(-5, 0, 0), T::At(5, 0, 0)));
	PHYSICS_REQUIRE(World.GetQueryDiagnostics().FallbackRange == 0);
	// 問い合わせ自体が範囲外。
	PHYSICS_REQUIRE(!RaySame_Internal<T>(World, T::At(1e31f, 0, 0), T::At(1e31f, 10, 0)));
	// 索引の経路だけが範囲外として数える（参照経路は無効化として別に数える）。
	PHYSICS_REQUIRE(World.GetQueryDiagnostics().FallbackRange == 1);
}

// 32件を超える接触: スロット昇順の先頭32件と全件数を、スロットの再利用で木の順と違う登録順でも保つ。
template <typename T> void ContactCapacity_Internal()
{
	typename T::FWorld World;
	typename T::FBodyDescription Fixed;
	Fixed.Type = EBodyType::Static;
	FRandom Random(8675309);
	TVector<typename T::FBody> Bodies;
	for (int32 Index = 0; Index < 60; ++Index)
	{
		const auto Body = World.CreateBody(Fixed);
		T::Place(World, Body, T::Point(Random, 0.3f));
		World.AttachCollider(Body, T::Ball(T::At(0, 0, 0), 0.5f));
		Bodies.PushBack(Body);
	}
	for (size_t Index = 0; Index < Bodies.Size(); Index += 3)
	{
		World.DestroyBody(Bodies[Index]);
	}
	for (int32 Index = 0; Index < 20; ++Index)
	{
		const auto Body = World.CreateBody(Fixed);
		T::Place(World, Body, T::Point(Random, 0.3f));
		World.AttachCollider(Body, T::Ball(T::At(0, 0, 0), 0.5f));
	}
	const typename T::FBall Shape{T::At(0, 0, 0), 0.1f};
	const auto Indexed = World.QueryContacts(Shape, 0.5);
	World.SetQueryIndexEnabled_Internal(false);
	const auto Reference = World.QueryContacts(Shape, 0.5);
	World.SetQueryIndexEnabled_Internal(true);
	PHYSICS_REQUIRE(SameContacts(Indexed, Reference));
	PHYSICS_REQUIRE(Indexed.TotalFound == 60 && Indexed.Count == 32 && !Indexed.IsComplete());
	for (uint32 Index = 1; Index < Indexed.Count; ++Index)
	{
		PHYSICS_REQUIRE(Indexed.Items[Index - 1].Collider.Index < Indexed.Items[Index].Collider.Index);
	}
	// 生存する60個のスロットの小さい方から32個（スロットは0～59）。
	PHYSICS_REQUIRE(Indexed.Items[31].Collider.Index == 31);
}

// 同じ割合: スロットの小さい方を返す（大きい方が木で先に見つかる配置でも）。
template <typename T> void TieBreak_Internal()
{
	typename T::FWorld World;
	typename T::FBodyDescription Fixed;
	Fixed.Type = EBodyType::Static;
	const auto First = World.CreateBody(Fixed);
	T::Place(World, First, T::At(500, 0, 0));
	const auto FirstId = World.AttachCollider(First, T::Box(T::At(0, 0, 0), 1, 1));
	const auto Second = World.CreateBody(Fixed);
	World.AttachCollider(Second, T::Box(T::At(0, 0, 0), 1, 1));
	for (int32 Index = 0; Index < 20; ++Index)
	{
		const auto Other = World.CreateBody(Fixed);
		T::Place(World, Other, T::At(static_cast<f32>(Index * 5 + 20), 0, 0));
		World.AttachCollider(Other, T::Box(T::At(0, 0, 0), 1, 1));
	}
	T::Place(World, First, T::At(0, 0, 0));
	const auto Hit = RaySame_Internal<T>(World, T::At(-5, 0, 0), T::At(5, 0, 0));
	PHYSICS_REQUIRE(Hit && Hit->Collider == FirstId && Abs(Hit->Fraction - 0.4) < Tolerance);
	const auto Sweep = World.SweepClosest(typename T::FBall{T::At(-5, 0, 0), 0.5f}, T::At(5, 0, 0));
	PHYSICS_REQUIRE(Sweep && Sweep->Collider == FirstId);
}

// キャラクター移動の型（2D／3D）。
template <typename T> struct TCharacter;
template <> struct TCharacter<F2D>
{
	using FSettings = FCharacterMoveSettings2D;
	using FState = FCharacterState2D;
	using FInput = FCharacterMoveInput2D;
	static FInput Input(f32 Angle, bool bJump)
	{
		FInput Result;
		Result.Move = {Cos(Angle) >= 0 ? 1.0f : -1.0f, 0};
		Result.bJump = bJump;
		return Result;
	}
};
template <> struct TCharacter<F3D>
{
	using FSettings = FCharacterMoveSettings3D;
	using FState = FCharacterState3D;
	using FInput = FCharacterMoveInput3D;
	static FInput Input(f32 Angle, bool bJump)
	{
		FInput Result;
		Result.Move = {static_cast<f32>(Cos(Angle)), 0, static_cast<f32>(Sin(Angle))};
		Result.bJump = bJump;
		return Result;
	}
};
// 別々のWorldのIDは、World番号を除いたスロットと世代で比べる（登録順が同じなら対応する）。
template <typename TId> bool SameLogical_Internal(const TId& A, const TId& B)
{
	return A.Index == B.Index && A.Generation == B.Generation && A.Body.Index == B.Body.Index &&
	       A.Body.Generation == B.Body.Generation;
}
// 同じ配置・入力列のキャラクターを、索引のWorldと総当たりのWorldで進め、固定更新ごとの状態とイベントが一致する。
template <typename T> void Trajectory_Internal()
{
	using FCharacter = TCharacter<T>;
	typename T::FWorld Worlds[2];
	Worlds[1].SetQueryIndexEnabled_Internal(false);
	typename T::FBody Bodies[2][4];
	typename FCharacter::FState States[2][4];
	for (int32 Path = 0; Path < 2; ++Path)
	{
		FRandom Random(55555);
		typename T::FBodyDescription Fixed;
		Fixed.Type = EBodyType::Static;
		const auto Level = Worlds[Path].CreateBody(Fixed);
		Worlds[Path].AttachCollider(Level, T::Floor(40));
		for (int32 Index = 0; Index < 120; ++Index)
		{
			// 低い段差・壁・天井を乱数で置く。
			const f32 X = Random.Range(-38, 38);
			const f32 Z = Random.Range(-38, 38);
			const f32 Height = Random.Chance(50) ? Random.Range(0.05f, 0.25f) : Random.Range(0.5f, 2);
			Worlds[Path].AttachCollider(Level,
			                            T::Box(T::At(X, Height * 0.5f, Z), Random.Range(0.2f, 1.5f), Height * 0.5f));
		}
		for (int32 Index = 0; Index < 4; ++Index)
		{
			typename T::FBodyDescription Description;
			Description.Type = EBodyType::Kinematic;
			Description.Position = T::At(static_cast<f32>(Index * 6 - 9), 3, static_cast<f32>(Index * 3 - 4));
			Bodies[Path][Index] = Worlds[Path].CreateBody(Description);
			Worlds[Path].AttachCollider(Bodies[Path][Index], T::Ball(T::At(0, 0, 0), 0.5f));
			States[Path][Index].Center = Description.Position;
		}
	}
	const typename FCharacter::FSettings Settings;
	for (int32 Step = 0; Step < 300; ++Step)
	{
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const auto Input = FCharacter::Input(static_cast<f32>(Index) * 2.4f + static_cast<f32>(Step) * 0.03f,
			                                     (Step + Index * 7) % 70 == 0);
			decltype(StepCharacter(Worlds[0], Settings, States[0][Index], Input, 1.0 / 60.0)) Results[2];
			for (int32 Path = 0; Path < 2; ++Path)
			{
				Results[Path] =
				    StepCharacter(Worlds[Path], Settings, States[Path][Index], Input, 1.0 / 60.0, Bodies[Path][Index]);
				States[Path][Index] = Results[Path].State;
				T::Place(Worlds[Path], Bodies[Path][Index], States[Path][Index].Center);
			}
			const auto& A = Results[0];
			const auto& B = Results[1];
			PHYSICS_REQUIRE(A.State.Center == B.State.Center && A.State.Velocity == B.State.Velocity);
			PHYSICS_REQUIRE(A.State.Ground.State == B.State.Ground.State &&
			                A.State.Ground.Collider.HasValue() == B.State.Ground.Collider.HasValue());
			PHYSICS_REQUIRE(!A.State.Ground.Collider ||
			                SameLogical_Internal(*A.State.Ground.Collider, *B.State.Ground.Collider));
			PHYSICS_REQUIRE(A.bJumped == B.bJumped && A.bLanded == B.bLanded && A.bSteppedUp == B.bSteppedUp &&
			                A.bHitCeiling == B.bHitCeiling && A.bSnapped == B.bSnapped &&
			                A.bLeftGround == B.bLeftGround);
			PHYSICS_REQUIRE(A.Horizontal.Stop == B.Horizontal.Stop && A.Vertical.Stop == B.Vertical.Stop &&
			                A.Queries == B.Queries && A.Recovery.Status == B.Recovery.Status);
		}
		Worlds[0].Step(1.0 / 60.0);
		Worlds[1].Step(1.0 / 60.0);
	}
}

// 問い合わせ（集計を有効にした状態を含む）の有無で、物理の経過・休止・Step数が変わらない。
template <typename T> void NonInterference_Internal()
{
	typename T::FWorld Worlds[2];
	typename T::FBody Bodies[2][12];
	for (int32 Path = 0; Path < 2; ++Path)
	{
		typename T::FBodyDescription Fixed;
		Fixed.Type = EBodyType::Static;
		const auto Floor = Worlds[Path].CreateBody(Fixed);
		Worlds[Path].AttachCollider(Floor, T::Floor(20));
		for (int32 Index = 0; Index < 12; ++Index)
		{
			typename T::FBodyDescription Description;
			Description.Position =
			    T::At(static_cast<f32>(Index % 4) * 1.1f, 1 + static_cast<f32>(Index / 4) * 1.05f, 0);
			Bodies[Path][Index] = Worlds[Path].CreateBody(Description);
			Worlds[Path].AttachCollider(Bodies[Path][Index], T::Box(T::At(0, 0, 0), 0.5f, 0.5f));
		}
	}
	Worlds[0].SetQueryDiagnosticsEnabled(true);
	FRandom Random(777);
	TVector<typename T::FBody> Excluded;
	Excluded.PushBack(Bodies[0][0]);
	for (int32 Step = 0; Step < 120; ++Step)
	{
		CompareQueries<T>(Worlds[0], Random, Excluded, 10, 6, 777, Step);
		Worlds[0].Step(1.0 / 60.0);
		Worlds[1].Step(1.0 / 60.0);
	}
	for (int32 Index = 0; Index < 12; ++Index)
	{
		PHYSICS_REQUIRE(Worlds[0].GetPosition(Bodies[0][Index]) == Worlds[1].GetPosition(Bodies[1][Index]));
		PHYSICS_REQUIRE(Worlds[0].GetVelocity(Bodies[0][Index]) == Worlds[1].GetVelocity(Bodies[1][Index]));
		PHYSICS_REQUIRE(Worlds[0].IsSleeping(Bodies[0][Index]) == Worlds[1].IsSleeping(Bodies[1][Index]));
	}
	PHYSICS_REQUIRE(Worlds[0].CaptureSnapshot().StepIndex == Worlds[1].CaptureSnapshot().StepIndex);
}

// 診断の集計は有効な間だけ加算し、0へ戻せる。索引の状態は常に読める。
template <typename T> void Diagnostics_Internal()
{
	typename T::FWorld World;
	typename T::FBodyDescription Fixed;
	Fixed.Type = EBodyType::Static;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const auto Body = World.CreateBody(Fixed);
		T::Place(World, Body, T::At(static_cast<f32>(Index) * 10, 0, 0));
		World.AttachCollider(Body, T::Ball(T::At(0, 0, 0), 1));
	}
	(void)World.RaycastClosest(T::At(-5, 0, 0), T::At(5, 0, 0));
	FWorldQueryDiagnostics Diagnostics = World.GetQueryDiagnostics();
	PHYSICS_REQUIRE(Diagnostics.Raycast.Queries == 0 && Diagnostics.AliveColliders == 10 &&
	                Diagnostics.IndexedColliders == 10 && Diagnostics.IndexNodes == 19 &&
	                Diagnostics.IndexMemoryBytes > 0);
	World.SetQueryDiagnosticsEnabled(true);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		(void)World.RaycastClosest(T::At(-5, 0, 0), T::At(5, 0, 0));
	}
	(void)World.QueryContacts(typename T::FBall{T::At(0, 0, 0), 0.5f}, 0.1);
	Diagnostics = World.GetQueryDiagnostics();
	PHYSICS_REQUIRE(Diagnostics.Raycast.Queries == 3 && Diagnostics.Contacts.Queries == 1);
	// 局所の問い合わせは、10個のうち近い1個だけを詳細に判定する。
	PHYSICS_REQUIRE(Diagnostics.Raycast.NarrowTests == 3 && Diagnostics.Raycast.FallbackQueries == 0);
	World.ResetQueryDiagnostics();
	Diagnostics = World.GetQueryDiagnostics();
	PHYSICS_REQUIRE(Diagnostics.Raycast.Queries == 0 && Diagnostics.IndexInserts == 0 &&
	                Diagnostics.AliveColliders == 10);
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"2D query index reflects changes without a Step", &ImmediateUpdates_Internal<F2D>},
    {"3D query index reflects changes without a Step", &ImmediateUpdates_Internal<F3D>},
    {"2D query index follows the final pose of a normal Step", &StepSync_Internal<F2D>},
    {"3D query index follows the final pose of a normal Step", &StepSync_Internal<F3D>},
    {"2D query index recovers after an incomplete Step", &FailedStep_Internal<F2D>},
    {"3D query index recovers after an incomplete Step", &FailedStep_Internal<F3D>},
    {"2D unindexable colliders keep the reference failure contract", &Unindexed_Internal<F2D>},
    {"3D unindexable colliders keep the reference failure contract", &Unindexed_Internal<F3D>},
    {"2D out-of-range coordinates use the reference path", &RangeFallback_Internal<F2D>},
    {"3D out-of-range coordinates use the reference path", &RangeFallback_Internal<F3D>},
    {"2D contacts beyond capacity keep the lowest slots", &ContactCapacity_Internal<F2D>},
    {"3D contacts beyond capacity keep the lowest slots", &ContactCapacity_Internal<F3D>},
    {"2D equal fractions pick the lower slot", &TieBreak_Internal<F2D>},
    {"3D equal fractions pick the lower slot", &TieBreak_Internal<F3D>},
    {"2D character trajectory matches with and without the index", &Trajectory_Internal<F2D>},
    {"3D character trajectory matches with and without the index", &Trajectory_Internal<F3D>},
    {"2D queries do not change the simulation", &NonInterference_Internal<F2D>},
    {"3D queries do not change the simulation", &NonInterference_Internal<F3D>},
    {"2D query diagnostics count only while enabled", &Diagnostics_Internal<F2D>},
    {"3D query diagnostics count only while enabled", &Diagnostics_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetQueryIndexContractCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
