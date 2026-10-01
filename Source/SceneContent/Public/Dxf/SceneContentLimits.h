// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SCENE_CONTENT_LIMITS_H
#define DXF_SCENE_CONTENT_LIMITS_H
#include "Toolbox/JsonLimits.h"
namespace Dxf
{
/**
 * 一要求の読み取りと展開を制限する。切り捨てず超過は失敗する。
 */
struct FSceneContentLimits
{
	/**
	 * 一つのJSONの上限。
	 */
	Toolbox::FJsonLimits Json;
	/**
	 * 要求全体の定義バイト上限。
	 */
	Toolbox::size_t MaxTotalBytes = 32 * 1024 * 1024;
	/**
	 * Prefab依存の深さ。最上位を1と数える。
	 */
	Toolbox::uint32 MaxPrefabDepth = 16;
	/**
	 * 異なる定義ファイル数。
	 */
	Toolbox::uint32 MaxFiles = 128;
	/**
	 * 展開後の部品数。
	 */
	Toolbox::uint32 MaxParts = 4096;
	/**
	 * 展開後のJoint数。
	 */
	Toolbox::uint32 MaxJoints = 8192;
	/**
	 * 展開後の資源数。
	 */
	Toolbox::uint32 MaxAssets = 1024;
	/**
	 * 一論理IDの最大バイト長。
	 */
	Toolbox::uint32 MaxIdBytes = 128;
};
} // namespace Dxf
#endif
