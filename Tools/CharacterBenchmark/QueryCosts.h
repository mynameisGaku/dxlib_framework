// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_BENCHMARK_QUERY_COSTS_H
#define DXF_CHARACTER_BENCHMARK_QUERY_COSTS_H
#include "BenchmarkSupport.h"
namespace Dxf::Benchmark
{
/**
 * 計る問い合わせの種別の数（接触・スイープ・初期接触を除くスイープ・線分・重なり）。
 */
constexpr Toolbox::int32 QueryKinds = 5;
/**
 * 種別の表示名。
 * @param Kind 種別の番号。
 */
const char* QueryKindName(Toolbox::int32 Kind) noexcept;

/**
 * 問い合わせ種別ごとの1回あたりの費用。
 */
struct FQueryCostTable
{
	/**
	 * 計測したか（場面が対応していなければfalse）。
	 */
	bool bMeasured = false;
	/**
	 * 索引の経路の1回あたりの時間（ナノ秒、5回の中央値）。
	 */
	Toolbox::f64 IndexedNs[QueryKinds]{};
	/**
	 * 総当たりの経路の1回あたりの時間（ナノ秒、5回の中央値）。
	 */
	Toolbox::f64 ReferenceNs[QueryKinds]{};
	/**
	 * 索引の経路の1回あたりの詳細判定の数。
	 */
	Toolbox::f64 IndexedNarrow[QueryKinds]{};
	/**
	 * 総当たりの経路の1回あたりの詳細判定の数。
	 */
	Toolbox::f64 ReferenceNarrow[QueryKinds]{};
	/**
	 * 同じ入力で二つの経路の結果（ID・割合・中心・法線・接触集合と順序）が一致したか。
	 */
	bool bSameResults[QueryKinds]{};
};

/**
 * 2DのWorldで、指定した中心の列について種別ごとの費用を計る（索引と総当たりを切り替えて同じ入力で比べる）。
 * 索引のないビルドでは総当たりの時間だけを計る。
 * @param World 対象のWorld（計測中だけ経路と集計を切り替え、最後に索引の経路へ戻す）。
 * @param Centers 問い合わせの中心（キャラクターの位置）。
 * @param Bodies 同じ順の自己Body（除外）。
 * @param Count 中心の数。
 * @param Calls 1回の計測の呼出し数。
 */
FQueryCostTable MeasureQueryCosts(FPhysicsWorld2D& World, const Toolbox::FVector2* Centers, const FBodyId2D* Bodies,
                                  Toolbox::int32 Count, Toolbox::int32 Calls);
/**
 * 3D版。
 * @param World 対象のWorld。
 * @param Centers 問い合わせの中心。
 * @param Bodies 同じ順の自己Body。
 * @param Count 中心の数。
 * @param Calls 1回の計測の呼出し数。
 */
FQueryCostTable MeasureQueryCosts(FPhysicsWorld3D& World, const Toolbox::FVector3* Centers, const FBodyId3D* Bodies,
                                  Toolbox::int32 Count, Toolbox::int32 Calls);
} // namespace Dxf::Benchmark
#endif
