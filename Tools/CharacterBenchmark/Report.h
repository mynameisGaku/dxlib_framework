// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_BENCHMARK_REPORT_H
#define DXF_CHARACTER_BENCHMARK_REPORT_H
#include "ScenarioRunner.h"
namespace Dxf::Benchmark
{
/**
 * 実行条件（慣らし・繰り返し・集計方法・ビルド）を出力する。
 * @param Warmup 慣らしの固定更新の数。
 * @param Measured 1回の繰り返しの固定更新の数。
 */
void PrintConditions(Toolbox::int32 Warmup, Toolbox::int32 Measured);
/**
 * 移動の計測の表の見出しを出力する。
 * @param Title 表の題。
 */
void PrintRunHeader(const char* Title);
/**
 * 移動の計測の1行を出力する。
 * @param Label 条件の名前（次元や系列）。
 * @param Obstacles 障害物の数（系列の変数）。
 * @param Characters キャラクターの数。
 * @param bReference 総当たりの参照経路で計ったか。
 * @param Result 計測結果。
 */
void PrintRunRow(const char* Label, Toolbox::int32 Obstacles, Toolbox::int32 Characters, bool bReference,
                 const FRunResult& Result);
/**
 * 問い合わせ種別ごとの費用の表の見出しを出力する。
 */
void PrintCostHeader();
/**
 * 問い合わせ種別ごとの費用の行を出力する。
 * @param Label 次元。
 * @param Obstacles 障害物の数。
 * @param Characters キャラクターの数。
 * @param Table 費用の表。
 */
void PrintCostRows(const char* Label, Toolbox::int32 Obstacles, Toolbox::int32 Characters,
                   const FQueryCostTable& Table);
} // namespace Dxf::Benchmark
#endif
