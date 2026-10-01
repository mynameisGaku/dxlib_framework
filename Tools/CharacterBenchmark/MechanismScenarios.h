// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MECHANISM_BENCHMARK_SCENARIOS_H
#define DXF_MECHANISM_BENCHMARK_SCENARIOS_H
#include "Toolbox/Utility.h"
namespace Dxf::Benchmark
{
/**
 * 新Jointと混在構成の費用を既存の時間・確保計測で記録する。
 * @param Warmup 初回以後の慣らしStep数。
 * @param Steps 慣らし後に集計するStep数。
 */
void RunMechanismSeries(Toolbox::int32 Warmup, Toolbox::int32 Steps);
} // namespace Dxf::Benchmark
#endif
