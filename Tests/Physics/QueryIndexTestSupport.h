// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_TEST_QUERY_INDEX_TEST_SUPPORT_H
#define DXF_PHYSICS_TEST_QUERY_INDEX_TEST_SUPPORT_H
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/Vector.h"
#include <stdio.h>
#include <string.h>
namespace PhysicsTest::QueryIndexTest
{
/**
 * 種を固定した乱数（xorshift64*）。試験の再現のため、失敗時は種と手順の番号を出す。
 */
class FRandom
{
public:
	/**
	 * 種を受け取る（0は1へ置き換える）。
	 * @param Seed 種。
	 */
	explicit FRandom(Toolbox::uint64 Seed) : m_State(Seed == 0 ? 1 : Seed)
	{
	}
	/**
	 * 次の64bitの値を返す。
	 */
	Toolbox::uint64 Next() noexcept
	{
		m_State ^= m_State >> 12;
		m_State ^= m_State << 25;
		m_State ^= m_State >> 27;
		return m_State * 2685821657736338717ULL;
	}
	/**
	 * [0, Count)の整数を返す。
	 * @param Count 上限（正）。
	 */
	Toolbox::int32 Below(Toolbox::int32 Count) noexcept
	{
		return static_cast<Toolbox::int32>(Next() % static_cast<Toolbox::uint64>(Count));
	}
	/**
	 * [Min, Max]のf32を返す。
	 * @param Min 下限。
	 * @param Max 上限。
	 */
	Toolbox::f32 Range(Toolbox::f32 Min, Toolbox::f32 Max) noexcept
	{
		const Toolbox::f64 Unit = static_cast<Toolbox::f64>(Next() >> 11) / 9007199254740992.0;
		return static_cast<Toolbox::f32>(Min + (Max - Min) * Unit);
	}
	/**
	 * 確率Percent%でtrueを返す。
	 * @param Percent 0～100。
	 */
	bool Chance(Toolbox::int32 Percent) noexcept
	{
		return Below(100) < Percent;
	}

private:
	// 状態。
	Toolbox::uint64 m_State;
};

/**
 * 呼出しの結果（例外ならそのメッセージ）。
 */
template <typename T> struct TOutcome
{
	/**
	 * 例外で終わったか。
	 */
	bool bThrew = false;
	/**
	 * 例外のメッセージ（先頭だけ）。
	 */
	char Message[160]{};
	/**
	 * 戻り値（例外でなければ）。
	 */
	T Value{};
};
/**
 * 関数を呼び、戻り値または例外を記録する。
 * @param Run 呼び出す関数。
 */
template <typename T, typename F> TOutcome<T> Capture(F&& Run)
{
	TOutcome<T> Result;
	try
	{
		Result.Value = Run();
	}
	catch (const Toolbox::FException& Error)
	{
		Result.bThrew = true;
		snprintf(Result.Message, sizeof(Result.Message), "%s", Error.What());
	}
	return Result;
}

/**
 * 任意の値の一致（両方空、または両方あって値が同じ）。
 * @param A 一つ目。
 * @param B 二つ目。
 */
template <typename V> bool SameOptional(const Toolbox::TOptional<V>& A, const Toolbox::TOptional<V>& B)
{
	if (A.HasValue() != B.HasValue())
	{
		return false;
	}
	return !A.HasValue() || *A == *B;
}
/**
 * 線分の結果の一致（完全なID・割合・交点）。
 * @param A 一つ目。
 * @param B 二つ目。
 */
template <typename THit> bool SameSegment(const Toolbox::TOptional<THit>& A, const Toolbox::TOptional<THit>& B)
{
	if (A.HasValue() != B.HasValue())
	{
		return false;
	}
	return !A.HasValue() || (A->Collider == B->Collider && A->Fraction == B->Fraction && A->Position == B->Position);
}
/**
 * スイープの結果の一致（完全なID・割合・中心・初期接触・法線の有無と値）。
 * @param A 一つ目。
 * @param B 二つ目。
 */
template <typename THit> bool SameSweep(const Toolbox::TOptional<THit>& A, const Toolbox::TOptional<THit>& B)
{
	if (A.HasValue() != B.HasValue())
	{
		return false;
	}
	if (!A.HasValue())
	{
		return true;
	}
	return A->Collider == B->Collider && A->Fraction == B->Fraction && A->CenterAtHit == B->CenterAtHit &&
	       A->bInitialContact == B->bInitialContact && SameOptional(A->Normal, B->Normal);
}
/**
 * 接触の集合の一致（件数・全件数・順序・ID・距離・法線）。
 * @param A 一つ目。
 * @param B 二つ目。
 */
template <typename TSet> bool SameContacts(const TSet& A, const TSet& B)
{
	if (A.Count != B.Count || A.TotalFound != B.TotalFound)
	{
		return false;
	}
	for (Toolbox::uint32 Index = 0; Index < A.Count; ++Index)
	{
		if (!(A.Items[Index].Collider == B.Items[Index].Collider) ||
		    A.Items[Index].Separation != B.Items[Index].Separation ||
		    !SameOptional(A.Items[Index].Normal, B.Items[Index].Normal))
		{
			return false;
		}
	}
	return true;
}
/**
 * IDの列の一致（順序を含む）。
 * @param A 一つ目。
 * @param B 二つ目。
 */
template <typename TId> bool SameIds(const Toolbox::TVector<TId>& A, const Toolbox::TVector<TId>& B)
{
	if (A.Size() != B.Size())
	{
		return false;
	}
	for (Toolbox::size_t Index = 0; Index < A.Size(); ++Index)
	{
		if (!(A[Index] == B[Index]))
		{
			return false;
		}
	}
	return true;
}
/**
 * 二つの結果（例外を含む）が一致するか。
 * @param A 一つ目。
 * @param B 二つ目。
 * @param Same 戻り値の比較。
 */
template <typename T, typename TSame> bool SameOutcome(const TOutcome<T>& A, const TOutcome<T>& B, TSame&& Same)
{
	if (A.bThrew != B.bThrew)
	{
		return false;
	}
	if (A.bThrew)
	{
		return strcmp(A.Message, B.Message) == 0;
	}
	return Same(A.Value, B.Value);
}

/**
 * 2Dの型と、乱数の配置・問い合わせの作り方。
 */
struct F2D
{
	using FWorld = Dxf::FPhysicsWorld2D;
	using FBody = Dxf::FBodyId2D;
	using FCollider = Dxf::FColliderId2D;
	using FVector = Toolbox::FVector2;
	using FBall = Toolbox::FCircle2D;
	using FBodyDescription = Dxf::FBodyDescription2D;
	using FColliderDescription = Dxf::FColliderDescription2D;
	/**
	 * 次元の名前。
	 */
	static constexpr const char* Name = "2D";
	/**
	 * 範囲内の乱数の点。
	 * @param Random 乱数。
	 * @param Extent 各軸の半幅。
	 */
	static FVector Point(FRandom& Random, Toolbox::f32 Extent)
	{
		return {Random.Range(-Extent, Extent), Random.Range(-Extent, Extent)};
	}
	/**
	 * 点を返す（Zは無視する）。
	 * @param X X。
	 * @param Y Y。
	 * @param Z 使わない。
	 */
	static FVector At(Toolbox::f32 X, Toolbox::f32 Y, Toolbox::f32 Z)
	{
		(void)Z;
		return {X, Y};
	}
	/**
	 * 基準点からずらした点（f32で足す）。
	 * @param Origin 基準点。
	 * @param Delta ずらす量。
	 */
	static FVector Offset(FVector Origin, FVector Delta)
	{
		return {Origin.X + Delta.X, Origin.Y + Delta.Y};
	}
	/**
	 * 倍率を掛けた点。
	 * @param Value 点。
	 * @param Factor 倍率。
	 */
	static FVector Scale(FVector Value, Toolbox::f32 Factor)
	{
		return {Value.X * Factor, Value.Y * Factor};
	}
	/**
	 * 乱数の形状（円、回転矩形、半幅0の辺・点を含む）。
	 * @param Random 乱数。
	 * @param Offset 重心からの位置の範囲。
	 */
	static FColliderDescription RandomShape(FRandom& Random, Toolbox::f32 Offset)
	{
		FColliderDescription Description;
		const FVector Center = Point(Random, Offset);
		if (Random.Chance(40))
		{
			Description.Shape = Toolbox::FCircle2D{Center, Random.Chance(10) ? 0.0f : Random.Range(0.05f, 1.5f)};
		}
		else
		{
			const Toolbox::f32 X = Random.Chance(10) ? 0.0f : Random.Range(0.05f, 3.0f);
			const Toolbox::f32 Y = Random.Chance(10) ? 0.0f : Random.Range(0.05f, 3.0f);
			Description.Shape =
			    Toolbox::FOrientedBox2D{Center, {X, Y}, Random.Chance(30) ? 0.0f : Random.Range(-3.2f, 3.2f)};
		}
		return Description;
	}
	/**
	 * 乱数の姿勢へ置く。
	 * @param World World。
	 * @param Body 対象。
	 * @param Random 乱数。
	 * @param Extent 位置の範囲。
	 */
	static void RandomTransform(FWorld& World, FBody Body, FRandom& Random, Toolbox::f32 Extent)
	{
		World.SetBodyTransform(Body, Point(Random, Extent), Random.Range(-3.2f, 3.2f));
	}
	/**
	 * 生成時の姿勢を乱数で決める（移動による索引の予測の広がりを含まない配置用）。
	 * @param Description 生成の設定。
	 * @param Random 乱数。
	 * @param Extent 位置の範囲。
	 */
	static void RandomPose(FBodyDescription& Description, FRandom& Random, Toolbox::f32 Extent)
	{
		Description.Position = Point(Random, Extent);
		Description.Angle = Random.Range(-3.2f, 3.2f);
	}
	/**
	 * 軸に平行な箱（3Dの奥行きの半幅はHalfY）。
	 * @param Center 重心からの中心。
	 * @param HalfX X方向の半幅。
	 * @param HalfY Y方向の半幅。
	 */
	static FColliderDescription Box(FVector Center, Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOrientedBox2D{Center, {HalfX, HalfY}, 0};
		return Description;
	}
	/**
	 * 円。
	 * @param Center 重心からの中心。
	 * @param Radius 半径。
	 */
	static FColliderDescription Ball(FVector Center, Toolbox::f32 Radius)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FCircle2D{Center, Radius};
		return Description;
	}
	/**
	 * 上面y=0の床（X方向の半幅Half）。
	 * @param Half 半幅。
	 */
	static FColliderDescription Floor(Toolbox::f32 Half)
	{
		return Box({0, -1}, Half, 1);
	}
	/**
	 * Z軸回りに90度回した姿勢へ置く。
	 * @param World World。
	 * @param Body 対象。
	 * @param Position 位置。
	 */
	static void PlaceTurned(FWorld& World, FBody Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, 1.5707963267948966f);
	}
	/**
	 * 姿勢を置く（角度0）。
	 * @param World World。
	 * @param Body 対象。
	 * @param Position 位置。
	 */
	static void Place(FWorld& World, FBody Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, 0);
	}
	/**
	 * 位置を返す。
	 * @param World World。
	 * @param Body 対象。
	 */
	static FVector Position(const FWorld& World, FBody Body)
	{
		return World.GetPosition(Body);
	}
};
/**
 * 3Dの型と、乱数の配置・問い合わせの作り方。
 */
