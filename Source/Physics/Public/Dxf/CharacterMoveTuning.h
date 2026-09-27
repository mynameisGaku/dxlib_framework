// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_MOVE_TUNING_H
#define DXF_CHARACTER_MOVE_TUNING_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * キャラクター移動の2D／3D共通の設定。距離は物理ワールドの距離単位、時間は秒、角度はラジアン。
 * 既定値はメートル単位の人型程度を想定した例で、ゲームごとに調整する。
 */
struct FCharacterMoveTuning
{
	/**
	 * 問い合わせる円／球の半径（有限の正の値）。
	 */
	Toolbox::f32 Radius = 0.5f;
	/**
	 * 対象の表面から保つ接触余裕（表面の法線方向の距離、有限の正の値）。移動は表面へこの距離まで近づいて止まる。
	 * SweepClosestのCenterAtHitや、ComputeSlideMoveの経路上の後退距離とは別の意味。
	 */
	Toolbox::f64 SkinWidth = 0.02;
	/**
	 * これより短い残り移動は処理しない（有限の正の値）。
	 */
	Toolbox::f64 MinMoveDistance = 1e-4;
	/**
	 * 1回の移動（MoveAndSlide）での反復上限（1以上）。
	 */
	Toolbox::int32 MaxIterations = 8;
	/**
	 * 1回の呼出しでのWorld問い合わせ（Sweep・接触）の上限（1以上）。超える前に止め、その理由を返す。
	 */
	Toolbox::int32 MaxQueries = 128;
	/**
	 * 初期重なりの解消の反復上限（0以上。0なら解消しない）。
	 */
	Toolbox::int32 MaxRecoveryIterations = 4;
	/**
	 * 初期重なりの解消で中心を動かす合計距離の上限（有限・非負）。超える深さは解消不能として返す。
	 */
	Toolbox::f64 MaxRecoveryDistance = 0.25;
	/**
	 * 歩ける床の最大傾斜（ラジアン、0以上π/2未満）。dot(法線, Up) >= cos(MaxSlopeAngle)の面を歩ける床とする。
	 */
	Toolbox::f64 MaxSlopeAngle = 0.7853981633974483;
	/**
	 * 接地中に上れる段差の高さ（有限・非負。0で段差上りを無効にする）。
	 */
	Toolbox::f64 StepHeight = 0.3;
	/**
	 * 接地を続けるために床へ吸い付く最大の下向き距離（有限・非負）。下り坂や小さな段差の下りに使う。
	 */
	Toolbox::f64 GroundSnapDistance = 0.1;
	/**
	 * 入力の大きさ1で目標とする水平方向の速さ（距離毎秒、有限・非負）。
	 */
	Toolbox::f64 MaxSpeed = 5;
	/**
	 * 入力がある間、水平速度を目標へ近づける加速度（距離毎秒毎秒、有限・非負）。
	 */
	Toolbox::f64 Acceleration = 40;
	/**
	 * 入力がない間、水平速度を0へ近づける減速度（距離毎秒毎秒、有限・非負）。
	 */
	Toolbox::f64 Deceleration = 40;
	/**
	 * 空中での加速度・減速度の倍率（0以上1以下）。
	 */
	Toolbox::f64 AirControl = 0.3;
	/**
	 * ジャンプ開始時のUp方向の速さ（距離毎秒、有限・非負）。
	 */
	Toolbox::f64 JumpSpeed = 6;
	/**
	 * キャラクターに掛ける重力加速度の大きさ（-Up方向、距離毎秒毎秒、有限・非負）。Worldの重力とは独立。
	 */
	Toolbox::f64 Gravity = 20;
	/**
	 * 落下速度の上限（距離毎秒、有限の正の値）。
	 */
	Toolbox::f64 MaxFallSpeed = 30;
	/**
	 * 固定更新の開始時に歩ける床がKinematicのBodyなら、その物理Stepでの床の運動（並進・回転）に中心を追従させるか。
	 * 追従は床に固定した中心の点の移動（FPhysicsWorld::PredictBodyPoint）で、床以外のSolidとの経路を検査する。
	 * 回転は同じ積分の途中点へ最大32分割し、曲線のずれを半径へ加える。ずれをSkinWidth/4以内にできない、
	 * または角速度×固定秒数がπを超える場合は追従を拒否する。障害物で軌道から外れたら安全な位置で止める。
	 */
	bool bFollowMovingGround = true;
	/**
	 * 動く床を離れた固定更新（ジャンプ・歩いて降りる）で、開始時のキャラクター中心の支持点速度を一度だけ速度へ加えるか。
	 * 支持点速度は床の並進速度＋角速度×（初期重なり解消後・歩行とジャンプ前の中心−床の重心）。終点変位/秒数ではない。
	 */
	bool bInheritGroundVelocity = true;
	/**
	 * 追従する床の点の速さの上限（距離毎秒、有限の正の値）。並進の速さ＋回転接線の速さが超える運動は追従を拒否する。
	 * 拒否した床の変位・速度はキャラクターへ加えず、通常の歩行・ジャンプの結果とbCarryRejectedを返す。
	 */
	Toolbox::f64 MaxGroundCarrySpeed = 50;
};
} // namespace Dxf
#endif
