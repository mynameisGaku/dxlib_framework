#pragma once
#include "Dxf/AssetBackend.h"
namespace Dxf
{
class FDxLibTextureBackend final : public ITextureBackend
{
public:
    TResult<FTextureAllocation> LoadTexture(const std::string& Path, const FTextureLoadOptions& Options) override;
    TResult<FTextureAllocation> CreateRenderTarget(int Width, int Height, bool bAlpha) override;
    void DeleteTexture(int Handle) noexcept override;
};
}
