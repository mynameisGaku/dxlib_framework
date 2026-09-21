// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_DEBUG_CAMERA_3D_H
#define DXF_DEBUG_CAMERA_3D_H
#include "Dxf/RenderView3D.h"
#include "Dxf/Result.h"
namespace Dxf
{
/**
 * 注視点、距離、ラジアン角で保持するデバッグカメラの姿勢。
 */
struct FDebugCameraPose3D
{
	/**
	 * ワールド座標の注視点。
	 */
	Toolbox::FVector3 Focus{0, 1, 0};
	/**
	 * 注視点までの距離。単位はメートル。
	 */
	Toolbox::f64 Distance = 8;
	/**
	 * Y軸回りの水平角。
	 */
	Toolbox::f64 Yaw = -0.55;
	/**
	 * 上向きを正とする仰角。極点の手前で制限する。
	 */
	Toolbox::f64 Pitch = -0.35;
};
/**
 * 描画や入力デバイスに依存しない、単独所有の調査用カメラ。
 * ゲームを停止していても実時間入力で操作できる。
 */
class FDebugCamera3D
{
public:
	/**
	 * 現在の姿勢を値で返す。
	 */
	FORCEINLINE FDebugCameraPose3D GetPose() const noexcept
	{
		return m_Pose;
	}
	/**
	 * @param Pose 有限値、距離0.1〜10000、注視点各軸±100000以内の姿勢。
	 */
	TResult<void> SetPose(const FDebugCameraPose3D& Pose);
	/**
	 * @param YawDelta 水平角差。
	 * @param PitchDelta 仰角差。いずれもラジアン。
	 */
	TResult<void> Orbit(Toolbox::f64 YawDelta, Toolbox::f64 PitchDelta);
	/**
	 * @param Factor 距離へ掛ける有限な正の倍率。範囲端では停止する。
	 */
	TResult<void> Zoom(Toolbox::f64 Factor);
	/**
	 * 注視点と視点を一緒に移す。斜め入力で速度を増やさない。
	 * @param Input ローカル右・ワールド上・視線前方の入力、各軸-1〜1。
	 * @param RealSeconds 経過実秒数。0〜0.25。
	 * @param Speed メートル毎秒。0〜1000。
	 */
	TResult<void> MoveLocal(Toolbox::FVector3 Input, Toolbox::f64 RealSeconds, Toolbox::f64 Speed);
	/**
	 * @param Base 光・投影・表示モードを保ち、Eye/Target/Upだけを差し替える基準ビュー。
	 */
	TResult<FRenderView3D> MakeView(const FRenderView3D& Base) const;
private:
	/**
	 * 状態変更前に姿勢と丸め後のカメラ基底を検査する。
	 */
	static bool IsValidPose_Internal(const FDebugCameraPose3D& Pose) noexcept;
	/**
	 * 現在のカメラ姿勢。
	 */
	FDebugCameraPose3D m_Pose;
};
}
#endif
