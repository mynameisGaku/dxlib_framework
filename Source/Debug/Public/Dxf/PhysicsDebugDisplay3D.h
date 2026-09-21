// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_DEBUG_DISPLAY_3D_H
#define DXF_PHYSICS_DEBUG_DISPLAY_3D_H
#include "Dxf/PhysicsDebugSnapshot3D.h"
#include "Dxf/Render3DContext.h"
namespace Dxf
{
/**
 * ビューごとの物理観察設定。Worldの状態へ書き戻さない。
 */
struct FPhysicsDebugDisplaySettings3D
{
	/**
	 * Colliderのワイヤー表示。
	 */
	bool bColliders = true;
	/**
	 * 重心から延びる速度線。
	 */
	bool bVelocities = true;
	/**
	 * 重心の小さな十字。
	 */
	bool bCenters = true;
	/**
	 * 速度線へ換算する秒数。0〜10。
	 */
	Toolbox::f32 VelocitySeconds = 0.25f;
	/**
	 * trueは面の裏でも描く。falseは深度検査だけ行う。
	 */
	bool bAlwaysVisible = false;
};
/**
 * 物理の観察値を線だけの命令へ変換する。照明モードと関係なく表示できる。
 * @param Item 一つのColliderの観察値。
	 * @param Settings 表示条件。
 */
TResult<FGeometryCommand3D> BuildPhysicsDebugGeometry3D(const FPhysicsDebugItem3D& Item,
	const FPhysicsDebugDisplaySettings3D& Settings);
/**
 * 同じビューのまま命令を一括反映する。ビューを切り替えて深度を消去しない。
 * @param Snapshot 実固定更新後の観察値。
	 * @param Settings 表示条件。
 * @param Render Applicationの共有JobSystemが接続済みの3D窓口。
 */
TResult<void> SubmitPhysicsDebugSnapshot3D(const FPhysicsDebugSnapshot3D& Snapshot,
	const FPhysicsDebugDisplaySettings3D& Settings, FRender3DContext& Render);
}
#endif