struct F3D
{
	using FWorld = Dxf::FPhysicsWorld3D;
	using FBody = Dxf::FBodyId3D;
	using FCollider = Dxf::FColliderId3D;
	using FVector = Toolbox::FVector3;
	using FBall = Toolbox::FSphere;
	using FBodyDescription = Dxf::FBodyDescription3D;
	using FColliderDescription = Dxf::FColliderDescription3D;
	/**
	 * 次元の名前。
	 */
	static constexpr const char* Name = "3D";
	static FVector Point(FRandom& Random, Toolbox::f32 Extent)
	{
		return {Random.Range(-Extent, Extent), Random.Range(-Extent, Extent), Random.Range(-Extent, Extent)};
	}
	static FVector At(Toolbox::f32 X, Toolbox::f32 Y, Toolbox::f32 Z)
	{
		return {X, Y, Z};
	}
	static FVector Offset(FVector Origin, FVector Delta)
	{
		return {Origin.X + Delta.X, Origin.Y + Delta.Y, Origin.Z + Delta.Z};
	}
	static FVector Scale(FVector Value, Toolbox::f32 Factor)
	{
		return {Value.X * Factor, Value.Y * Factor, Value.Z * Factor};
	}
	/**
	 * 乱数の回転（正規化した四元数）。
	 * @param Random 乱数。
	 */
	static Toolbox::FQuaternion Rotation(FRandom& Random)
	{
		return Toolbox::FQuaternion::FromAxisAngle(
		    Toolbox::FVector3{Random.Range(-1, 1), Random.Range(-1, 1), Random.Range(0.1f, 1)},
		    Random.Range(-3.2f, 3.2f));
	}
	static FColliderDescription RandomShape(FRandom& Random, Toolbox::f32 Offset)
	{
		FColliderDescription Description;
		const FVector Center = Point(Random, Offset);
		if (Random.Chance(40))
		{
			Description.Shape = Toolbox::FSphere{Center, Random.Chance(10) ? 0.0f : Random.Range(0.05f, 1.5f)};
		}
		else
		{
			Toolbox::FOBB Box{Center,
			                  {Random.Chance(10) ? 0.0f : Random.Range(0.05f, 3.0f),
			                   Random.Chance(10) ? 0.0f : Random.Range(0.05f, 3.0f),
			                   Random.Chance(10) ? 0.0f : Random.Range(0.05f, 3.0f)}};
			if (Random.Chance(70))
			{
				// 実際の軸を回した直交単位軸（f32の丸めを含む）。
				const Toolbox::FQuaternion Q = Rotation(Random);
				for (Toolbox::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Box.Axes[Axis] = Q.Rotate(Box.Axes[Axis]);
				}
			}
			Description.Shape = Box;
		}
		return Description;
	}
	static void RandomTransform(FWorld& World, FBody Body, FRandom& Random, Toolbox::f32 Extent)
	{
		World.SetBodyTransform(Body, Point(Random, Extent), Rotation(Random));
	}
	static void RandomPose(FBodyDescription& Description, FRandom& Random, Toolbox::f32 Extent)
	{
		Description.Position = Point(Random, Extent);
		Description.Orientation = Rotation(Random);
	}
	static FColliderDescription Box(FVector Center, Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOBB{Center, {HalfX, HalfY, HalfY}};
		return Description;
	}
	static FColliderDescription Ball(FVector Center, Toolbox::f32 Radius)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FSphere{Center, Radius};
		return Description;
	}
	static FColliderDescription Floor(Toolbox::f32 Half)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOBB{{0, -1, 0}, {Half, 1, Half}};
		return Description;
	}
	static void PlaceTurned(FWorld& World, FBody Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, Toolbox::FQuaternion::FromAxisAngle({0, 0, 1}, 1.5707963267948966f));
	}
	static void Place(FWorld& World, FBody Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, Toolbox::FQuaternion{});
	}
	static FVector Position(const FWorld& World, FBody Body)
	{
		return World.GetPosition(Body);
	}
};

