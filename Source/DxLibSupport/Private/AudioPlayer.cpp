#include "Dxf/AudioPlayer.h"
#include <cmath>
#include <exception>
namespace Dxf
{
namespace
{
template <typename TFunction>
auto CallSoundBackend_Internal(TFunction&& Function) -> decltype(Function())
{
	using FResult = decltype(Function());
	try
	{
		return Function();
	}
	catch (const std::exception& Error)
	{
		return FResult::Failure(EErrorCode::BackendFailure, Error.what());
	}
	catch (...)
	{
		return FResult::Failure(EErrorCode::BackendFailure, "Unknown sound backend exception");
	}
}
}
TResult<FPlaybackHandle> FAudioPlayer::Play(const FSound& Sound, const FPlaybackOptions& Options)
{
	return CallSoundBackend_Internal([&]
	{
		return Play_Internal(Sound, Options);
	});
}
TResult<FPlaybackHandle> FAudioPlayer::Play_Internal(const FSound& Sound, const FPlaybackOptions& Options)
{
	if (m_bShutdown || !Sound.IsValid())
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidState, "Audio or sound unavailable");
	}
	if (!std::isfinite(Options.Volume) || Options.Volume < 0.0f || Options.Volume > 1.0f)
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidArgument, "Volume must be in [0, 1]");
	}
	if (Sound.GetResource_Internal()->GetBackendIdentity_Internal() != m_pBackend)
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidArgument, "Sound belongs to a different backend");
	}
	const auto& Metadata = Sound.GetResource_Internal()->GetMetadata();
	auto Loaded = Metadata.Options.Storage == ESoundStorage::Memory ? m_pBackend->DuplicateSound(Sound.GetNativeHandle_Internal()) : m_pBackend->LoadSound(Metadata.Path, Metadata.Options);
	if (!Loaded)
	{
		return TResult<FPlaybackHandle>::Failure(Loaded.Error());
	}
	FNativeHandle Handle(Loaded.Value(), m_pBackend, [](void* Context, int Value) noexcept
	{
		auto* Backend = static_cast<ISoundBackend*>(Context);
		Backend->StopSound(Value);
		Backend->DeleteSound(Value);
	});
	if (Handle.Get() < 0)
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::BackendFailure, "Invalid voice allocation");
	}
	if (m_bShutdown)
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidState, "Audio stopped during allocation");
	}
	auto Volume = m_pBackend->SetSoundVolume(Handle.Get(), Options.Volume);
	if (!Volume)
	{
		return TResult<FPlaybackHandle>::Failure(Volume.Error());
	}
	if (m_bShutdown)
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidState, "Audio stopped during volume setup");
	}
	auto Started = m_pBackend->StartSound(Handle.Get(), Options.bLoop);
	if (!Started)
	{
		return TResult<FPlaybackHandle>::Failure(Started.Error());
	}
	if (m_bShutdown)
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidState, "Audio stopped during playback start");
	}
	return TResult<FPlaybackHandle>::Success(m_Playbacks.Insert(std::make_unique<DPlayback>(std::move(Handle), Options.Scope)));
}
bool FAudioPlayer::Stop(FPlaybackHandle Handle) noexcept
{
	return m_Playbacks.Remove(Handle);
}
TResult<void> FAudioPlayer::SetVolume(FPlaybackHandle Handle, float Volume)
{
	auto* Playback = m_Playbacks.Find_Internal(Handle.GetId());
	if (!Playback)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Unknown playback");
	}
	if (!std::isfinite(Volume) || Volume < 0.0f || Volume > 1.0f)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid volume");
	}
	const int NativeHandle = Playback->GetHandle_Internal();
	return CallSoundBackend_Internal([&]
	{
		return m_pBackend->SetSoundVolume(NativeHandle, Volume);
	});
}
void FAudioPlayer::StopScope(std::uint64_t Scope) noexcept
{
	m_Playbacks.RemoveIf_Internal([&](const DPlayback& Playback)
	{
		return Playback.GetScope() == Scope;
	});
}
TResult<void> FAudioPlayer::Tick()
{
	for (auto Handle : m_Playbacks.Snapshot())
	{
		if (m_bShutdown)
		{
			break;
		}
		const auto* Playback = m_Playbacks.Find_Internal(Handle.GetId());
		if (!Playback)
		{
			continue;
		}
		// A backend callback may stop this or other voices. Do not retain or
		// dereference the playback pointer after calling into the backend.
		const int NativeHandle = Playback->GetHandle_Internal();
		auto Playing = CallSoundBackend_Internal([&]
		{
			return m_pBackend->IsSoundPlaying(NativeHandle);
		});
		if (!Playing)
		{
			Stop(Handle);
			return TResult<void>::Failure(Playing.Error());
		}
		if (!Playing.Value())
		{
			Stop(Handle);
		}
	}
	return {};
}
void FAudioPlayer::Shutdown() noexcept
{
	m_bShutdown = true;
	m_Playbacks.RemoveIf_Internal([](const DPlayback&)
	{
		return true;
	});
}
}
