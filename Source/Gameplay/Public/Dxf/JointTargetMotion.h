// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_JOINTTARGETMOTION_H
#define DXF_JOINTTARGETMOTION_H
#include "Dxf/AngularJointTargetSettings.h"
#include "Dxf/AngularJointTargetCommand.h"
#include "Dxf/LinearJointTargetSettings.h"
#include "Dxf/LinearJointTargetCommand.h"
#include "Dxf/AngularJointLimits.h"
#include "Dxf/LinearJointLimits.h"
namespace Dxf
{
/**
 * 現在観察から速度Motor要求を作る。非有限・不正範囲・Limit外の目標・dt<=0は拒否する。
 * @param Settings 目標・減速・許容・有限駆動力。
 * @param Coordinate 成功した物理観察の角度radまたは移動m。
 * @param Speed 同じ観察のrad/sまたはm/s。
 * @param Limits この要求に属する有効区間。
 * @param Seconds 次の固定Step秒数。
 */
FAngularJointTargetCommand ComputeJointTargetDrive(const FAngularJointTargetSettings& Settings, Toolbox::f64 Coordinate, Toolbox::f64 Speed, const FAngularJointLimits& Limits, Toolbox::f64 Seconds);
/**
 * 現在観察から速度Motor要求を作る。非有限・不正範囲・Limit外の目標・dt<=0は拒否する。
 * @param Settings 目標・減速・許容・有限駆動力。
 * @param Coordinate 成功した物理観察の角度radまたは移動m。
 * @param Speed 同じ観察のrad/sまたはm/s。
 * @param Limits この要求に属する有効区間。
 * @param Seconds 次の固定Step秒数。
 */
FLinearJointTargetCommand ComputeJointTargetDrive(const FLinearJointTargetSettings& Settings, Toolbox::f64 Coordinate, Toolbox::f64 Speed, const FLinearJointLimits& Limits, Toolbox::f64 Seconds);
} // namespace Dxf
#endif
