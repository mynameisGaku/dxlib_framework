// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_MOVE_RESULT_3D_H
#define DXF_CHARACTER_MOVE_RESULT_3D_H
#include "Dxf/CharacterMoveStop.h"
#include "Dxf/ColliderId3D.h"
#include "Toolbox/Array.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 移動中に制約として使った接触面。
 */
struct FCharacterMoveContact3D
{
	/**
	 * 接触したCollider（問い合わせ時点の完全なID）。
	 */
	FColliderId3D Collider;
	/**
	 * 対象から球の中心へ向く単位法線（制約として使った値）。
	 */
	Toolbox::FVector3 Normal;
};
/**
 * MoveAndSlideの結果。Worldや内部配列を参照しない値。
 */
struct FCharacterMoveResult3D
{
	/**
	 * 保持できる接触面の最大数。
	 */
	static constexpr Toolbox::uint32 MaxContacts = 8;
	/**
	 * 到達した中心（すべての経路を検査済みのf32座標）。
	 */
	Toolbox::FVector3 EndCenter;
	/**
	 * 実際に動いた変位（EndCenter - 開始中心、f64で求めてf32へ丸めた値）。
	 */
	Toolbox::FVector3 Applied;
	/**
	 * 処理しなかった移動（上限・方向不明などで止まった場合）。制約で取り除いた内向き成分は含まない。
	 */
	Toolbox::FVector3 Remaining;
	/**
	 * 止まった理由。
	 */
	ECharacterMoveStop Stop = ECharacterMoveStop::NoMovement;
	/**
	 * 制約として使った接触面（先頭ContactCount件、使った順）。
	 */
	Toolbox::TArray<FCharacterMoveContact3D, MaxContacts> Contacts;
	/**
	 * Contactsのうち有効な件数。
	 */
	Toolbox::uint32 ContactCount = 0;
	/**
	 * 行った反復の回数。
	 */
	Toolbox::int32 Iterations = 0;
	/**
	 * 行ったWorld問い合わせの回数。
	 */
	Toolbox::int32 Queries = 0;
};
} // namespace Dxf
#endif
