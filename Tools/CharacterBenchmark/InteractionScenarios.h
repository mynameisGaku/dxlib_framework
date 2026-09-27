// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_BENCHMARK_INTERACTION_SCENARIOS_H
#define DXF_CHARACTER_BENCHMARK_INTERACTION_SCENARIOS_H
#include "Toolbox/Utility.h"

namespace Dxf::Benchmark
{
/**
 * 2D／3Dのイベント・配送・移動床を、Collider数64／512／1024と移動数1／16／64で計測する。
 * 初回と慣らし後を分け、公開APIで取得できないイベント内部の時間・確保はNAと出力する。
 * @param Warmup 初回計測後に行う慣らしの固定更新数。
 * @param Measured 繰り返し1回あたりの固定更新数。繰り返し数・時計・確保追跡は既存benchmarkと共通。
 */
void RunInteractionSeries(Toolbox::int32 Warmup, Toolbox::int32 Measured);
} // namespace Dxf::Benchmark
#endif
