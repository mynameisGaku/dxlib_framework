// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_LAYOUT_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_LAYOUT_H
#include "SampleLevel.h"
#include "Toolbox/Array.h"
namespace Dxf::GameplaySample
{
/**
 * 接触・Trigger・動く床のサンプルの配置（2D／3D共通。3DはZ=0の平面に同じ配置を奥行き付きで置く）。
 * 左から、触れる箱→取得物→圧力板と扉→横に動く床（下は穴）→チェックポイント→昇降床→上の段→回転する床→ゴール。
 * 穴の下の全体に危険領域があり、落ちると最後のチェックポイントへ戻る。
 */
namespace InteractionLayout
{
/**
 * 静止した地形の数。
 */
constexpr Toolbox::size_t GroundCount = 4;
/**
 * 静止した地形（床A x∈[-4,15]、床B x∈[21,24]、上の段C1 x∈[26.5,32]、上の段C2 x∈[36,44]）。
 */
const Toolbox::TArray<FLevelBox, GroundCount>& GetGround() noexcept;
/**
 * 触れる箱（Dynamic、半幅0.4）の初期位置。
 */
constexpr Toolbox::f32 CrateX = 3;
constexpr Toolbox::f32 CrateHalf = 0.4f;
/**
 * 取得物の数・X・高さ・半径。
 */
constexpr Toolbox::int32 PickupCount = 3;
constexpr Toolbox::f32 PickupX[PickupCount] = {5.5f, 6.5f, 7.5f};
constexpr Toolbox::f32 PickupY = 0.8f;
constexpr Toolbox::f32 PickupRadius = 0.3f;
/**
 * 圧力板（Sensor、床の上）の位置と半幅。
 */
constexpr Toolbox::f32 PlateX = 10;
constexpr Toolbox::f32 PlateY = 0.15f;
constexpr Toolbox::f32 PlateHalfX = 1;
constexpr Toolbox::f32 PlateHalfY = 0.15f;
/**
 * 扉（Kinematic）の位置・半幅・閉じた／開いた高さ・動く速さ。
 */
constexpr Toolbox::f32 DoorX = 13;
constexpr Toolbox::f32 DoorHalfX = 0.3f;
constexpr Toolbox::f32 DoorHalfY = 1.2f;
constexpr Toolbox::f32 DoorClosedY = 1.2f;
constexpr Toolbox::f32 DoorOpenY = 3.8f;
constexpr Toolbox::f64 DoorSpeed = 3;
/**
 * 圧力板が空になってから扉が閉じ始めるまでの秒数。
 */
constexpr Toolbox::f64 DoorHoldSeconds = 2;
/**
 * 横に動く床（x=18±2.4、上面0）。
 */
constexpr Toolbox::f32 PlatformHalfX = 1.2f;
constexpr Toolbox::f32 PlatformHalfY = 0.25f;
/**
 * 横に動く床の位置（経路の時刻の関数）。
 * @param Seconds 固定更新の累計秒数。
 */
Toolbox::FVector2 PlatformAt(Toolbox::f64 Seconds) noexcept;
/**
 * 昇降床（x=25.25、上面0〜3を往復）。
 */
constexpr Toolbox::f32 LiftX = 25.25f;
constexpr Toolbox::f32 LiftHalfX = 1;
/**
 * 昇降床の位置（経路の時刻の関数）。
 * @param Seconds 固定更新の累計秒数。
 */
Toolbox::FVector2 LiftAt(Toolbox::f64 Seconds) noexcept;
/**
 * 回転する床（x=34、上面3。2Dは歩ける範囲で傾き、3Dは鉛直軸回りに回る）。
 */
constexpr Toolbox::f32 TurnX = 34;
constexpr Toolbox::f32 TurnY = 2.75f;
constexpr Toolbox::f32 TurnHalf = 1.5f;
/**
 * 2Dの回転する床の角度（ラジアン）。
 * @param Seconds 固定更新の累計秒数。
 */
Toolbox::f64 TiltAt(Toolbox::f64 Seconds) noexcept;
/**
 * 3Dの回転する床の鉛直軸回りの角度（ラジアン）。
 * @param Seconds 固定更新の累計秒数。
 */
Toolbox::f64 YawAt(Toolbox::f64 Seconds) noexcept;
/**
 * チェックポイントの数と位置（キャラクターの中心）。0番は開始位置。
 */
constexpr Toolbox::int32 CheckpointCount = 3;
constexpr Toolbox::f32 CheckpointX[CheckpointCount] = {0, 22.5f, 40};
constexpr Toolbox::f32 CheckpointY[CheckpointCount] = {0.52f, 0.52f, 3.52f};
/**
 * チェックポイントのSensorの半幅。
 */
constexpr Toolbox::f32 CheckpointHalf = 0.6f;
/**
 * 危険領域（穴の下の全体）の中心と半幅。
 */
constexpr Toolbox::f32 HazardX = 20;
constexpr Toolbox::f32 HazardY = -5;
constexpr Toolbox::f32 HazardHalfX = 30;
constexpr Toolbox::f32 HazardHalfY = 1;
/**
 * 3Dの奥行きの半幅（地形・床）。
 */
constexpr Toolbox::f32 DepthHalf = 2;
} // namespace InteractionLayout
} // namespace Dxf::GameplaySample
#endif
