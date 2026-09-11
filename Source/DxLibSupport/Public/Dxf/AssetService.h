#pragma once
#include "Dxf/TextureLoader.h"
#include "Dxf/SoundLoader.h"
#include "Dxf/FontLoader.h"
#include "Dxf/ResourceCache.h"
namespace Dxf
{
using FTextureCache = TResourceCache<FTextureResource>;
using FSoundCache = TResourceCache<FSoundResource>;
using FFontCache = TResourceCache<FFontResource>;
/** Single-thread service. Backends must outlive this object and its Shutdown(). */
class FAssetService
{
public:
	FAssetService(ITextureBackend& Textures, ISoundBackend& Sounds, IFontBackend& Fonts);
	~FAssetService();
	FAssetService(const FAssetService&) = delete;
	FAssetService& operator=(const FAssetService&) = delete;
	TResult<FTexture> LoadTexture(const std::string& Path, const FTextureLoadOptions& Options = {});
	TResult<FSound> LoadSound(const std::string& Path, const FSoundLoadOptions& Options = {});
	TResult<FFont> LoadFont(const FFontOptions& Options = {});
	TResult<FRenderTarget> CreateRenderTarget(int Width, int Height, bool bAlpha = true);
	void CollectUnused();
	void Shutdown() noexcept;
private:
	FResourceRegistry m_Registry;
	FTextureLoader m_TextureLoader;
	FSoundLoader m_SoundLoader;
	FFontLoader m_FontLoader;
	FTextureCache m_TextureCache;
	FSoundCache m_SoundCache;
	FFontCache m_FontCache;
};
}
