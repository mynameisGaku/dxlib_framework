#include "Dxf/FontLoader.h"
#include "Dxf/Utf8.h"

namespace Dxf
{
TResult<FFont> FFontLoader::Load(const FFontOptions& Options)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FFont>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (Options.Size <= 0 || Options.Thickness <= 0 || !Detail::IsValidNativeString_Internal(Options.Family, true))
	{
		return TResult<FFont>::Failure(EErrorCode::InvalidArgument, "Invalid font dimensions or family name");
	}
	auto Loaded = m_pBackend->CreateFont(Options);
	if (!Loaded)
	{
		return TResult<FFont>::Failure(Loaded.Error());
	}
	FNativeHandle Handle(Loaded.Value(), m_pBackend, [](void* Context, int Value) noexcept
	{
		static_cast<IFontBackend*>(Context)->DeleteFont(Value);
	});
	if (Handle.Get() < 0)
	{
		return TResult<FFont>::Failure(EErrorCode::BackendFailure, "Invalid font handle");
	}
	auto Resource = std::make_shared<FFontResource>(std::move(Handle), Options);
	if (!m_pRegistry->Register(Resource))
	{
		return TResult<FFont>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	return TResult<FFont>::Success(FFont(std::move(Resource)));
}
}
