#pragma once
#include "Dxf/AssetBackend.h"
#include "Dxf/Texture.h"
namespace Dxf
{
/**
 * テクスチャの読み込み器を管理する型。
 */
class FTextureLoader
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Backend ネイティブ処理の呼び出し先。
	 * @param Registry リソースの登録先。
	 */
	FTextureLoader(ITextureBackend& Backend, FResourceRegistry& Registry) : m_pBackend(&Backend), m_pRegistry(&Registry)
	{
	}
	/**
	 * 対象のリソースを読み込む。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FTexture> Load(const Toolbox::FString& Path, const FTextureLoadOptions& Options);
	/**
	 * 準備済みデータからリソースを読み込む。ファイルを読まない。
	 * @param Data 画像ファイルのバイト列。呼び出し中だけ有効。
	 * @param Size バイト列の長さ。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FTexture> LoadMemory(const void* Data, Toolbox::size_t Size, const FTextureLoadOptions& Options);
	/**
	 * 描画先として使うテクスチャを生成する。
	 * @param Width 幅。
	 * @param Height 高さ。
	 * @param bAlpha 透過を扱う描画先を生成するか。
	 */
	TResult<FRenderTarget> CreateRenderTarget(Toolbox::int32 Width, Toolbox::int32 Height, bool bAlpha);

private:
	/**
	 * ネイティブリソースの所有権を引き受ける。
	 * @param Allocation ネイティブリソースの確保結果。
	 * @param bRenderTarget 描画先として確保したリソースか。
	 */
	TResult<Toolbox::TSharedPtr<FTextureResource>> Adopt_Internal(FTextureAllocation Allocation, bool bRenderTarget);
	/**
	 * ネイティブ処理の呼び出し先。
	 */
	ITextureBackend* m_pBackend;
	/**
	 * リソースの登録先。
	 */
	FResourceRegistry* m_pRegistry;
};
} // namespace Dxf
