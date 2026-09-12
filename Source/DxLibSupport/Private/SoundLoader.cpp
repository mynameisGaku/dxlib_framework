#include "Dxf/SoundLoader.h"
#include "Dxf/Utf8.h"

namespace Dxf
{
TResult<FSound> FSoundLoader::Load(const std::string& Path, const FSoundLoadOptions& Options)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Detail::IsValidNativeString_Internal(Path))
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidArgument, "Path must be nonempty UTF-8 without NUL");
	}
	if (Options.Storage != ESoundStorage::Memory && Options.Storage != ESoundStorage::Stream)
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidArgument, "Invalid sound storage mode");
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
