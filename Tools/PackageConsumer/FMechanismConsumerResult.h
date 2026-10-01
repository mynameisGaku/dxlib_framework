// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_MECHANISM_RESULT_H
#define DXF_PACKAGE_MECHANISM_RESULT_H
#include "Toolbox/Vector3.h"
#include "Toolbox/Function.h"
namespace Dxf
{
class FRenderContext;
}
/**
 * Sceneより長く保持する外部利用の結果。
 */
struct FMechanismConsumerResult
{
	/**
	 * 全3種類の接続・解除・再接続を確認したか。
	 */
	bool bComplete = false;
	/**
	 * Scene終了時に接続を終了したか。
	 */
	bool bShutdown = false;
	/**
	 * 実描画したRevolute／Fixed／Prismaticの補間位置。
	 */
	Toolbox::FVector3 Positions[3]{};
	/**
	 * UI adapterからの次の有限速度要求。
	 */
	Toolbox::f64 RequestedSpeed = 0;
	/**
	 * 次の更新で一度だけ要求を送るか。
	 */
	bool bDriveRequested = false;
	/**
	 * 成功PostPhysicsで観察された回転Motor設定と角速度。
	 */
	Toolbox::f64 ObservedTargetSpeed = 0;
	/**
	 * 実Bodyの回転速度。
	 */
	Toolbox::f64 ObservedAngularSpeed = 0;
	/**
	 * 固定更新へ渡したUI要求回数。
	 */
	Toolbox::uint64 AppliedRequests = 0;
	/**
	 * UI依存を持たないSceneへ外部adapterが渡す描画。
	 */
	Toolbox::TFunction<void(Dxf::FRenderContext&)> DrawOverlay;
};
#endif
