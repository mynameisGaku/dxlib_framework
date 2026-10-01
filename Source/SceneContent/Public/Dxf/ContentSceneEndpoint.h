// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_SCENE_ENDPOINT_H
#define DXF_CONTENT_SCENE_ENDPOINT_H
#include "Dxf/ContentExportDefinition.h"
namespace Dxf
{
/**
 * 同じScene内の明示公開先。別Worldや内部部品への参照ではない。
 */
struct FContentSceneEndpoint
{
	/**
	 * Sceneの解決済みInstance配列位置。
	 */
	Toolbox::uint32 Instance = 0;
	/**
	 * 型と部品位置を検査済みのBody公開先。
	 */
	FContentExportDefinition Export;
};
} // namespace Dxf
#endif
