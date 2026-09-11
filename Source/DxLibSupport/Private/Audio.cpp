#include "Dxf/AudioPlayer.h"
#include <cmath>
namespace Dxf
{
TResult<FPlaybackHandle> FAudioPlayer::Play(const FSound& Sound, const FPlaybackOptions& Options)
{
    if (m_bShutdown || !Sound.IsValid()) { return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidState, "Audio or sound unavailable"); }
    if (!std::isfinite(Options.Volume) || Options.Volume < 0.0f || Options.Volume > 1.0f)
    { return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidArgument, "Volume must be in [0, 1]"); }
    const auto& Metadata = Sound.GetResource_Internal()->GetMetadata();
    auto Loaded = Metadata.Options.Storage == ESoundStorage::Memory ? m_pBackend->DuplicateSound(Sound.GetNativeHandle_Internal()) : m_pBackend->LoadSound(Metadata.Path, Metadata.Options);
    if (!Loaded) { return TResult<FPlaybackHandle>::Failure(Loaded.Error()); }
    FNativeHandle Handle(Loaded.Value(), [Backend = m_pBackend](int Value) { Backend->StopSound(Value); Backend->DeleteSound(Value); });
    if (Handle.Get() < 0) { return TResult<FPlaybackHandle>::Failure(EErrorCode::BackendFailure, "Invalid voice allocation"); }
    auto Volume = m_pBackend->SetSoundVolume(Handle.Get(), Options.Volume);
    if (!Volume) { return TResult<FPlaybackHandle>::Failure(Volume.Error()); }
    auto Started = m_pBackend->StartSound(Handle.Get(), Options.bLoop);
    if (!Started) { return TResult<FPlaybackHandle>::Failure(Started.Error()); }
    return TResult<FPlaybackHandle>::Success(m_Playbacks.Insert(std::make_unique<DPlayback>(std::move(Handle), Options.Scope)));
}
bool FAudioPlayer::Stop(FPlaybackHandle Handle) noexcept { return m_Playbacks.Remove(Handle); }
TResult<void> FAudioPlayer::SetVolume(FPlaybackHandle Handle, float Volume)
{
    auto* Playback = m_Playbacks.Find_Internal(Handle.GetId());
    if (!Playback) { return TResult<void>::Failure(EErrorCode::InvalidArgument, "Unknown playback"); }
    if (!std::isfinite(Volume) || Volume < 0.0f || Volume > 1.0f) { return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid volume"); }
    return m_pBackend->SetSoundVolume(Playback->GetHandle_Internal(), Volume);
}
void FAudioPlayer::StopScope(std::uint64_t Scope) noexcept
{
    m_Playbacks.RemoveIf_Internal([&](const DPlayback& Playback) { return Playback.GetScope() == Scope; });
}
TResult<void> FAudioPlayer::Tick()
{
    for (auto Handle : m_Playbacks.Snapshot())
    {
        auto Playing = m_pBackend->IsSoundPlaying(Handle.Get()->GetHandle_Internal());
        if (!Playing) { Stop(Handle); return TResult<void>::Failure(Playing.Error()); }
        if (!Playing.Value()) { Stop(Handle); }
    }
    return {};
}
void FAudioPlayer::Shutdown() noexcept { m_bShutdown = true; m_Playbacks.RemoveIf_Internal([](const DPlayback&) { return true; }); }
}
