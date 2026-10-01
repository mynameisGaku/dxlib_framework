// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_ANGULARJOINTTARGETSETTINGS_H
#define DXF_ANGULARJOINTTARGETSETTINGS_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 目標から速度指示を求める設定。BodyのPoseや速度を直接変更しない。
 */
struct FAngularJointTargetSettings
{
	/**
	 * 目標座標。角度は折り返しを跨がない主値、並進はm。
	 */
	Toolbox::f64 TargetAngle = 0;
	/**
	 * 目標へ向かう最大速度。rad/sまたはm/s、0以上。
	 */
	Toolbox::f64 MaxAngularSpeed = 1;
	/**
	 * 減速を始める距離。radまたはm、正の有限値。
	 */
	Toolbox::f64 SlowdownAngle = 0.2;
	/**
	 * 位置の許容誤差。radまたはm、0以上。
	 */
	Toolbox::f64 AngleTolerance = 0.001;
	/**
	 * 停止と判定する速度。rad/sまたはm/s、0以上。
	 */
	Toolbox::f64 AngularSpeedTolerance = 0.001;
	/**
	 * 有限Motorの最大TorqueまたはForce。0以上。
	 */
	Toolbox::f64 MaxTorque = 10;
};
} // namespace Dxf
#endif
