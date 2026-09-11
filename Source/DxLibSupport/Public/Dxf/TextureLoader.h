#pragma once
#include "Dxf/AssetBackend.h"
#include "Dxf/Texture.h"
namespace Dxf
{
class FTextureLoader
{
public:
    FTextureLoader(ITextureBackend& Backend, FResourceRegistry& Registry) : m_pBackend(&Backend), m_pRegistry(&Registry) {}
    TResult<FTexture> Load(const std::string& Path, const FTextureLoadOptions& Options);
    TResult<FRenderTarget> CreateRenderTarget(int Width, int Height, bool bAlpha);
private:
    TResult<std::shared_ptr<FTextureResource>> Adopt_Internal(FTextureAllocation Allocation, bool bRenderTarget);
    ITextureBackend* m_pBackend;
    FResourceRegistry* m_pRegistry;
};
}
