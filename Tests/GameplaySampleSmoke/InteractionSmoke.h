// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_SMOKE_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_SMOKE_H
#include "SmokeSupport.h"
namespace Dxf::GameplaySmoke
{
/**
 * 正規の相互作用サンプルを実Applicationで動かし、2D／3Dのゲーム状態・イベント列・描画を照合する。
 * 同じ固定入力と位置設定を1／2画面で実行する。起動中のApplicationがない状態で呼ぶ。
 * @param Backends ネイティブの窓口。
 * @param ProjectRoot アセットの起点。
 * @param Output 実際の画面画像を保存するディレクトリ。
 */
void RunInteractionSmoke(FDxLibBackends& Backends, const char* ProjectRoot, const Toolbox::FPath& Output);
} // namespace Dxf::GameplaySmoke
#endif
