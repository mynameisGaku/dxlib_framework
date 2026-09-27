// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_RESIZE_2D_H
#define DXF_CHARACTER_RESIZE_2D_H
#include "Dxf/ColliderId2D.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Vector2.h"
namespace Dxf
{
/**
 * カプセルのキャラクターの高さ（半高）の変更を調べた結果。Worldは変更しない。
 */
struct FCharacterResize2D
{
	/**
	 * 変更できるか。falseなら元の形状と中心を使い続ける。
	 */
	bool bResized = false;
	/**
	 * 足元（中心線の下端の円の底）を保った新しい中心。変更できない場合は元の中心。
	 */
	Toolbox::FVector2 Center;
	/**
	 * 変更を妨げた（新しい形状と重なる）Collider。変更できた場合は空。
	 */
	Toolbox::TOptional<FColliderId2D> Blocker;
};
} // namespace Dxf
#endif
