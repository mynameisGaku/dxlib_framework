// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_DEBUG_PICKING_3D_H
#define DXF_PHYSICS_DEBUG_PICKING_3D_H
#include "Dxf/PhysicsDebugSnapshot3D.h"
#include "Dxf/PhysicsDebugPick3D.h"
#include "Dxf/RenderGeometry3D.h"
#include "Toolbox/Optional.h"
namespace Dxf
{
/**
 * 採取済み形状の最近接Colliderを返す。非交差・未採取は成功した空Optional。
 * 全入力を検査し、不正形状・ID不整合・計算不能は失敗。同距離はItemsの先頭側を優先。
 * 最大256件、ID検査O(n²)、交差O(n)、追加記憶O(1)。再採取・Step・並べ替えを行わない。
 * @param Snapshot 表示と共有する不変の採取値。World破棄後も利用できる。
 * @param Segment 近接面～遠方面等の有限線分。端点と内部始点を含む。
 */
TResult<Toolbox::TOptional<FPhysicsDebugPick3D>> PickPhysicsDebugSnapshot3D(const FPhysicsDebugSnapshot3D& Snapshot, const FLine3D& Segment);
} // namespace Dxf
#endif
