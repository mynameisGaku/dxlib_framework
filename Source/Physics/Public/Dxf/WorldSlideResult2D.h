// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_SLIDE_RESULT_2D_H
#define DXF_WORLD_SLIDE_RESULT_2D_H
#include "Dxf/WorldSlideStop.h"
#include "Dxf/WorldSweepHit2D.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Vector2.h"
namespace Dxf
{
/**
 * 円の移動候補（最大1回の滑り補正）の結果。Worldや内部配列を参照しない値。
 */
struct FWorldSlideResult2D
{
	/**
	 * 選んだ最終中心（円の中心。接触表面の点ではない）。開始時の接触で止めた場合は開始中心。
	 */
	Toolbox::FVector2 EndCenter;
	/**
	 * EndCenterを選んだ理由。
	 */
	EWorldSlideStop Stop = EWorldSlideStop::NoMovement;
	/**
	 * 開始中心から希望終点への最初の移動のヒット（SweepClosestの結果をそのまま保持）。非交差なら空。
	 */
	Toolbox::TOptional<FWorldSweepHit2D> FirstHit;
	/**
	 * 補正した滑り経路のヒット。Fractionはその滑り経路に対する割合で、元の移動全体の割合ではない。
	 * 滑り経路を問い合わせなかった場合と、非交差だった場合は空。
	 */
	Toolbox::TOptional<FWorldSweepHit2D> SlideHit;
};
} // namespace Dxf
#endif
