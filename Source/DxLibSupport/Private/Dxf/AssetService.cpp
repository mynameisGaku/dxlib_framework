#include "Dxf/AssetService.h"
#include "Toolbox/Platform.h"
#include "Dxf/Utf8.h"
namespace Dxf
{
namespace
{
// 読み込み用のパスを正規化する。
// @param Path 読み込むファイルのパス。
Toolbox::FString NormalizePath_Internal(const Toolbox::FString& Path)
{
	// 正規化したパス。
	auto Normalized = Toolbox::FPath(Path).Normalize().ToUtf8();
	return Toolbox::FString(reinterpret_cast<const char*>(Normalized.Data()), Normalized.Size());
}
} // namespace
// 要求パスをProjectRoot基準で解決する。未設定なら正規化だけ行う。
// @param Path 読み込むファイルのパス。
// @param Resolved 解決したパスの格納先。
bool FAssetService::ResolveRequestPath_Internal(const Toolbox::FString& Path, Toolbox::FString& Resolved) const
{
	if (!m_Resolver.IsSet())
	{
		Resolved = NormalizePath_Internal(Path);
		return true;
	}
	// 解決した絶対パス。
	Toolbox::FPath Absolute;
	if (!m_Resolver.Resolve(Path, Absolute))
	{
		return false;
	}
	// 正規化した解決結果。
	auto Normalized = Absolute.Normalize().ToUtf8();
	Resolved = Toolbox::FString(reinterpret_cast<const char*>(Normalized.Data()), Normalized.Size());
	return true;
}
// 必要な依存関係を受け取り、初期状態を構築する。
// @param Textures 管理するテクスチャ群。
// @param Sounds 管理する音声群。
// @param Fonts 管理するフォント群。
FAssetService::FAssetService(ITextureBackend& Textures, ISoundBackend& Sounds, IFontBackend& Fonts)
    : m_TextureLoader(Textures, m_Registry), m_SoundLoader(Sounds, m_Registry), m_FontLoader(Fonts, m_Registry)
{
}
// 所有する状態を終了し、必要なリソースを解放する。
FAssetService::~FAssetService()
{
	Shutdown();
}
// 画像を読み込みテクスチャを取得する。
// @param Path 読み込むファイルのパス。
// @param Options 処理に適用する設定。
TResult<FTexture> FAssetService::LoadTexture(const Toolbox::FString& Path, const FTextureLoadOptions& Options)
{
	if (m_Registry.IsShutdown())
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Detail::IsValidNativeString_Internal(Path))
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidArgument,
		                                  "Texture path must be nonempty UTF-8 without NUL");
	}
	// 解決した読み込みパス。
	Toolbox::FString Resolved;
	if (!ResolveRequestPath_Internal(Path, Resolved))
	{
		return TResult<FTexture>::Failure(
		    EErrorCode::InvalidArgument,
		    Toolbox::FString("Texture path cannot resolve against root ") + m_Resolver.GetRoot().ToUtf8() + ": " + Path);
	}
	// 正規化したパス。
	const Toolbox::FString Normalized = Resolved;
	// 検索または入力のキー。
	const Toolbox::FString Key = Normalized + (Options.bUse3D ? "|3d" : "|2d");
	// 再利用可能なキャッシュを取得して有効性を確認する。
	if (auto Cached = m_TextureCache.Find(Key))
	{
		return TResult<FTexture>::Success(FTexture(Toolbox::Move(Cached)));
	}
	// 処理結果。
	auto Result = m_TextureLoader.Load(Normalized, Options);
	if (Result)
	{
		m_TextureCache.Insert(Key, Result.Value().GetResource_Internal());
	}
	return Result;
}
// 音声ファイルを読み込む。
// @param Path 読み込むファイルのパス。
// @param Options 処理に適用する設定。
TResult<FSound> FAssetService::LoadSound(const Toolbox::FString& Path, const FSoundLoadOptions& Options)
{
	if (m_Registry.IsShutdown())
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Detail::IsValidNativeString_Internal(Path) ||
	    (Options.Storage != ESoundStorage::Memory && Options.Storage != ESoundStorage::Stream))
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidArgument,
		                                "Sound requires a valid storage mode and nonempty UTF-8 path without NUL");
	}
	// 解決した読み込みパス。
	Toolbox::FString Resolved;
	if (!ResolveRequestPath_Internal(Path, Resolved))
	{
		return TResult<FSound>::Failure(
		    EErrorCode::InvalidArgument,
		    Toolbox::FString("Sound path cannot resolve against root ") + m_Resolver.GetRoot().ToUtf8() + ": " + Path);
	}
	// 正規化したパス。
	const Toolbox::FString Normalized = Resolved;
	// 検索または入力のキー。
	const Toolbox::FString Key = Normalized + (Options.Storage == ESoundStorage::Memory ? "|memory" : "|stream");
	// 再利用可能なキャッシュを取得して有効性を確認する。
	if (auto Cached = m_SoundCache.Find(Key))
	{
		return TResult<FSound>::Success(FSound(Toolbox::Move(Cached)));
	}
	// 処理結果。
	auto Result = m_SoundLoader.Load(Normalized, Options);
	if (Result)
	{
		m_SoundCache.Insert(Key, Result.Value().GetResource_Internal());
	}
	return Result;
}
// 指定設定のフォントを取得する。
// @param Options 処理に適用する設定。
TResult<FFont> FAssetService::LoadFont(const FFontOptions& Options)
{
	if (m_Registry.IsShutdown())
	{
		return TResult<FFont>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	// 検索または入力のキー。
	const Toolbox::FString Key = Toolbox::ToString(Options.Family.Size()) + ":" + Options.Family + "|" +
	                             Toolbox::ToString(Options.Size) + "|" + Toolbox::ToString(Options.Thickness) +
	                             (Options.bAntialias ? "|aa" : "|plain");
	// 再利用可能なキャッシュを取得して有効性を確認する。
	if (auto Cached = m_FontCache.Find(Key))
	{
		return TResult<FFont>::Success(FFont(Toolbox::Move(Cached)));
	}
	// 処理結果。
	auto Result = m_FontLoader.Load(Options);
	if (Result)
	{
		m_FontCache.Insert(Key, Result.Value().GetResource_Internal());
	}
	return Result;
}
// 描画先として使うテクスチャを生成する。
// @param Width 幅。
// @param Height 高さ。
// @param bAlpha 透過を扱う描画先を生成するか。
TResult<FRenderTarget> FAssetService::CreateRenderTarget(Toolbox::int32 Width, Toolbox::int32 Height, bool bAlpha)
{
	return m_TextureLoader.CreateRenderTarget(Width, Height, bAlpha);
}
// 参照されていないキャッシュ項目を除去する。
void FAssetService::CollectUnused()
{
	m_Registry.CollectUnused();
	m_TextureCache.CollectUnused();
	m_SoundCache.CollectUnused();
	m_FontCache.CollectUnused();
}
// 管理する処理とリソースを順序どおり終了する。
void FAssetService::Shutdown() noexcept
{
	m_TextureCache.Clear();
	m_SoundCache.Clear();
	m_FontCache.Clear();
	m_Registry.Shutdown();
}
// ProjectRootを一度だけ設定する。以後の要求はこのRootを基準に解決する。
// @param Root sln配置先の完全修飾ディレクトリ。
bool FAssetService::SetProjectRoot(const Toolbox::FPath& Root)
{
	return m_Resolver.SetRoot(Root);
}
// 設定済みのProjectRootを返す。未設定なら空。
Toolbox::FPath FAssetService::GetProjectRoot() const
{
	return m_Resolver.GetRoot();
}
} // namespace Dxf
