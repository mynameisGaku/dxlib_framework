// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_VALIDATION_H
#define DXF_CONTENT_VALIDATION_H
#include "Dxf/PrefabDefinition2D.h"
#include "Dxf/PrefabDefinition3D.h"
#include "Dxf/SceneContentLimits.h"
#include "Dxf/SceneDefinition2D.h"
#include "Dxf/SceneDefinition3D.h"
namespace Dxf
{
/**
 * C++で作った解決済み初期定義も、資源準備と実体登録の前に検査する。
 * @param Definition 実行時IDを含まない型付き値。変更やWorld生成は行わない。
 * @param Limits 部品・Joint・資源・名前の有限上限。不正は診断付き例外。
 */
void ValidatePrefabDefinition(const FPrefabDefinition2D& Definition, FSceneContentLimits Limits = {});
/**
 * 3D初期定義の数値・解決済み参照を、Native作成の前に検査する。
 * @param Definition 実行時IDを含まない型付き値。QuaternionとFrameも検査する。
 * @param Limits 有限の上限。不正は診断付き例外。
 */
void ValidatePrefabDefinition(const FPrefabDefinition3D& Definition, FSceneContentLimits Limits = {});
/**
 * 全Prefab・初期配置・共有資源・Scene接続を、最初の資源取込前に検査する。
 * @param Definition 解決済みScene。WorldやNative状態は変更しない。
 * @param Limits 展開全体の有限上限。不正は診断付き例外。
 */
void ValidateSceneDefinition(const FSceneDefinition2D& Definition, FSceneContentLimits Limits = {});
/**
 * 3Dの初期配置とScene公開先も検査する。
 * @param Definition 解決済みScene。WorldやNative状態は変更しない。
 * @param Limits 展開全体の有限上限。不正は診断付き例外。
 */
void ValidateSceneDefinition(const FSceneDefinition3D& Definition, FSceneContentLimits Limits = {});
} // namespace Dxf
#endif
