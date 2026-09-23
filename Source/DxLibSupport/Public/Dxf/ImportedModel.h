// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_IMPORTED_MODEL_H
#define DXF_IMPORTED_MODEL_H
#include "Dxf/ImportedModelTexture.h"
#include "Dxf/ImportedModelMesh.h"
#include "Dxf/ImportedModelMorph.h"
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
	 * DxLibの読込境界で付加するメッシュ属性。空なら標準.xだけで読める。
	 */
	Toolbox::TVector<FImportedModelMesh> MeshExtensions;
	/**
	 * 頂点色を有効にする変換後のフレーム名。材質別に分かれたメッシュすべてへ適用する。
	 */
	Toolbox::TVector<Toolbox::FString> VertexColorFrames;
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
	/**
	 * 未対応データを省略した理由と、解析時の回復可能な問題。成功でも空とは限らない。
	 */
	Toolbox::TVector<Toolbox::FString> Warnings;
	/**
	 * ネイティブのシェイプ番号順のモーフ。
	 */
	Toolbox::TVector<FImportedModelMorph> Morphs;
};
} // namespace Dxf
#endif
