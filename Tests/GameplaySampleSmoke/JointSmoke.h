// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_JOINT_SMOKE_H
#define DXF_SAMPLE_JOINT_SMOKE_H
#include "SmokeSupport.h"
namespace Dxf::GameplaySmoke
{
/**
 * 既存GameplaySampleの距離拘束を両次元で操作し、補間Anchorの実画素を確認する。
 * @param Backends 既存DxLibの実窓口。
 * @param Root アセットの起点。
 * @param Output 既存試験の記録先。
 */
void RunJointSmoke(FDxLibBackends& Backends, const char* Root, const Toolbox::FPath& Output);
} // namespace Dxf::GameplaySmoke
#endif
