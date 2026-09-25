// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_SMOKE_ACCEPTANCE_H
#define DXF_GAMEPLAY_SAMPLE_SMOKE_ACCEPTANCE_H
#include "SmokeSupport.h"
namespace Dxf::GameplaySmoke
{
/**
 * 2Dのサンプルの受け入れ（現在のシーンが開始直後の2Dであること）: 歩行・停止・段差・30度の坂・急坂の手前での停止・
 * 重ならないこと・画素、リセット、ジャンプと着地、天井への衝突、初期めり込みからの回復、壁と床の角での停止、
 * 一時停止と再開、歩行キャラクターの途中の生成と破棄（Bodyの解放）。
 * @param App 実Application。
 * @param Output 画面の保存先。
 */
void RunAcceptance2D(FSmokeApp& App, const Toolbox::FPath& Output);
/**
 * 3Dのサンプルの受け入れ（現在のシーンが開始直後の3Dであること）。2Dと同じ項目に加え、二つの壁の稜線での停止。
 * @param App 実Application。
 * @param Output 画面の保存先。
 */
void RunAcceptance3D(FSmokeApp& App, const Toolbox::FPath& Output);
} // namespace Dxf::GameplaySmoke
#endif
