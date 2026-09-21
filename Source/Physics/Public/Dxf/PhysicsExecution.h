// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_EXECUTION_H
#define DXF_PHYSICS_EXECUTION_H
#include "Toolbox/JobSystem.h"
namespace Dxf
{
/**
 * Physics内部の並列実行方法。World公開API自体は呼び出しスレッドを限定する。
 */
struct FPhysicsExecutionSettings
{
	/**
	 * Step中だけ借用するJob System。nullptrなら完全な同期実行。
	 * 呼び出し側はWorldより先にJob Systemを破棄してはならない。
	 */
	Toolbox::FJobSystem* JobSystem = nullptr;
	/**
	 * 独立Bodyの速度・位置積分をJobへ分割するか。
	 */
	bool bParallelIntegration = true;
	/**
	 * Sweep-and-Pruneの候補走査をJobへ分割するか。
	 */
	bool bParallelBroadPhase = true;
	/**
	 * BroadPhase候補ごとの接触生成をJobへ分割するか。
	 */
	bool bParallelNarrowPhase = true;
	/**
	 * Dynamic Bodyを共有しないIslandをJobへ分割して拘束を解くか。
	 */
	bool bParallelIslandSolver = true;
};
/**
 * 直近Stepで利用した並列物理の診断値。
 */
struct FPhysicsExecutionDiagnostics
{
	/**
	 * Job Systemが報告した実行レーン数。同期実行は1。
	 */
	Toolbox::uint32 ExecutionThreadCount = 1;
	/**
	 * BroadPhaseがNarrowPhaseへ渡した候補組数。
	 */
	Toolbox::uint64 CandidatePairCount = 0;
	/**
	 * NarrowPhaseが生成した接触Manifold数。
	 */
	Toolbox::uint64 ManifoldCount = 0;
	/**
	 * 現在接触から構築したDynamic Island数。
	 */
	Toolbox::uint64 IslandCount = 0;
	/**
	 * Job経路で解決したIsland数。直列経路では増えない。
	 */
	Toolbox::uint64 SolverIslandCount = 0;
};
} // namespace Dxf
#endif
