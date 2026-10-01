// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_ASSET_DEFINITION_H
#define DXF_CONTENT_ASSET_DEFINITION_H
#include "Dxf/AssetBackend.h"
#include "Dxf/ModelLoader.h"
namespace Dxf
{
/**
 * 資源の型。準備は既存AssetServiceへ委譲する。
 */
enum class EContentAssetKind : Toolbox::uint8
{
	/**
	 * 画像。
	 */
	Texture,
	/**
	 * モデル。
	 */
	Model,
	/**
	 * 音。
	 */
	Sound,
	/**
	 * フォント。
	 */
	Font
};
/**
 * 必須資源の条件。Nativeハンドルや生成済み参照を保持しない。
 */
struct FContentAssetDefinition
{
	/**
	 * 論理キー。
	 */
	Toolbox::FString Id;
	/**
	 * 資源の型。
	 */
	EContentAssetKind Kind = EContentAssetKind::Texture;
	/**
	 * ProjectRoot基準の相対パス。Fontは空。
	 */
	Toolbox::FString Path;
	/**
	 * 画像の条件。
	 */
	FTextureLoadOptions Texture;
	/**
	 * モデルの条件。
	 */
	FModelLoadOptions Model;
	/**
	 * 音の保持条件。
	 */
	FSoundLoadOptions Sound;
	/**
	 * フォントの条件。
	 */
	FFontOptions Font;
};
} // namespace Dxf
#endif
