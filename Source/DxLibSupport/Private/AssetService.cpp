#include "Dxf/AssetService.h"
#include <filesystem>
#include "Dxf/Utf8.h"
namespace Dxf
{
namespace
{
std::string NormalizePath_Internal(const std::string& Path)
{
	auto Normalized = std::filesystem::path(std::u8string(Path.begin(), Path.end())).lexically_normal().generic_u8string();
	return std::string(reinterpret_cast<const char*>(Normalized.data()), Normalized.size());
}
}
FAssetService::FAssetService(ITextureBackend& Textures, ISoundBackend& Sounds, IFontBackend& Fonts)
	: m_TextureLoader(Textures, m_Registry), m_SoundLoader(Sounds, m_Registry), m_FontLoader(Fonts, m_Registry)
{
}
FAssetService::~FAssetService()
{
	Shutdown();
}
TResult<FTexture> FAssetService::LoadTexture(const std::string& Path, const FTextureLoadOptions& Options)
{
	if (m_Registry.IsShutdown())
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Detail::IsValidNativeString_Internal(Path))
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidArgument, "Texture path must be nonempty UTF-8 without NUL");
	}
	const std::string Normalized = NormalizePath_Internal(Path);
	const std::string Key = Normalized + (Options.bUse3D ? "|3d" : "|2d");
	if (auto Cached = m_TextureCache.Find(Key))
	{
		return TResult<FTexture>::Success(FTexture(std::move(Cached)));
	}
	auto Result = m_TextureLoader.Load(Normalized, Options);
	if (Result)
	{
		m_TextureCache.Insert(Key, Result.Value().GetResource_Internal());
	}
	return Result;
}
TResult<FSound> FAssetService::LoadSound(const std::string& Path, const FSoundLoadOptions& Options)
{
	if (m_Registry.IsShutdown())
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Detail::IsValidNativeString_Internal(Path))
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidArgument, "Sound path must be nonempty UTF-8 without NUL");
	}
	const std::string Normalized = NormalizePath_Internal(Path);
	const std::string Key = Normalized + (Options.Storage == ESoundStorage::Memory ? "|memory" : "|stream");
	if (auto Cached = m_SoundCache.Find(Key))
	{
		return TResult<FSound>::Success(FSound(std::move(Cached)));
	}
	auto Result = m_SoundLoader.Load(Normalized, Options);
	if (Result)
	{
		m_SoundCache.Insert(Key, Result.Value().GetResource_Internal());
	}
	return Result;
}
TResult<FFont> FAssetService::LoadFont(const FFontOptions& Options)
{
	if (m_Registry.IsShutdown())
	{
		return TResult<FFont>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	const std::string Key = std::to_string(Options.Family.size()) + ":" + Options.Family + "|" + std::to_string(Options.Size) + "|" + std::to_string(Options.Thickness) + (Options.bAntialias ? "|aa" : "|plain");
	if (auto Cached = m_FontCache.Find(Key))
	{
		return TResult<FFont>::Success(FFont(std::move(Cached)));
	}
	auto Result = m_FontLoader.Load(Options);
	if (Result)
	{
		m_FontCache.Insert(Key, Result.Value().GetResource_Internal());
	}
	return Result;
}
TResult<FRenderTarget> FAssetService::CreateRenderTarget(int Width, int Height, bool bAlpha)
{
	return m_TextureLoader.CreateRenderTarget(Width, Height, bAlpha);
}
void FAssetService::CollectUnused()
{
	m_Registry.CollectUnused();
	m_TextureCache.CollectUnused();
	m_SoundCache.CollectUnused();
	m_FontCache.CollectUnused();
}
void FAssetService::Shutdown() noexcept
{
	m_TextureCache.Clear();
	m_SoundCache.Clear();
	m_FontCache.Clear();
	m_Registry.Shutdown();
}
}
