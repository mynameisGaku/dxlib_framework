#pragma once
#include "Dxf/TextureLoader.h"
#include "Dxf/SoundLoader.h"
#include "Dxf/FontLoader.h"
#include "Dxf/ResourceCache.h"
namespace Dxf
{
/**
 * テクスチャを再利用するキャッシュ。
 */
using FTextureCache = TResourceCache<FTextureResource>;
/**
 * 音声を再利用するキャッシュ。
 */
using FSoundCache = TResourceCache<FSoundResource>;
/**
 * フォントを再利用するキャッシュ。
 */
using FFontCache = TResourceCache<FFontResource>;
/**
 * 単一スレッドで使用する。バックエンドはこのサービスの終了処理後まで存続させる。
 */
class FAssetService
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Textures 管理するテクスチャ群。
	 * @param Sounds 管理する音声群。
	 * @param Fonts 管理するフォント群。
	 */
	FAssetService(ITextureBackend& Textures, ISoundBackend& Sounds, IFontBackend& Fonts);
	/**
	 * 所有する状態を終了し、必要なリソースを解放する。
	 */
	~FAssetService();
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FAssetService(const FAssetService&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FAssetService& operator=(const FAssetService&) = delete;
	/**
	 * 画像を読み込みテクスチャを取得する。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FTexture> LoadTexture(const Toolbox::FString& Path, const FTextureLoadOptions& Options = {});
	/**
	 * 音声ファイルを読み込む。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FSound> LoadSound(const Toolbox::FString& Path, const FSoundLoadOptions& Options = {});
	/**
	 * 指定設定のフォントを取得する。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FFont> LoadFont(const FFontOptions& Options = {});
	/**
	 * 描画先として使うテクスチャを生成する。
	 * @param Width 幅。
	 * @param Height 高さ。
	 * @param bAlpha 透過を扱う描画先を生成するか。
	 */
	TResult<FRenderTarget> CreateRenderTarget(Toolbox::int32 Width, Toolbox::int32 Height, bool bAlpha = true);
	/**
	 * 参照されていないキャッシュ項目を除去する。
	 */
	void CollectUnused();
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	void Shutdown() noexcept;

private:
	/**
	 * リソースの登録先。
	 */
	FResourceRegistry m_Registry;
	/**
	 * テクスチャの読み込み器。
	 */
	FTextureLoader m_TextureLoader;
	/**
	 * 音声の読み込み器。
	 */
	FSoundLoader m_SoundLoader;
	/**
	 * フォントの読み込み器。
	 */
	FFontLoader m_FontLoader;
	/**
	 * テクスチャの再利用キャッシュ。
	 */
	FTextureCache m_TextureCache;
	/**
	 * 音声の再利用キャッシュ。
	 */
	FSoundCache m_SoundCache;
	/**
	 * フォントの再利用キャッシュ。
	 */
	FFontCache m_FontCache;
};
} // namespace Dxf
