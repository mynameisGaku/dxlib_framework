// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_BENCHMARK_LEGACY_SCENARIOS_H
#define DXF_CHARACTER_BENCHMARK_LEGACY_SCENARIOS_H
#include "ScenarioRunner.h"
namespace Dxf::Benchmark
{
/**
 * 前回（2026-09-25）の2Dの条件と同じ配置・入力列（床と障害物は既定のDynamicのBodyで、重力で落ち続ける）。キャラクターごとに別の床（高さ10ずつ）を置き、
 * 障害物（低い段差・高い壁・30度の坂を順に）を床へ順に配る。多くの個体は障害物の前で止まる。
 * @param Obstacles 障害物の数。
 * @param Characters キャラクターの数（64以下）。
 */
Toolbox::TUniquePtr<IScenario> MakeLegacy2D(Toolbox::int32 Obstacles, Toolbox::int32 Characters);
/**
 * 前回の3Dの条件と同じ配置・入力列。一つの床に障害物を格子状に置き、キャラクターは格子の隅から歩く。
 * @param Obstacles 障害物の数。
 * @param Characters キャラクターの数（64以下）。
 */
Toolbox::TUniquePtr<IScenario> MakeLegacy3D(Toolbox::int32 Obstacles, Toolbox::int32 Characters);
/**
 * 前回の2Dの条件と同じ配置・入力列で、床と障害物のBodyをStaticにしたもの（前回は既定のDynamicで、落ち続けていた）。
 * @param Obstacles 障害物の数。
 * @param Characters キャラクターの数（64以下）。
 */
Toolbox::TUniquePtr<IScenario> MakeLegacyStatic2D(Toolbox::int32 Obstacles, Toolbox::int32 Characters);
/**
 * 前回の3Dの条件と同じ配置・入力列で、床と障害物のBodyをStaticにしたもの。
 * @param Obstacles 障害物の数。
 * @param Characters キャラクターの数（64以下）。
 */
Toolbox::TUniquePtr<IScenario> MakeLegacyStatic3D(Toolbox::int32 Obstacles, Toolbox::int32 Characters);
/**
 * 決まった入力の向き（個体ごとにずらし、時間とともに回す）。前回と同じ式。
 * @param Character 個体の番号。
 * @param StepIndex 固定更新の通し番号。
 */
Toolbox::f32 LegacyAngle(Toolbox::int32 Character, Toolbox::int64 StepIndex);
} // namespace Dxf::Benchmark
#endif
