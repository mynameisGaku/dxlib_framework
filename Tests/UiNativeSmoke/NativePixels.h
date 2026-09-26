// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_UI_NATIVE_PIXELS_H
#define DXF_TEST_UI_NATIVE_PIXELS_H
#include "Dxf/UiRoot.h"
#include "Toolbox/Platform.h"
namespace Dxf::UiSmoke
{
/**
 * 文字・クリップ検査の小さな部品をルートへ追加する。
 */
void InstallPixelFixture(FUiRoot& Root);
/**
 * Present前の画面を一度読み戻し、既知色と日本語の描画を検査して保存する。
 */
void VerifyPixels(const Toolbox::FPath& Path);
} // namespace Dxf::UiSmoke
#endif
