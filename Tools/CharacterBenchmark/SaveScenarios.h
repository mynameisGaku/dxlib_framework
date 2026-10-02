// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_BENCHMARK_SAVE_SCENARIOS_H
#define DXF_BENCHMARK_SAVE_SCENARIOS_H
#include "Toolbox/Utility.h"
namespace Dxf::Benchmark
{
/**
 * 独立した五環境で、チェックポイント採取・実保存・再開・入力の費用を分ける。
 * @param Steps 慣らし後の通常更新と要求なしPollの繰り返し数。
 * @param Dimension 0なら両次元。2または3なら該当次元だけ。
 * @param Instances 0なら16・64・256個体。指定値ならその条件だけ。
 */
void RunSaveSeries(Toolbox::int32 Steps, Toolbox::uint32 Dimension = 0, Toolbox::uint32 Instances = 0);
} // namespace Dxf::Benchmark
#endif
