// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_UI_WINDOW_SCENARIO_H
#define DXF_TEST_UI_WINDOW_SCENARIO_H
#include "Toolbox/Platform.h"
namespace Dxf::UiSmoke
{
/**
 * 実ウィンドウ（自アプリのウィンドウだけ）を拡縮・最小化・復帰し、描画先の寸法・UIの画素・入力・資源を確かめる。
 * 利用者の実マウスの拡縮ではなく、自アプリのウィンドウへのAPI呼出しによる自動操作。失敗は例外で知らせる。
 * @param ProjectRoot プロジェクトの根。
 * @param Out 画像の保存先。
 */
void RunWindowScenario(const char* ProjectRoot, const Toolbox::FPath& Out);
} // namespace Dxf::UiSmoke
#endif
