// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_UI_DISPLAY_SCENARIO_H
#define DXF_TEST_UI_DISPLAY_SCENARIO_H
#include "Toolbox/Platform.h"
namespace Dxf::UiSmoke
{
/**
 * 表示先（全画面・左右のViewport・2Dワールド・3Dの不透明／透明のパネル）を実DxLibで描き、
 * パネルを描かない基準のフレームと比べて画素を検査し、既知の画素のクリックの届け先を確かめる。
 * 失敗はToolbox::FExceptionで知らせる。
 * @param ProjectRoot プロジェクトの根。
 * @param Out 画像の保存先。
 */
void RunDisplayScenario(const char* ProjectRoot, const Toolbox::FPath& Out);
} // namespace Dxf::UiSmoke
#endif
