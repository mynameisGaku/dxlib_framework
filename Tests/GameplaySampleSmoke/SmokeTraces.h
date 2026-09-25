// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_SMOKE_TRACES_H
#define DXF_GAMEPLAY_SAMPLE_SMOKE_TRACES_H
#include "SmokeSupport.h"
namespace Dxf::GameplaySmoke
{
/**
 * 同じ固定入力（歩行キャラクターの生成・歩行・ジャンプ・向きの反転）を、2Dのシーンで3回実行し、
 * 1画面／2画面、索引あり／総当たりの参照経路で、フレームごとのプレイヤーと歩行キャラクターの位置・速度・固定更新の数・
 * ジャンプと着地のイベント・アニメーション時間がビット単位で一致することを確かめる。
 * 各回は新しいApplicationで起動し、同じフレーム時刻の列で進める（アニメーション時間はフレームの時刻差の和で、
 * 時刻の絶対値によって丸めが変わるため、同じ時刻の列でなければビット単位では比べられない）。
 * 他のApplicationが動いていないときに呼ぶこと。
 * @param Backends ネイティブの窓口。
 * @param ProjectRoot アセットの起点。
 */
void RunTraceComparisons2D(FDxLibBackends& Backends, const char* ProjectRoot);
/**
 * 3D版（2Dのシーンで起動してから3Dへ切り替えて記録する）。
 * @param Backends ネイティブの窓口。
 * @param ProjectRoot アセットの起点。
 */
void RunTraceComparisons3D(FDxLibBackends& Backends, const char* ProjectRoot);
} // namespace Dxf::GameplaySmoke
#endif
