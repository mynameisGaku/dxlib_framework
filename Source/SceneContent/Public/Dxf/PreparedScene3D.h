// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_PREPAREDSCENE3D_H
#define DXF_CONTENT_PREPAREDSCENE3D_H
#include "Dxf/SceneDefinition3D.h"
#include "Dxf/PreparedPrefab3D.h"
namespace Dxf
{
/**
 * Scene変更を要求する前に準備した全必須資源と初期構成。
 */
struct FPreparedScene3D
{
	/**
	 * Prefabが空でも全必須資源の取得と寿命を保持する共通表。
	 */
	Toolbox::TSharedPtr<const FContentResources> Resources;
	/**
	 * 配列順と配置・World重力を保持する不変定義。
	 */
	Toolbox::TSharedPtr<const FSceneDefinition3D> Definition;
	/**
	 * 個体ごとの検証済み定義と資源参照。
	 */
	Toolbox::TVector<FPreparedPrefab3D> Prefabs;
};
/**
 * 全必須資源を所有側で準備する。失敗なら旧Sceneへの変更要求を出さない。
 * @param Definition CPU側で検証したSceneの初期構成。
 * @param Assets Native資源の作成・解放を担当する既存サービス。
 */
FPreparedScene3D PrepareScene(FSceneDefinition3D Definition, FAssetService& Assets);
} // namespace Dxf
#endif
