// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_BENCHMARK_KERNEL_COSTS_H
#define DXF_CHARACTER_BENCHMARK_KERNEL_COSTS_H
#include "Toolbox/Utility.h"
namespace Dxf::Benchmark
{
/**
 * Toolboxの詳細判定の関数（重なり・スイープ・接触・線分）を、2D／3Dで同じ配置（床の上の円／球、坂の箱、円／球同士）
 * に対して直接呼び、1回あたりの時間を出力する（5回の中央値）。World・索引・形状の変換を含まない。
 */
void RunKernelCosts();
} // namespace Dxf::Benchmark
#endif
