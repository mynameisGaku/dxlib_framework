// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CONTACT_3D_H
#define TOOLBOX_CONTACT_3D_H
#include "Toolbox/CollisionShapes.h"
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 二形状の最近傍から作る単一接触。法線は二つ目から一つ目へ向く。
 */
struct FContactPoint3D
{
	/**
	 * 両表面の中点。メートル単位。
	 */
	FVector3 Position;
	/**
	 * 一つ目を押し出す方向の単位法線。
	 */
	FVector3 Normal{1, 0, 0};
	/**
	 * 表面間の符号付き距離。負は貫通深度の大きさ。
	 */
	f32 Separation = 0;
	/**
	 * 箱側の面・辺・頂点を区別する安定ID。球同士は0。
	 */
	uint32 FeatureId = 0;
};
/**
 * 球同士の接触を求める。不正な入力はFExceptionで通知する。
 * @param A 一つ目の球。
 * @param B 二つ目の球。
 */
FContactPoint3D FindContact(const FSphere& A, const FSphere& B);
/**
 * 球と任意姿勢の箱の接触を求める。軸は単位化して使用する。
 * 不正な入力はFExceptionで通知する。
 * @param Sphere 対象の球。
 * @param Box 対象の箱。
 */
FContactPoint3D FindContact(const FSphere& Sphere, const FOBB& Box);
/**
 * 箱と球の順序を入れ替えた接触。法線だけが反転する。
 * @param Box 対象の箱。
 * @param Sphere 対象の球。
 */
FORCEINLINE FContactPoint3D FindContact(const FOBB& Box, const FSphere& Sphere)
{
	FContactPoint3D Hit = FindContact(Sphere, Box);
	Hit.Normal = -Hit.Normal;
	return Hit;
}
} // namespace Toolbox
#endif
