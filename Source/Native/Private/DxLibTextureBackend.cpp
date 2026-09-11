#include "Dxf/DxLibTextureBackend.h"
#include "NativeApi.h"
namespace Dxf
{
TResult<FTextureAllocation> FDxLibTextureBackend::LoadTexture(const std::string& Path, const FTextureLoadOptions& Options)
{
    const int Handle = DxLib::LoadGraph(Path.c_str(), Options.bUse3D ? FALSE : TRUE);
    if (Handle < 0)
    {
        return TResult<FTextureAllocation>::Failure(EErrorCode::NotFound, "LoadGraph failed: " + Path);
    }
    int Width = 0, Height = 0;
    if (DxLib::GetGraphSize(Handle, &Width, &Height) < 0 || Width <= 0 || Height <= 0)
    {
        DxLib::DeleteGraph(Handle);
        return TResult<FTextureAllocation>::Failure(EErrorCode::BackendFailure, "GetGraphSize failed: " + Path);
    }
    return TResult<FTextureAllocation>::Success({Handle, Width, Height});
}
TResult<FTextureAllocation> FDxLibTextureBackend::CreateRenderTarget(int Width, int Height, bool bAlpha)
{
    if (Width <= 0 || Height <= 0)
    {
        return TResult<FTextureAllocation>::Failure(EErrorCode::InvalidArgument, "Invalid render target dimensions");
    }
    const int Handle = DxLib::MakeScreen(Width, Height, bAlpha ? TRUE : FALSE);
    if (Handle < 0)
    {
        return TResult<FTextureAllocation>::Failure(EErrorCode::BackendFailure, "MakeScreen failed");
    }
    return TResult<FTextureAllocation>::Success({Handle, Width, Height});
}
void FDxLibTextureBackend::DeleteTexture(int Handle) noexcept
{
    if (Handle >= 0) { DxLib::DeleteGraph(Handle); }
}
}
