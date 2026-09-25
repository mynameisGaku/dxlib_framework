// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_BENCHMARK_SUPPORT_H
#define DXF_CHARACTER_BENCHMARK_SUPPORT_H
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/Utility.h"
namespace Dxf::Benchmark
{
/**
 * 1条件の繰り返しの回数。
 */
constexpr Toolbox::int32 Repetitions = 5;
/**
 * 固定更新の間隔（秒）。
 */
constexpr Toolbox::f64 StepSeconds = 1.0 / 60.0;

/**
 * 繰り返しの値の中央値・最小・最大。
 */
struct FSpread
{
	/**
	 * 中央値。
	 */
	Toolbox::f64 Median = 0;
	/**
	 * 最小。
	 */
	Toolbox::f64 Min = 0;
	/**
	 * 最大。
	 */
	Toolbox::f64 Max = 0;
};
/**
 * 繰り返しの値を集計する（並べ替えたコピーから中央値・最小・最大を取る）。
 * @param Values 繰り返しごとの値。
 */
FSpread Summarize(const Toolbox::f64 (&Values)[Repetitions]);

/**
 * World問い合わせの集計（索引のないビルドではbAvailable=false）。
 */
struct FQueryCounts
{
	/**
	 * 集計を取得できたか。
	 */
	bool bAvailable = false;
	/**
	 * 線分・スイープ・接触・重なりの呼出し回数。
	 */
	Toolbox::uint64 Queries[4]{};
	/**
	 * 同じ順の詳細判定の数。
	 */
	Toolbox::uint64 NarrowTests[4]{};
	/**
	 * 同じ順の候補の数。
	 */
	Toolbox::uint64 Candidates[4]{};
	/**
	 * 同じ順の訪問ノード数。
	 */
	Toolbox::uint64 Nodes[4]{};
	/**
	 * 総当たりで処理した呼出しの数（全種別）。
	 */
	Toolbox::uint64 Fallbacks = 0;
	/**
	 * 索引の境界の確認の回数。
	 */
	Toolbox::uint64 Refreshes = 0;
	/**
	 * 索引の入れ直しの回数。
	 */
	Toolbox::uint64 Reinserts = 0;
	/**
	 * 索引の保持領域のバイト数。
	 */
	Toolbox::uint64 MemoryBytes = 0;
	/**
	 * 索引の木の高さ。
	 */
	Toolbox::uint64 Height = 0;
	/**
	 * 生存しているColliderの数。
	 */
	Toolbox::uint64 Colliders = 0;
};

/**
 * 2点の距離（f64）。
 * @param A 一つ目。
 * @param B 二つ目。
 */
Toolbox::f64 Distance(Toolbox::FVector2 A, Toolbox::FVector2 B);
/**
 * 3D版。
 * @param A 一つ目。
 * @param B 二つ目。
 */
Toolbox::f64 Distance(Toolbox::FVector3 A, Toolbox::FVector3 B);
/**
 * 単調増加の時計（ナノ秒）。
 */
Toolbox::uint64 NowNanoseconds();
/**
 * これまでの確保の累計件数。
 */
Toolbox::uint64 TotalAllocations();
/**
 * このビルドがWorld問い合わせの索引と診断を持つか（基準版のビルドではfalse）。
 */
bool HasQueryIndex() noexcept;
/**
 * 問い合わせの集計を有効／無効にし、累計を0へ戻す（索引のないビルドでは何もしない）。
 * @param World 対象のWorld。
 * @param bEnabled 集計するか。
 */
void BeginQueryCounts(FPhysicsWorld2D& World, bool bEnabled);
/**
 * 3D版。
 * @param World 対象のWorld。
 * @param bEnabled 集計するか。
 */
void BeginQueryCounts(FPhysicsWorld3D& World, bool bEnabled);
/**
 * 問い合わせの集計を読み取る。
 * @param World 対象のWorld。
 */
FQueryCounts ReadQueryCounts(const FPhysicsWorld2D& World);
/**
 * 3D版。
 * @param World 対象のWorld。
 */
FQueryCounts ReadQueryCounts(const FPhysicsWorld3D& World);
/**
 * 問い合わせを総当たりの参照経路にするか（索引のないビルドでは常に総当たりなので何もしない）。
 * @param World 対象のWorld。
 * @param bReference 総当たりにするか。
 */
void SetReferencePath(FPhysicsWorld2D& World, bool bReference);
/**
 * 3D版。
 * @param World 対象のWorld。
 * @param bReference 総当たりにするか。
 */
void SetReferencePath(FPhysicsWorld3D& World, bool bReference);
} // namespace Dxf::Benchmark
#endif
