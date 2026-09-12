#pragma once
#include "Dxf/AssetBackend.h"
namespace Dxf
{
/**
 * DxLibによるテクスチャの生成と解放を管理する型。
 */
class FDxLibTextureBackend final : public ITextureBackend
{
public:
	/**
	 * 画像を読み込みテクスチャを取得する。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FTextureAllocation> LoadTexture(const Toolbox::FString& Path, const FTextureLoadOptions& Options) override;
	/**
	 * 描画先として使うテクスチャを生成する。
	 * @param Width 幅。
	 * @param Height 高さ。
	 * @param bAlpha 透過を扱う描画先を生成するか。
	 */
	TResult<FTextureAllocation> CreateRenderTarget(Toolbox::int32 Width, Toolbox::int32 Height, bool bAlpha) override;
	/**
	 * ネイティブテクスチャを解放する。
	 * @param Handle ハンドル。
	 */
	void DeleteTexture(Toolbox::int32 Handle) noexcept override;
};
} // namespace Dxf
