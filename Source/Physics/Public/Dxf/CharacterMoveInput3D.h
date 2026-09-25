// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_MOVE_INPUT_3D_H
#define DXF_CHARACTER_MOVE_INPUT_3D_H
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 1回の固定更新に渡す、入力デバイスに依存しない移動要求。プレイヤー入力でもAIでも同じ値を使う。
 */
struct FCharacterMoveInput3D
{
	/**
	 * 希望する水平方向の移動（Upに直交する平面内の成分だけを使う）。長さ1で最大速度、1を超える長さは1へ縮める。
	 */
	Toolbox::FVector3 Move;
	/**
	 * この更新でジャンプを試みるか（歩ける床に立っている場合だけ有効）。
	 */
	bool bJump = false;
};
} // namespace Dxf
#endif
