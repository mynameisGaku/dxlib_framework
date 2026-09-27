// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_BENCHMARK_CAPSULE_SCENARIOS_H
#define DXF_CHARACTER_BENCHMARK_CAPSULE_SCENARIOS_H
#include "Toolbox/Utility.h"

namespace Dxf::Benchmark
{
/**
 * カプセル・押し合い・SolverのBroadPhaseの系列（CSV）を2D／3Dで計る。時間は合否にしない。
 * 計測条件が壊れた場合（候補の削減がない・容量超過・値が有限でない）だけ例外で止める。
 * @param Warmup 慣らしの固定更新の数。
 * @param Measured 1回の繰り返しの固定更新の数。
 */
void RunCapsuleSeries(Toolbox::int32 Warmup, Toolbox::int32 Measured);
} // namespace Dxf::Benchmark
#endif
