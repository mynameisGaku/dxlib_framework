// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_BENCHMARK_SCALING_SCENARIOS_H
#define DXF_CHARACTER_BENCHMARK_SCALING_SCENARIOS_H
#include "ScenarioRunner.h"
namespace Dxf::Benchmark
{
/**
 * 規模を変える系列を2D／3Dで、索引と総当たりの両方の経路で計って出力する。
 * 各系列のキャラクターは行ごとに+X方向へ歩き続け、低い段差（0.2）と30度の坂を越え、行の端で始点へ戻る。
 * - moving: 近くの障害物の数を変える（壁で止まらず位置が変わり続ける）
 * - local: 近くの障害物は固定し、遠方（X≧10000）の障害物だけを増やす
 * - dynamic: キャラクター以外のKinematicのBodyを毎回SetBodyTransformで動かす数を変える（索引の更新の費用）
 * - dense: キャラクターの周り2m四方に重なり合う箱を置く数を変える（候補を減らせない場合）
 * - churn: 計測前にColliderの削除と追加を繰り返した回数を変える（スロット再利用後の木の品質）
 * - floor: 床の大きさを変える（大きな包囲箱一つが絞り込みを崩さないか）
 * @param Warmup 慣らしの固定更新の数。
 * @param Measured 1回の繰り返しの固定更新の数。
 */
void RunScalingSeries(Toolbox::int32 Warmup, Toolbox::int32 Measured);
} // namespace Dxf::Benchmark
#endif
