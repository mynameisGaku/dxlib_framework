// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_MECHANISM_SMOKE_H
#define DXF_SAMPLE_MECHANISM_SMOKE_H
#include "SmokeSupport.h"
namespace Dxf::GameplaySmoke
{
/**
 * 同じゲーム装置の回転・直動・固定を両次元で操作し、指定ビューの実画素を照合する。
 * Backendsは既存Native、Rootは資源起点、Outputは試行専用の画像保存先。
 */
void RunMechanismSmoke(FDxLibBackends& Backends, const char* Root, const Toolbox::FPath& Output);
} // namespace Dxf::GameplaySmoke
#endif
