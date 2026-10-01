// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_PREFABDEFINITION3D_H
#define DXF_CONTENT_PREFABDEFINITION3D_H
#include "Dxf/ContentPartDefinition3D.h"
#include "Dxf/ContentJointDefinition3D.h"
#include "Dxf/ContentAssetDefinition.h"
#include "Dxf/ContentExportDefinition.h"
#include "Dxf/ContentParameterValue.h"
#include "Toolbox/JsonDocument.h"
#include "Toolbox/SharedPtr.h"
namespace Dxf
{
/**
 * 検証済みPrefabの値。実行中の世代ID・Nativeハンドルを保持しない。
 */
struct FPrefabDefinition3D
{
	/**
	 * 診断用の定義パス。
	 */
	Toolbox::FString Path;
	/**
	 * 展開したPrefab root数。空の子でも指数的展開を制限する。
	 */
	Toolbox::uint32 ExpandedPrefabCount = 1;
	/**
	 * ファイル読込時に固定したRoot。JSONでは指定できず、空なら準備側のRootを使う。
	 */
	Toolbox::FString ProjectRoot;
	/**
	 * 解決済みの部品。
	 */
	Toolbox::TVector<FContentPartDefinition3D> Parts;
	/**
	 * 解決済みのJoint。
	 */
	Toolbox::TVector<FContentJointDefinition3D> Joints;
	/**
	 * 必須資源の型付き表。
	 */
	Toolbox::TVector<FContentAssetDefinition> Assets;
	/**
	 * 公開先の解決済み表。
	 */
	Toolbox::TVector<FContentExportDefinition> Exports;
	/**
	 * 子の公開先を検証時に解決した表。自身のExportsには自動追加しない。
	 */
	Toolbox::TVector<FContentExportDefinition> Imports;
	/**
	 * 今回の検証済み公開値。
	 */
	Toolbox::TVector<FContentParameterValue> Parameters;
};
} // namespace Dxf
#endif
