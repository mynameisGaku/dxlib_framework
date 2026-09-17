// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_PARALLEL_CORE_H
#define DXF_PHYSICS_PARALLEL_CORE_H
#include "Toolbox/JobSystem.h"
#include "Toolbox/Vector.h"
namespace Dxf::PhysicsPrivate
{
/**
 * BroadPhaseへ渡す軸平行境界。2DではZを0にしてbUseZをfalseにする。
 */
struct FBroadPhaseBounds
{
	Toolbox::f64 MinX = 0;
	Toolbox::f64 MinY = 0;
	Toolbox::f64 MinZ = 0;
	Toolbox::f64 MaxX = 0;
	Toolbox::f64 MaxY = 0;
	Toolbox::f64 MaxZ = 0;
	bool bUseZ = false;
};
/**
 * BroadPhaseの不変入力。生成中は変更しない。
 */
struct FBroadPhaseEntry
{
	Toolbox::size_t ColliderIndex = 0;
	Toolbox::size_t BodyIndex = 0;
	Toolbox::uint64 BodyGeneration = 0;
	bool bDynamic = false;
	FBroadPhaseBounds Bounds;
};
/**
 * NarrowPhaseへ渡す正準順序のCollider組。
 */
struct FBroadPhasePair
{
	Toolbox::size_t FirstColliderIndex = 0;
	Toolbox::size_t SecondColliderIndex = 0;
	bool operator==(const FBroadPhasePair&) const = default;
};
/**
 * Sweep-and-Pruneで重なり得るCollider組を抽出する。
 * Worker数に依存せずCollider indexの辞書順で結果を返す。
 */
class FBroadPhase
{
public:
	/**
	 * 候補を生成する。Marginは各境界を全方向へ広げる距離。
	 * Jobsがnullptrまたは1レーンなら同期実行する。
	 * @param Entries 有効Colliderの不変入力。
	 * @param Margin ContactSlop等の非負マージン。
	 * @param Jobs 任意のJob System。
	 * @param Out 生成する候補。呼び出し時の内容は破棄する。
	 */
	static void Generate(const Toolbox::TVector<FBroadPhaseEntry>& Entries, Toolbox::f32 Margin,
	                     Toolbox::FJobSystem* Jobs, Toolbox::TVector<FBroadPhasePair>& Out);
};
/**
 * 接触拘束が結ぶBody組。ConstraintIndexはManifoldの安定添字。
 */
struct FIslandEdge
{
	Toolbox::size_t BodyA = 0;
	Toolbox::size_t BodyB = 0;
	Toolbox::size_t ConstraintIndex = 0;
	bool bDynamicA = false;
	bool bDynamicB = false;
};
/**
 * 同じDynamic Bodyを共有する拘束集合。BodyIndicesは昇順。
 */
struct FPhysicsIsland
{
	Toolbox::size_t RootBodyIndex = 0;
	Toolbox::TVector<Toolbox::size_t> BodyIndices;
	Toolbox::TVector<Toolbox::size_t> ConstraintIndices;
};
/**
 * Dynamic同士の接触グラフから独立Solver Islandを決定的に構築する。
 * Static/Kinematicは島同士を連結せず、Constraintだけを所属Dynamic島へ追加する。
 */
class FIslandManager
{
public:
	/**
	 * 接触グラフをIslandへ分解する。
	 * @param BodyCount Body slot総数。EdgeのBody indexはこの範囲内である必要がある。
	 * @param Edges Manifold順の接触辺。
	 * @param Out 構築結果。呼び出し時の内容は破棄する。
	 */
	static void Build(Toolbox::size_t BodyCount, const Toolbox::TVector<FIslandEdge>& Edges,
	                  Toolbox::TVector<FPhysicsIsland>& Out);
};
} // namespace Dxf::PhysicsPrivate
#endif
