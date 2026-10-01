// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_CONTENT_SMOKE_H
#define DXF_GAMEPLAY_CONTENT_SMOKE_H
#include "SmokeSupport.h"
namespace Dxf::GameplaySmoke
{
/**
 * 同じデータから2D／3Dの実Sceneを生成し、実描画・再開始を確認する。
 * @param Backends 同一プロセスで初期化と終了を繰り返すNative窓口。
 * @param Root 定義と資源のProjectRoot。
 * @param Output 今回専用の画面保存先。
 */
void RunContentSmoke(FDxLibBackends& Backends, const char* Root, const Toolbox::FPath& Output);
} // namespace Dxf::GameplaySmoke
#endif
