#include "Dxf/AssetService.h"
#include <filesystem>
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
TResult<std::shared_ptr<FTextureResource>> FTextureLoader::Adopt_Internal(FTextureAllocation Allocation, bool bRenderTarget)
{
    FNativeHandle Handle(Allocation.NativeHandle, m_pBackend, [](void* Context, int Value) noexcept { static_cast<ITextureBackend*>(Context)->DeleteTexture(Value); });
    if (Allocation.NativeHandle < 0 || Allocation.Width <= 0 || Allocation.Height <= 0)
    { return TResult<std::shared_ptr<FTextureResource>>::Failure(EErrorCode::BackendFailure, "Invalid texture allocation"); }
    auto Resource = std::make_shared<FTextureResource>(std::move(Handle), FTextureMetadata{Allocation.Width, Allocation.Height, bRenderTarget});
    if (!m_pRegistry->Register(Resource))
    { return TResult<std::shared_ptr<FTextureResource>>::Failure(EErrorCode::InvalidState, "Resource registry stopped"); }
    return TResult<std::shared_ptr<FTextureResource>>::Success(std::move(Resource));
}
TResult<FTexture> FTextureLoader::Load(const std::string& Path, const FTextureLoadOptions& Options)
{
    if (m_pRegistry->IsShutdown()) { return TResult<FTexture>::Failure(EErrorCode::InvalidState, "Assets stopped"); }
    auto Allocation = m_pBackend->LoadTexture(Path, Options);
    if (!Allocation) { return TResult<FTexture>::Failure(Allocation.Error()); }
    auto Resource = Adopt_Internal(Allocation.Value(), false);
    return Resource ? TResult<FTexture>::Success(FTexture(std::move(Resource).Value())) : TResult<FTexture>::Failure(Resource.Error());
}
TResult<FRenderTarget> FTextureLoader::CreateRenderTarget(int Width, int Height, bool bAlpha)
{
    if (m_pRegistry->IsShutdown()) { return TResult<FRenderTarget>::Failure(EErrorCode::InvalidState, "Assets stopped"); }
    if (Width <= 0 || Height <= 0) { return TResult<FRenderTarget>::Failure(EErrorCode::InvalidArgument, "Invalid target size"); }
    auto Allocation = m_pBackend->CreateRenderTarget(Width, Height, bAlpha);
    if (!Allocation) { return TResult<FRenderTarget>::Failure(Allocation.Error()); }
    auto Resource = Adopt_Internal(Allocation.Value(), true);
    return Resource ? TResult<FRenderTarget>::Success(FRenderTarget(std::move(Resource).Value())) : TResult<FRenderTarget>::Failure(Resource.Error());
}
TResult<FSound> FSoundLoader::Load(const std::string& Path, const FSoundLoadOptions& Options)
{
    if (m_pRegistry->IsShutdown()) { return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped"); }
    auto Loaded = m_pBackend->LoadSound(Path, Options);
    if (!Loaded) { return TResult<FSound>::Failure(Loaded.Error()); }
    FNativeHandle Handle(Loaded.Value(), m_pBackend, [](void* Context, int Value) noexcept { static_cast<ISoundBackend*>(Context)->DeleteSound(Value); });
    if (Handle.Get() < 0) { return TResult<FSound>::Failure(EErrorCode::BackendFailure, "Invalid sound handle"); }
    auto Resource = std::make_shared<FSoundResource>(std::move(Handle), FSoundMetadata{Path, Options});
    if (!m_pRegistry->Register(Resource)) { return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped"); }
    return TResult<FSound>::Success(FSound(std::move(Resource)));
}
TResult<FFont> FFontLoader::Load(const FFontOptions& Options)
{
    if (m_pRegistry->IsShutdown()) { return TResult<FFont>::Failure(EErrorCode::InvalidState, "Assets stopped"); }
    if (Options.Size <= 0 || Options.Thickness <= 0) { return TResult<FFont>::Failure(EErrorCode::InvalidArgument, "Invalid font dimensions"); }
    auto Loaded = m_pBackend->CreateFont(Options);
    if (!Loaded) { return TResult<FFont>::Failure(Loaded.Error()); }
    FNativeHandle Handle(Loaded.Value(), m_pBackend, [](void* Context, int Value) noexcept { static_cast<IFontBackend*>(Context)->DeleteFont(Value); });
    if (Handle.Get() < 0) { return TResult<FFont>::Failure(EErrorCode::BackendFailure, "Invalid font handle"); }
    auto Resource = std::make_shared<FFontResource>(std::move(Handle), Options);
    if (!m_pRegistry->Register(Resource)) { return TResult<FFont>::Failure(EErrorCode::InvalidState, "Assets stopped"); }
    return TResult<FFont>::Success(FFont(std::move(Resource)));
}
FAssetService::FAssetService(ITextureBackend& Textures, ISoundBackend& Sounds, IFontBackend& Fonts)
    : m_TextureLoader(Textures, m_Registry), m_SoundLoader(Sounds, m_Registry), m_FontLoader(Fonts, m_Registry) {}
FAssetService::~FAssetService() { Shutdown(); }
TResult<FTexture> FAssetService::LoadTexture(const std::string& Path, const FTextureLoadOptions& Options)
{
    if (m_Registry.IsShutdown()) { return TResult<FTexture>::Failure(EErrorCode::InvalidState, "Assets stopped"); }
    if (Path.empty()) { return TResult<FTexture>::Failure(EErrorCode::InvalidArgument, "Empty texture path"); }
    const std::string Normalized = NormalizePath_Internal(Path);
    const std::string Key = Normalized + (Options.bUse3D ? "|3d" : "|2d");
    if (auto Cached = m_TextureCache.Find(Key)) { return TResult<FTexture>::Success(FTexture(std::move(Cached))); }
    auto Result = m_TextureLoader.Load(Normalized, Options);
    if (Result) { m_TextureCache.Insert(Key, Result.Value().GetResource_Internal()); }
    return Result;
}
TResult<FSound> FAssetService::LoadSound(const std::string& Path, const FSoundLoadOptions& Options)
{
    if (m_Registry.IsShutdown()) { return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped"); }
    if (Path.empty()) { return TResult<FSound>::Failure(EErrorCode::InvalidArgument, "Empty sound path"); }
    const std::string Normalized = NormalizePath_Internal(Path);
    const std::string Key = Normalized + (Options.Storage == ESoundStorage::Memory ? "|memory" : "|stream");
    if (auto Cached = m_SoundCache.Find(Key)) { return TResult<FSound>::Success(FSound(std::move(Cached))); }
    auto Result = m_SoundLoader.Load(Normalized, Options);
    if (Result) { m_SoundCache.Insert(Key, Result.Value().GetResource_Internal()); }
    return Result;
}
TResult<FFont> FAssetService::LoadFont(const FFontOptions& Options)
{
    if (m_Registry.IsShutdown()) { return TResult<FFont>::Failure(EErrorCode::InvalidState, "Assets stopped"); }
    const std::string Key = std::to_string(Options.Family.size()) + ":" + Options.Family + "|" + std::to_string(Options.Size) + "|" + std::to_string(Options.Thickness) + (Options.bAntialias ? "|aa" : "|plain");
    if (auto Cached = m_FontCache.Find(Key)) { return TResult<FFont>::Success(FFont(std::move(Cached))); }
    auto Result = m_FontLoader.Load(Options);
    if (Result) { m_FontCache.Insert(Key, Result.Value().GetResource_Internal()); }
    return Result;
}
TResult<FRenderTarget> FAssetService::CreateRenderTarget(int Width, int Height, bool bAlpha)
{ return m_TextureLoader.CreateRenderTarget(Width, Height, bAlpha); }
void FAssetService::CollectUnused()
{ m_Registry.CollectUnused(); m_TextureCache.CollectUnused(); m_SoundCache.CollectUnused(); m_FontCache.CollectUnused(); }
void FAssetService::Shutdown() noexcept
{ m_TextureCache.Clear(); m_SoundCache.Clear(); m_FontCache.Clear(); m_Registry.Shutdown(); }
}