/**
 * 同じWorldで、索引と総当たりの経路の結果（例外のメッセージを含む）を乱数の問い合わせで比べる。
 * 一致しなければ、種・手順・問い合わせの番号と種別を含むFExceptionを投げる。
 * @param World 対象（比べた後は索引の経路へ戻す）。
 * @param Random 乱数。
 * @param Bodies 自己除外に使うBody（無効なIDを含んでよい）。
 * @param Queries 比べる問い合わせの数。
 * @param Extent 問い合わせの位置の範囲。
 * @param Seed 失敗時に出す種。
 * @param Step 失敗時に出す手順の番号。
 * @param Origin 問い合わせの位置の中心（大きな共通オフセットの試験用）。
 */
template <typename T>
void CompareQueries(typename T::FWorld& World, FRandom& Random, const Toolbox::TVector<typename T::FBody>& Bodies,
                    Toolbox::int32 Queries, Toolbox::f32 Extent, Toolbox::uint64 Seed, Toolbox::int32 Step,
                    typename T::FVector Origin = {})
{
	using FVector = typename T::FVector;
	for (Toolbox::int32 Query = 0; Query < Queries; ++Query)
	{
		// 自己除外（なし／乱数のBody）とカテゴリ（全ビット／乱数のビット）。
		Toolbox::TOptional<typename T::FBody> Excluded;
		if (!Bodies.IsEmpty() && Random.Chance(50))
		{
			Excluded = Bodies[static_cast<Toolbox::size_t>(Random.Below(static_cast<Toolbox::int32>(Bodies.Size())))];
		}
		Dxf::FWorldQueryFilter Filter;
		if (Random.Chance(40))
		{
			Filter.IncludeCategories = static_cast<Toolbox::uint32>(Random.Below(8));
		}
		const FVector A = T::Offset(Origin, T::Point(Random, Extent));
		const FVector B = Random.Chance(10) ? A : T::Offset(Origin, T::Point(Random, Extent));
		const Toolbox::f32 Radius = Random.Chance(15) ? 0.0f : Random.Range(0.01f, 2.0f);
		const Toolbox::int32 Kind = Random.Below(5);
		bool bSame = true;
		{
			auto Run = [&](bool bReference, auto&& Call)
			{
				World.SetQueryIndexEnabled_Internal(!bReference);
				auto Result = Call();
				World.SetQueryIndexEnabled_Internal(true);
				return Result;
			};
			if (Kind == 0)
			{
				auto Call = [&]
				{
					return Capture<decltype(World.RaycastClosest(A, B))>(
					    [&]
					    {
						    return World.RaycastClosest(A, B, Excluded, Filter);
					    });
				};
				bSame = SameOutcome(Run(false, Call), Run(true, Call),
				                    [](const auto& X, const auto& Y)
				                    {
					                    return SameSegment(X, Y);
				                    });
			}
			else if (Kind == 1 || Kind == 2)
			{
				const typename T::FBall Shape{A, Radius};
				auto Call = [&]
				{
					return Capture<decltype(World.SweepClosest(Shape, B))>(
					    [&]
					    {
						    return Kind == 1 ? World.SweepClosest(Shape, B, Excluded, Filter)
						                     : World.SweepClosestIgnoringInitialContacts(Shape, B, Excluded, Filter);
					    });
				};
				bSame = SameOutcome(Run(false, Call), Run(true, Call),
				                    [](const auto& X, const auto& Y)
				                    {
					                    return SameSweep(X, Y);
				                    });
			}
			else if (Kind == 3)
			{
				const typename T::FBall Shape{A, Radius};
				const Toolbox::f64 Margin = Random.Chance(20) ? 0.0 : Random.Range(0.0f, 3.0f);
				auto Call = [&]
				{
					return Capture<decltype(World.QueryContacts(Shape, Margin))>(
					    [&]
					    {
						    return World.QueryContacts(Shape, Margin, Excluded, Filter);
					    });
				};
				bSame = SameOutcome(Run(false, Call), Run(true, Call),
				                    [](const auto& X, const auto& Y)
				                    {
					                    return SameContacts(X, Y);
				                    });
			}
			else
			{
				const typename T::FBall Shape{A, Radius * 2};
				auto Call = [&]
				{
					return Capture<decltype(World.OverlapAll(Shape))>(
					    [&]
					    {
						    return World.OverlapAll(Shape, Excluded, Filter);
					    });
				};
				bSame = SameOutcome(Run(false, Call), Run(true, Call),
				                    [](const auto& X, const auto& Y)
				                    {
					                    return SameIds(X, Y);
				                    });
			}
		}
		if (!bSame)
		{
			char Message[256];
			snprintf(Message, sizeof(Message), "%s index/reference mismatch: seed=%llu step=%d query=%d kind=%d",
			         T::Name, static_cast<unsigned long long>(Seed), Step, Query, Kind);
			throw Toolbox::FException(static_cast<const char*>(Message));
		}
	}
}
} // namespace PhysicsTest::QueryIndexTest
#endif
