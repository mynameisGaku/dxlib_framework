// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_IMPORTED_MODEL_H
#define DXF_IMPORTED_MODEL_H
#include "Dxf/ImportedModelTexture.h"
#include "Dxf/ImportedModelClip.h"
namespace Dxf
{
/**
 * FBXから変換したモデルデータを管理する型。
 */
struct FImportedModel
{
	/**
	 * DxLibが読めるテキスト形式のモデルデータ（DirectX .x）。
	 */
	Toolbox::TVector<char> ModelData;
	/**
	 * 参照するテクスチャ。
	 */
	Toolbox::TVector<FImportedModelTexture> Textures;
	/**
	 * FBX上の順序どおりのクリップ。ネイティブ側のアニメーション番号と同じ順序。
	 */
	Toolbox::TVector<FImportedModelClip> Clips;
	/**
	 * クリップのサンプリング間隔（1秒あたりのキー数）。
	 */
	Toolbox::uint32 SamplesPerSecond = 0;
};
} // namespace Dxf
#endif
