// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_PHYSICS_QUERY_CANDIDATES_H
#define DXF_PRIVATE_PHYSICS_QUERY_CANDIDATES_H
#include "QueryIndex.h"
#include "Dxf/WorldQueryDiagnostics.h"
#include "Dxf/WorldQueryFilter.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Vector.h"
namespace Dxf::PhysicsPrivate
{
/**
 * 問い合わせで使う座標の安全な範囲（絶対値2^100）。これを超える問い合わせ・登録があれば総当たりへ回す。
 * この範囲では、有効な形状の詳細判定は座標の組合せによる例外を出さない（f32の途中計算があふれない）。
 */
constexpr Toolbox::f64 QuerySafeLimit = 1267650600228229401496703205376.0;
/**
 * 候補の判定を広げる余白の、座標の規模に対する比（2^-19）。詳細判定のf32の途中計算の丸め（2^-24の数回分）を覆う。
 */
constexpr Toolbox::f64 QueryInflationRatio = 1.9073486328125e-06;

/**
 * 候補の走査に使うWorldの状態（参照だけを持つ）。
 */
template <Toolbox::int32 Dimension, typename TRecord> struct TQuerySource
{
	/**
	 * 問い合わせの索引。
	 */
	const TQueryIndex<Dimension>& Index;
	/**
	 * Colliderの登録（スロット番号で参照する）。
	 */
	const Toolbox::TVector<TRecord>& Colliders;
	/**
	 * 索引を使うか（falseなら総当たり）。
	 */
	bool bIndexEnabled = true;
	/**
	 * 集計を加算するか。
	 */
	bool bDiagnostics = false;
	/**
	 * 集計の累計（診断が有効な間だけ加算する）。
	 */
	FWorldQueryDiagnostics& Totals;
};

/**
 * 候補の判定を広げる余白を返す。問い合わせと索引の座標の規模に比例する。
 * @param Index 問い合わせの索引。
 * @param QueryMaxAbs 問い合わせの座標（半径・Marginを含む）の絶対値の最大。
 */
template <Toolbox::int32 Dimension>
FORCEINLINE Toolbox::f64 QueryInflation_Internal(const TQueryIndex<Dimension>& Index, Toolbox::f64 QueryMaxAbs) noexcept
{
	return (QueryMaxAbs + Index.GetMaxAbs()) * QueryInflationRatio + 8.673617379884035e-19;
}

/**
 * 問い合わせの候補を、索引または総当たり（スロット昇順）で渡す。総当たりで処理したらtrueを返す。
 * カテゴリと自己除外に合わないColliderは渡さない（形状の変換・計算をしない）。索引を使うのは、座標が安全な範囲にあり、
 * 索引に入れられないColliderが今回の対象に含まれない場合だけ（それ以外は従来と同じ全件の昇順走査）。
 * @param Source Worldの状態。
 * @param QueryMaxAbs 問い合わせの座標（半径・Marginを含む）の絶対値の最大。
 * @param Filter 対象にするカテゴリ。
 * @param ExcludedBody 任意の自己Body。
 * @param Test 索引のノードの境界を受け取り、枝に入るかを返す関数。
 * @param Visit
 * Colliderのスロットと、索引の経路か（保存したWorld形状を使えるか）を受け取り、詳細判定と結果の組立てを行う関数。
 * @param Counters 走査の集計。
 * @param NarrowTests 詳細判定の数を加算する先。
 */
template <Toolbox::int32 Dimension, typename TRecord, typename TBodyId, typename TTest, typename TVisit>
bool VisitQueryCandidates_Internal(const TQuerySource<Dimension, TRecord>& Source, Toolbox::f64 QueryMaxAbs,
                                   const FWorldQueryFilter& Filter, const Toolbox::TOptional<TBodyId>& ExcludedBody,
                                   TTest&& Test, TVisit&& Visit, FQueryVisitCounters& Counters,
                                   Toolbox::uint64& NarrowTests)
{
	// カテゴリと自己除外の対象か。
	auto Eligible = [&](const TRecord& Record)
	{
		if ((Record.QueryCategory & Filter.IncludeCategories) == 0)
		{
			return false;
		}
		return !(ExcludedBody && Record.Body == *ExcludedBody);
	};
	// 索引を使えるか。
	bool bIndexed = Source.bIndexEnabled;
	if (!bIndexed)
	{
		Source.Totals.FallbackDisabled += Source.bDiagnostics ? 1 : 0;
	}
	else if (!(QueryMaxAbs <= QuerySafeLimit) || !(Source.Index.GetMaxAbs() <= QuerySafeLimit))
	{
		bIndexed = false;
		Source.Totals.FallbackRange += Source.bDiagnostics ? 1 : 0;
	}
	else if (Source.Index.HasUnindexed() && Source.Index.AnyUnindexed(
	                                            [&](Toolbox::size_t Slot)
	                                            {
		                                            return Eligible(Source.Colliders[Slot]);
	                                            }))
	{
		bIndexed = false;
		Source.Totals.FallbackUnindexed += Source.bDiagnostics ? 1 : 0;
	}
	if (!bIndexed)
	{
		for (Toolbox::size_t Index = 0; Index < Source.Colliders.Size(); ++Index)
		{
			const TRecord& Record = Source.Colliders[Index];
			if (!Record.bAlive)
			{
				continue;
			}
			++Counters.Leaves;
			if (!Eligible(Record))
			{
				continue;
			}
			++NarrowTests;
			Visit(Index, false);
		}
		return true;
	}
	Source.Index.GetTree().Query(
	    Test,
	    [&](Toolbox::uint32 Item)
	    {
		    const Toolbox::size_t Index = Item;
		    if (!Eligible(Source.Colliders[Index]))
		    {
			    return;
		    }
		    ++NarrowTests;
		    Visit(Index, true);
	    },
	    Counters);
	return false;
}

/**
 * 1回の問い合わせの集計を、診断が有効なら累計へ加える。
 * @param bEnabled 診断が有効か。
 * @param Totals 種別の累計。
 * @param Visit 走査の集計。
 * @param NarrowTests 詳細判定の数。
 * @param bFallback 総当たりで処理したか。
 */
FORCEINLINE void CommitQueryCounters_Internal(bool bEnabled, FWorldQueryTypeCounters& Totals,
                                              const FQueryVisitCounters& Visit, Toolbox::uint64 NarrowTests,
                                              bool bFallback) noexcept
{
	if (!bEnabled)
	{
		return;
	}
	++Totals.Queries;
	Totals.FallbackQueries += bFallback ? 1 : 0;
	Totals.NodesVisited += Visit.Nodes;
	Totals.Candidates += Visit.Leaves;
	Totals.NarrowTests += NarrowTests;
}
} // namespace Dxf::PhysicsPrivate
#endif
