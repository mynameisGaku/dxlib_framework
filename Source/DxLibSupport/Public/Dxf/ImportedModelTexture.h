// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_IMPORTED_MODEL_TEXTURE_H
#define DXF_IMPORTED_MODEL_TEXTURE_H
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 変換後のモデルが参照するテクスチャを管理する型。
 */
struct FImportedModelTexture
{
	/**
	 * 変換後のモデルデータ内で使う参照名。
	 */
	Toolbox::FString Name;
	/**
	 * モデルファイルのディレクトリからの相対パス（UTF-8）。埋め込みテクスチャでは空。
	 */
	Toolbox::FString RelativePath;
	/**
	 * FBXへ記録された絶対パス（UTF-8）。相対パスで見つからない場合の候補。
	 */
	Toolbox::FString AbsolutePath;
	/**
	 * FBXへ埋め込まれた画像データ。外部ファイル参照では空。
	 */
	Toolbox::TVector<Toolbox::uint8> Embedded;
};
} // namespace Dxf
#endif
