#pragma once
#include "Dxf/CharacterMoveSettings2D.h"
#include "Dxf/WorldQueryFilter.h"
#include "Toolbox/Vector2.h"
namespace Dxf
{
/**
 * DCharacterMovement2DComponentの初期条件。
 */
struct FCharacterMovementDescription2D
{
	/**
	 * 移動の設定（半径・接触余裕・速度・ジャンプ・段差など）。
	 */
	FCharacterMoveSettings2D Settings;
	/**
	 * 初期の円の中心（物理ワールド座標）。
	 */
	Toolbox::FVector2 Center;
	/**
	 * 自分をWorldへ登録するか。trueならKinematicのBodyと円のColliderを一つ作り、ほかの問い合わせから見えるようにする。
	 * 位置はこのComponentだけが決め、Worldの速度積分では動かさない。
	 */
	bool bRegisterBody = true;
	/**
	 * 登録するColliderの問い合わせカテゴリ。
	 */
	Toolbox::uint32 BodyQueryCategory = 1u;
	/**
	 * 移動の問い合わせの対象（自分のBodyは常に除く）。
	 */
	FWorldQueryFilter Filter;
};
} // namespace Dxf
