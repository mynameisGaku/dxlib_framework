// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_EXPANSION_H
#define DXF_CONTENT_EXPANSION_H
#include "Dxf/ContentSchemaReader.h"
#include "Dxf/SceneDefinition2D.h"
#include "Dxf/SceneDefinition3D.h"
#include "Toolbox/Function.h"
namespace Dxf::ContentPrivate
{
/**
 * 子PrefabのCPU展開をファイル読取側へ委譲する。WorldやNativeを捕捉しない。
 */
using FChildExpansion2D = Toolbox::TFunction<void(FSchemaReader&, Toolbox::int32, FPrefabDefinition2D&)>;
/**
 * 3Dの子を同じ読取・制限の範囲で展開する。
 */
using FChildExpansion3D = Toolbox::TFunction<void(FSchemaReader&, Toolbox::int32, FPrefabDefinition3D&)>;
/**
 * ReaderでIndexの接続を検査し、Definitionへ型付き参照を保存する。Limits超過は例外。
 */
void ReadSceneConnections(FSchemaReader& Reader, Toolbox::int32 Index, FSceneDefinition2D& Definition, FSceneContentLimits Limits);
/**
 * ReaderでIndexの接続を検査し、Definitionへ型付き参照を保存する。Limits超過は例外。
 */
void ReadSceneConnections(FSchemaReader& Reader, Toolbox::int32 Index, FSceneDefinition3D& Definition, FSceneContentLimits Limits);
// Sceneの共有資源も同じ型・パス検査を使う。
/**
 * ReaderでIndexの資源を検査する。未知項目、型違い、Limits超過は例外。
 */
Toolbox::TVector<FContentAssetDefinition> ReadSceneAssets(FSchemaReader& Reader, Toolbox::int32 Index, FSceneContentLimits Limits);
// 本文だけの入口は子参照を拒否し、Source入口だけが限定したファイル展開を提供する。
/**
 * Textを2D定義へ検証する。Pathは診断、Overridesは個体値、Expandは限定した子展開。
 */
FPrefabDefinition2D ParseExpanded2D(Toolbox::FStringView Text, const Toolbox::FString& Path, const Toolbox::TVector<FContentParameterValue>& Overrides, FSceneContentLimits Limits, const FChildExpansion2D& Expand, const Toolbox::TVector<FContentAssetDefinition>& SharedAssets = {});
/**
 * Textを3D定義へ検証する。Pathは診断、Overridesは個体値、Expandは限定した子展開。
 */
FPrefabDefinition3D ParseExpanded3D(Toolbox::FStringView Text, const Toolbox::FString& Path, const Toolbox::TVector<FContentParameterValue>& Overrides, FSceneContentLimits Limits, const FChildExpansion3D& Expand, const Toolbox::TVector<FContentAssetDefinition>& SharedAssets = {});
} // namespace Dxf::ContentPrivate
#endif
