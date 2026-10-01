// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_PREFABSPAWNOPTIONS3D_H
#define DXF_CONTENT_PREFABSPAWNOPTIONS3D_H
#include "Toolbox/Vector2.h"
#include "Toolbox/Quaternion.h"
namespace Dxf
{
/**
 * 生成時に一度だけ適用する剛体配置変換。scaleは提供しない。
 */
struct FPrefabSpawnOptions3D
{
	/**
	 * 配置のWorld位置（m）。
	 */
	Toolbox::FVector3 Position;
	/**
	 * 配置のWorld回転。2Dはrad、3Dは単位Quaternion。
	 */
	Toolbox::FQuaternion Rotation{};
};
} // namespace Dxf
#endif
