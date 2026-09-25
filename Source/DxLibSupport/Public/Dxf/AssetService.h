#pragma once
#include "Dxf/TextureLoader.h"
#include "Dxf/SoundLoader.h"
#include "Dxf/FontLoader.h"
#include "Dxf/ModelLoader.h"
#include "Dxf/ResourceCache.h"
#include "Dxf/AsyncAsset.h"
#include "Toolbox/ProjectPaths.h"
namespace Dxf
{
class FTaskDispatcher;
struct FTaskScope;
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
 * モデルデータを再利用するキャッシュ。
 */
using FModelCache = TResourceCache<FModelResource>;
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
	 * モデルも扱う構成で、必要な依存関係を受け取り初期状態を構築する。
	 * 構築したスレッドがモデルのネイティブ処理を行う所有スレッドになる。
	 * @param Textures 管理するテクスチャ群。
	 * @param Sounds 管理する音声群。
	 * @param Fonts 管理するフォント群。
	 * @param Models 管理するモデル群。nullptrならモデルの読み込みは失敗する。
	 */
	FAssetService(ITextureBackend& Textures, ISoundBackend& Sounds, IFontBackend& Fonts, IModelBackend* Models);
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
	 * 画像を読み込みテクスチャを取得する。Root設定時は解決済み絶対パスで読み込む。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FTexture> LoadTexture(const Toolbox::FString& Path, const FTextureLoadOptions& Options = {});
	/**
	 * 音声ファイルを読み込む。Root設定時は解決済み絶対パスで読み込む。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FSound> LoadSound(const Toolbox::FString& Path, const FSoundLoadOptions& Options = {});
	/**
	 * .fbxを読み込む。事前の変換は不要。Root設定時は解決済み絶対パスで読み込む。
	 * 同じパスと設定で生存中のモデルがあれば共有する。所有スレッドから同期的に呼ぶ。
	 * @param Path 読み込む.fbxのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FModel> LoadModel(const Toolbox::FString& Path, const FModelLoadOptions& Options = {});
	/**
	 * モデルデータを共有し、変換と再生状態を独立に持つインスタンスを作る。所有スレッドから呼ぶ。
	 * @param Model 複製元のモデル。
	 */
	TResult<FModelInstance> CreateModelInstance(const FModel& Model);
	/**
	 * 画像の読み込みをWorkerの準備と所有側の取込に分けて要求する。
	 * 要求時点の解決済みパスを保持し、WorkerはRootを再評価しない。
	 * 取り消しや破棄で反映しなかった要求は未完了のまま残る。
	 * Dispatcherは終了前に停止し、このサービスより長く生存させること。
	 * @param Dispatcher 準備と反映の実行先。
	 * @param Scope 要求の所属。取り消し時は反映せず期限切れになる。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FAsyncTexture> LoadTextureAsync(FTaskDispatcher& Dispatcher, const FTaskScope& Scope,
	                                        const Toolbox::FString& Path,
	                                        const FTextureLoadOptions& Options = {});
	/**
	 * 音声の読み込みをWorkerの準備と所有側の取込に分けて要求する。
	 * Memory保持のみ対応し、Streamは明示的に拒否する。
	 * 要求時点の解決済みパスを保持し、WorkerはRootを再評価しない。
	 * 取り消しや破棄で反映しなかった要求は未完了のまま残る。
	 * Dispatcherは終了前に停止し、このサービスより長く生存させること。
	 * @param Dispatcher 準備と反映の実行先。
	 * @param Scope 要求の所属。取り消し時は反映せず期限切れになる。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FAsyncSound> LoadSoundAsync(FTaskDispatcher& Dispatcher, const FTaskScope& Scope,
	                                    const Toolbox::FString& Path, const FSoundLoadOptions& Options = {});
	/**
	 * ProjectRootを一度だけ設定する。以後の要求はこのRootを基準に解決する。
	 * 未設定のままなら従来どおり呼び出し側の相対パスをそのまま使う。
	 * @param Root sln配置先の完全修飾ディレクトリ。
	 * @return 初回かつ有効なRootならtrue。
	 */
	bool SetProjectRoot(const Toolbox::FPath& Root);
	/**
	 * 設定済みのProjectRootを返す。未設定なら空。
	 */
	Toolbox::FPath GetProjectRoot() const;
	/**
	 * 指定設定のフォントを取得する。
	 * @param Options 処理に適用する設定。
	 */
	TResult<FFont> LoadFont(const FFontOptions& Options = {});
	/**
	 * 一行の文字列の描画幅（画素）を、描画と同じフォントで計測する。
	 * @param Font 描画に使うフォント。
	 * @param Text UTF-8の文字列（改行を含まない）。
	 */
	TResult<Toolbox::int32> MeasureTextWidth(const FFont& Font, const Toolbox::FString& Text) const;
	/**
	 * 複数行を並べるときの行の送り（画素）。
	 * @param Font 描画に使うフォント。
	 */
	TResult<Toolbox::int32> GetFontLineHeight(const FFont& Font) const;
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
	 * 要求パスをProjectRoot基準で解決する。未設定なら正規化だけ行う。
	 * @param Path 読み込むファイルのパス。
	 * @param Resolved 解決したパスの格納先。
	 */
	bool ResolveRequestPath_Internal(const Toolbox::FString& Path, Toolbox::FString& Resolved) const;
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
	 * モデルの読み込み器。
	 */
	FModelLoader m_ModelLoader;
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
	/**
	 * モデルデータの再利用キャッシュ。
	 */
	FModelCache m_ModelCache;
	/**
	 * 要求パスをProjectRoot基準で解決する。未設定なら素通し。
	 */
	Toolbox::FAssetPathResolver m_Resolver;
};
} // namespace Dxf
