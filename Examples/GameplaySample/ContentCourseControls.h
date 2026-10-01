// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_CONTENT_COURSE_CONTROLS_H
#define DXF_SAMPLE_CONTENT_COURSE_CONTROLS_H
#include "Toolbox/Utility.h"
namespace Dxf::GameplaySample
{
/**
 * UIが保持する次の操作。定義へのoverrideは新生成用、Targetは生存個体へのゲーム操作。
 */
struct FContentCourseControls
{
	/**
	 * 操作する扉。0がdoorA、1がdoorB。
	 */
	Toolbox::uint32 Selection = 0;
	/**
	 * 扉の目標座標m。
	 */
	Toolbox::f64 Target = 1;
	/**
	 * 最大速度m/s。
	 */
	Toolbox::f64 Speed = 0.8;
	/**
	 * 有限Motorの最大力N。
	 */
	Toolbox::f64 Effort = 15;
	/**
	 * 目標へ駆動するか。falseは有限の速度0ブレーキ。
	 */
	bool bRunning = true;
	/**
	 * Jointの接続を保持するか。
	 */
	bool bConnected = true;
	/**
	 * 明示操作されるまでデータのMotor条件を維持する。
	 */
	bool bOperate = false;
	/**
	 * 次の生成だけへ渡す公開値。生存中の定義は変更しない。
	 */
	Toolbox::f64 NextSpeed = 0.8;
	Toolbox::f64 NextTravel = 2;
	Toolbox::f64 NextEffort = 15;
	bool bNextWarmColor = false;
	/**
	 * 次の通常更新から一回だけ生成する。
	 */
	bool bSpawn = false;
	/**
	 * 選択した個体の境界破棄を一回要求する。
	 */
	bool bDestroy = false;
	/**
	 * 有効なSceneのゲームイベントからcueを一回再生する。
	 */
	bool bCue = false;
	/**
	 * 左右の表示だけを切り替える。
	 */
	bool bSplit = false;
};
} // namespace Dxf::GameplaySample
#endif
