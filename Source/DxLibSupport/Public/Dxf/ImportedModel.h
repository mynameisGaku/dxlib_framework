// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_IMPORTED_MODEL_H
#define DXF_IMPORTED_MODEL_H
#include "Dxf/ModelCameraInfo.h"
#include "Dxf/ModelLightInfo.h"
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
	/**
	 * 静止カメラ。自動では適用しない。
	 */
	Toolbox::TVector<FModelCameraInfo> Cameras;
	/**
	 * 静止ライト。自動では適用しない。
	 */
	Toolbox::TVector<FModelLightInfo> Lights;
	/**
	 * 基本PBR材質を含む。モデル拡張3以降のシェーダー経路が必要。
	 */
	bool bHasPbrMaterials = false;
	/**
	 * 描画する全メッシュがUV1を持つか。個体全体へのUV上書き検証に使う。
	 */
	bool bAllMeshesHaveUv1 = true;
};
} // namespace Dxf
#endif
