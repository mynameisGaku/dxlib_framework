// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MODEL_IMPORT_H
#define DXF_MODEL_IMPORT_H
#include "Dxf/Result.h"
#include "Dxf/ImportedModel.h"
#include "Dxf/ModelImportOptions.h"
namespace Dxf
{
/**
 * FBXのファイル内容を、DxLibが直接読めるモデルデータへ変換する。
 * 座標系はDxLibと同じ左手系・Y軸上向きへ変換する。長さの単位は設定に従う（既定はファイルのまま）。
 * 座標系と単位の変換はこの関数だけで行う。
 * ネイティブAPIを呼ばないため、どのスレッドからも呼べる。
 * @param Data FBXファイルの内容。
 * @param Size バイト数。
 * @param Options 変換設定。
 * @return 変換結果。
 */
TResult<FImportedModel> ImportFbxModel(const void* Data, Toolbox::size_t Size, const FModelImportOptions& Options = {});
} // namespace Dxf
#endif
