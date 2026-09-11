#include "Dxf/SoundLoader.h"

namespace Dxf
{
TResult<FSound> FSoundLoader::Load(const std::string& Path, const FSoundLoadOptions& Options)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	auto Loaded = m_pBackend->LoadSound(Path, Options);
	if (!Loaded)
	{
		return TResult<FSound>::Failure(Loaded.Error());
	}
	FNativeHandle Handle(Loaded.Value(), m_pBackend, [](void* Context, int Value) noexcept
	{
		static_cast<ISoundBackend*>(Context)->DeleteSound(Value);
	});
	if (Handle.Get() < 0)
	{
		return TResult<FSound>::Failure(EErrorCode::BackendFailure, "Invalid sound handle");
	}
	auto Resource = std::make_shared<FSoundResource>(std::move(Handle), FSoundMetadata{Path, Options});
	if (!m_pRegistry->Register(Resource))
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	return TResult<FSound>::Success(FSound(std::move(Resource)));
}
}
