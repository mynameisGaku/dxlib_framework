// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_DEBUG_DISPLAY_2D_H
#define DXF_PHYSICS_DEBUG_DISPLAY_2D_H
#include "Dxf/PhysicsDebugSnapshot2D.h"
#include "Dxf/Render2DContext.h"
namespace Dxf
{
/**
 * 物理のメートル座標から画面ピクセルへの表示変換。Y軸は上向きから下向きへ反転する。
 */
struct FPhysicsDebugView2D
{
	/**
	 * 物理座標の原点を置く画面位置。ピクセル単位。
	 */
	FVector2 ScreenOrigin{640, 360};
	/**
	 * 1メートルあたりのピクセル数。有限な正値。
	 */
	Toolbox::f32 PixelsPerMeter = 32;
};
/**
 * 2D物理観察の表示設定。Worldの状態へ書き戻さない。
 */
struct FPhysicsDebugDisplaySettings2D
{
	/**
	 * Colliderの輪郭と、円の姿勢を示す半径線。
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
	 * 生成する2D命令の描画レイヤー。
	 */
	Toolbox::int32 Layer = 900;
};
/**
 * 一つのColliderの観察値を既存の2D線・円命令へ変換する。
 * 失敗時はOutputを変更しない。
 * @param Item 一つのColliderの観察値。
 * @param View メートルから画面ピクセルへの変換。
 * @param Settings 表示条件。
 * @param Output 生成した命令を末尾へ追加する配列。
 */
TResult<void> BuildPhysicsDebugCommands2D(const FPhysicsDebugItem2D& Item, const FPhysicsDebugView2D& View,
                                          const FPhysicsDebugDisplaySettings2D& Settings,
                                          Toolbox::TVector<FRenderCommand>& Output);
/**
 * 採取済みの2D観察値を2D窓口へ一括で追加する。途中で失敗した場合は何も追加しない。
 * @param Snapshot 実Step後に採取・変換した観察値。
 * @param View メートルから画面ピクセルへの変換。
 * @param Settings 表示条件。
 * @param Render 共有JobSystemが接続済みの2D窓口。
 */
TResult<void> SubmitPhysicsDebugSnapshot2D(const FPhysicsDebugSnapshot2D& Snapshot, const FPhysicsDebugView2D& View,
                                           const FPhysicsDebugDisplaySettings2D& Settings, FRender2DContext& Render);
} // namespace Dxf
#endif
