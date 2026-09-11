#include "Dxf/TextureLoader.h"

namespace Dxf
{
TResult<std::shared_ptr<FTextureResource>> FTextureLoader::Adopt_Internal(FTextureAllocation Allocation, bool bRenderTarget)
{
	FNativeHandle Handle(Allocation.NativeHandle, m_pBackend, [](void* Context, int Value) noexcept
	{
		static_cast<ITextureBackend*>(Context)->DeleteTexture(Value);
	});
	if (Allocation.NativeHandle < 0 || Allocation.Width <= 0 || Allocation.Height <= 0)
	{
		return TResult<std::shared_ptr<FTextureResource>>::Failure(EErrorCode::BackendFailure, "Invalid texture allocation");
	}
	auto Resource = std::make_shared<FTextureResource>(std::move(Handle), FTextureMetadata{Allocation.Width, Allocation.Height, bRenderTarget});
	if (!m_pRegistry->Register(Resource))
	{
		return TResult<std::shared_ptr<FTextureResource>>::Failure(EErrorCode::InvalidState, "Resource registry stopped");
	}
	return TResult<std::shared_ptr<FTextureResource>>::Success(std::move(Resource));
}
TResult<FTexture> FTextureLoader::Load(const std::string& Path, const FTextureLoadOptions& Options)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	auto Allocation = m_pBackend->LoadTexture(Path, Options);
	if (!Allocation)
	{
		return TResult<FTexture>::Failure(Allocation.Error());
	}
	auto Resource = Adopt_Internal(Allocation.Value(), false);
	return Resource ? TResult<FTexture>::Success(FTexture(std::move(Resource).Value())) : TResult<FTexture>::Failure(Resource.Error());
}
TResult<FRenderTarget> FTextureLoader::CreateRenderTarget(int Width, int Height, bool bAlpha)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FRenderTarget>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (Width <= 0 || Height <= 0)
	{
		return TResult<FRenderTarget>::Failure(EErrorCode::InvalidArgument, "Invalid target size");
	}
	auto Allocation = m_pBackend->CreateRenderTarget(Width, Height, bAlpha);
	if (!Allocation)
	{
		return TResult<FRenderTarget>::Failure(Allocation.Error());
	}
	auto Resource = Adopt_Internal(Allocation.Value(), true);
	return Resource ? TResult<FRenderTarget>::Success(FRenderTarget(std::move(Resource).Value())) : TResult<FRenderTarget>::Failure(Resource.Error());
}
}
