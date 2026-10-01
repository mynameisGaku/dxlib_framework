// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_PREPAREDPREFAB3D_H
#define DXF_CONTENT_PREPAREDPREFAB3D_H
#include "Dxf/PrefabDefinition3D.h"
#include "Dxf/ContentResources.h"
namespace Dxf
{
/**
 * 所有側で準備した不変定義と共有資源。Workerへ渡さない。
 */
struct FPreparedPrefab3D
{
	/**
	 * 保持する不変定義。
	 */
	Toolbox::TSharedPtr<const FPrefabDefinition3D> Definition;
	/**
	 * AssetService経由の共有参照。
	 */
	Toolbox::TSharedPtr<const FContentResources> Resources;
};
/**
 * 全資源を準備する。失敗で既存定義を変更しない。
 * @param Definition 検証済み初期構成。
 * @param Assets 所有側サービス。
 */
FPreparedPrefab3D PreparePrefab(FPrefabDefinition3D Definition, FAssetService& Assets);
} // namespace Dxf
#endif
