// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_BENCHMARK_JOINT_SCENARIOS_H
#define DXF_BENCHMARK_JOINT_SCENARIOS_H
#include "Toolbox/Utility.h"
namespace Dxf::Benchmark
{
/**
 * 距離JointのWorld全体とComponentの各境界を別の区間で測る。
 * @param Warmup 慣らしの回数。
 * @param Steps 記録する固定更新の回数。
 */
void RunJointSeries(Toolbox::int32 Warmup, Toolbox::int32 Steps);
} // namespace Dxf::Benchmark
#endif
