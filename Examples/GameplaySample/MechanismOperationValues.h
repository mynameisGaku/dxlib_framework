// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_MECHANISM_OPERATION_VALUES_H
#define DXF_SAMPLE_MECHANISM_OPERATION_VALUES_H
#include "Toolbox/Utility.h"
namespace Dxf::GameplaySample
{
/**
 * 一つの装置の操作意図。描画やUIでJointの状態を直接変更しない。
 */
struct FMechanismOperationValues
{
	/**
	 * -1〜1の目標。角度または距離へ変換する。
	 */
	Toolbox::f64 Target = 0;
	/**
	 * rad/sまたはm/sの上限。
	 */
	Toolbox::f64 Speed = 1;
	/**
	 * N mまたはNの有限上限。
	 */
	Toolbox::f64 Effort = 100;
	/**
	 * 目標制御を進めるか。falseは有限の0速度ブレーキ。
	 */
	bool bRunning = true;
	/**
	 * Limitを使うか。
	 */
	bool bLimited = true;
	/**
	 * 接続の意図。解除だけでBodyは破棄しない。
	 */
	bool bConnected = true;
};
} // namespace Dxf::GameplaySample
#endif
