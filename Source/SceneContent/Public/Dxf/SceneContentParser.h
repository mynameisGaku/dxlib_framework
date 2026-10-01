// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SCENE_CONTENT_PARSER_H
#define DXF_SCENE_CONTENT_PARSER_H
#include "Dxf/PrefabDefinition2D.h"
#include "Dxf/PrefabDefinition3D.h"
#include "Dxf/SceneContentLimits.h"
#include "Dxf/SceneContentError.h"
namespace Dxf
{
/**
 * JSONを型付きPrefabへ検証する。I/O、World登録、資源作成を行わない。
 * @param Text 全UTF-8本文。
 * @param Path 診断用のProjectRoot相対パス。
 * @param Overrides 名前・型の一致するインスタンス専用公開値。
 * @param Limits 読み取りと部品数の有限上限。
 */
FPrefabDefinition2D ParsePrefab2D(Toolbox::FStringView Text, Toolbox::FString Path = {}, const Toolbox::TVector<FContentParameterValue>& Overrides = {}, FSceneContentLimits Limits = {});
/**
 * 3DのJSONを検証する。Quaternionは[x,y,z,w]、単位はm/kg/s/rad。
 * @param Text 全UTF-8本文。
 * @param Path 診断用のProjectRoot相対パス。
 * @param Overrides インスタンス専用公開値。
 * @param Limits 読み取りと部品数の有限上限。
 */
FPrefabDefinition3D ParsePrefab3D(Toolbox::FStringView Text, Toolbox::FString Path = {}, const Toolbox::TVector<FContentParameterValue>& Overrides = {}, FSceneContentLimits Limits = {});
} // namespace Dxf
#endif
