// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_BENCHMARK_SCENARIOS_H
#define DXF_CONTENT_BENCHMARK_SCENARIOS_H
#include "Toolbox/Utility.h"
namespace Dxf::Benchmark
{
/**
 * データ準備・生成・固定更新全体・World単独・Content状態読取を分けて測る。
 * @param Steps 慣らし後の繰返し。各条件は独立した新しいSceneを5回作る。
 */
void RunContentSeries(Toolbox::int32 Steps);
} // namespace Dxf::Benchmark
#endif
