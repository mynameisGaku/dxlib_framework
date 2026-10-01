// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_VIEWS_H
#define DXF_CONTENT_VIEWS_H
#include "Dxf/ContentSchemaReader.h"
#include "Dxf/SceneDefinition2D.h"
#include "Dxf/SceneDefinition3D.h"
namespace Dxf::ContentPrivate
{
/**
 * @param Reader Sceneを読む純粋な検査器。
 * @param Node 任意のviews配列。不在なら既定の一画面。
 * @param Definition 2D表示条件の保存先。
 */
void ReadViews(FSchemaReader& Reader, Toolbox::int32 Node, FSceneDefinition2D& Definition);
/**
 * @param Reader Sceneを読む純粋な検査器。
 * @param Node 任意のviews配列。不在なら既定の一画面。
 * @param Definition 3D表示条件の保存先。
 */
void ReadViews(FSchemaReader& Reader, Toolbox::int32 Node, FSceneDefinition3D& Definition);
} // namespace Dxf::ContentPrivate
#endif
