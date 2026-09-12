#include "Toolbox/UniquePtr.h"
#include "Dxf/AudioPlayer.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
namespace
{
// 音声バックエンド処理を呼び出す。
template <typename TFunction> auto CallSoundBackend_Internal(TFunction&& Function) -> decltype(Function())
{
	// 呼び出し先が返す処理結果の型。
	using FResult = decltype(Function());
	try
	{
		return Function();
	}
	// 呼び出し先の例外を処理結果へ変換する。
	catch (const Toolbox::FException& Error)
	{
		return FResult::Failure(EErrorCode::BackendFailure, Error.What());
	}
	catch (...)
	{
		return FResult::Failure(EErrorCode::BackendFailure, "Unknown sound backend exception");
	}
}
} // namespace
// 指定した音声の再生を開始する。
// @param Sound 再生する音声。
// @param Options 処理に適用する設定。
TResult<FPlaybackHandle> FAudioPlayer::Play(const FSound& Sound, const FPlaybackOptions& Options)
{
	return CallSoundBackend_Internal(
	    [&]
	    {
		    return Play_Internal(Sound, Options);
	    });
}
// 指定した音声の再生を開始する。
// @param Sound 再生する音声。
// @param Options 処理に適用する設定。
TResult<FPlaybackHandle> FAudioPlayer::Play_Internal(const FSound& Sound, const FPlaybackOptions& Options)
{
	if (m_bShutdown || !Sound.IsValid())
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidState, "Audio or sound unavailable");
	}
	if (!Toolbox::IsFinite(Options.Volume) || Options.Volume < 0.0f || Options.Volume > 1.0f)
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidArgument, "Volume must be in [0, 1]");
	}
	if (Sound.GetResource_Internal()->GetBackendIdentity_Internal() != m_pBackend)
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidArgument, "Sound belongs to a different backend");
	}
	// リソースの付随情報。
	const auto& Metadata = Sound.GetResource_Internal()->GetMetadata();
	// リソースの読み込み結果。
	auto Loaded = Metadata.Options.Storage == ESoundStorage::Memory
	                  ? m_pBackend->DuplicateSound(Sound.GetNativeHandle_Internal())
	                  : m_pBackend->LoadSound(Metadata.Path, Metadata.Options);
	if (!Loaded)
	{
		return TResult<FPlaybackHandle>::Failure(Loaded.Error());
	}
	// ネイティブハンドルの解放を保証する所有者。
	FNativeHandle Handle(Loaded.Value(), m_pBackend,
	                     [](void* Context, Toolbox::int32 Value) noexcept
	                     {
		                     // ネイティブ処理の呼び出し先。
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
	// 再生音量。
	auto Volume = m_pBackend->SetSoundVolume(Handle.Get(), Options.Volume);
	if (!Volume)
	{
		return TResult<FPlaybackHandle>::Failure(Volume.Error());
	}
	if (m_bShutdown)
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidState, "Audio stopped during volume setup");
	}
	// 開始処理が完了しているか。
	auto Started = m_pBackend->StartSound(Handle.Get(), Options.bLoop);
	if (!Started)
	{
		return TResult<FPlaybackHandle>::Failure(Started.Error());
	}
	if (m_bShutdown)
	{
		return TResult<FPlaybackHandle>::Failure(EErrorCode::InvalidState, "Audio stopped during playback start");
	}
	return TResult<FPlaybackHandle>::Success(
	    m_Playbacks.Insert(Toolbox::MakeUnique<DPlayback>(Toolbox::Move(Handle), Options.Scope)));
}
// 対象の音声再生を停止する。
// @param Handle ハンドル。
bool FAudioPlayer::Stop(FPlaybackHandle Handle) noexcept
{
	return m_Playbacks.Remove(Handle);
}
// 再生音量を設定する。
// @param Handle ハンドル。
// @param Volume 再生音量。
TResult<void> FAudioPlayer::SetVolume(FPlaybackHandle Handle, Toolbox::f32 Volume)
{
	// 音声の再生状態。
	auto* Playback = m_Playbacks.Find_Internal(Handle.GetId());
	if (!Playback)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Unknown playback");
	}
	if (!Toolbox::IsFinite(Volume) || Volume < 0.0f || Volume > 1.0f)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid volume");
	}
	// ネイティブAPIのリソース識別値。
	const Toolbox::int32 NativeHandle = Playback->GetHandle_Internal();
	return CallSoundBackend_Internal(
	    [&]
	    {
		    return m_pBackend->SetSoundVolume(NativeHandle, Volume);
	    });
}
// 同じスコープに属する再生音声を停止する。
// @param Scope 再生音声をまとめる識別番号。
void FAudioPlayer::StopScope(Toolbox::uint64 Scope) noexcept
{
	m_Playbacks.RemoveIf_Internal(
	    [&](const DPlayback& Playback)
	    {
		    return Playback.GetScope() == Scope;
	    });
}
// 更新対象へフレーム更新を通知する。
TResult<void> FAudioPlayer::Tick()
{
	// ハンドルを順に処理する。
	for (auto Handle : m_Playbacks.Snapshot())
	{
		if (m_bShutdown)
		{
			break;
		}
		// 音声の再生状態。
		const auto* Playback = m_Playbacks.Find_Internal(Handle.GetId());
		if (!Playback)
		{
			continue;
		}
		// バックエンドのコールバックが音声を停止する場合があるため、呼び出し後は再生状態のポインターを参照しない。
		//
		// ネイティブAPIのリソース識別値。
		const Toolbox::int32 NativeHandle = Playback->GetHandle_Internal();
		// 音声を再生中か。
		auto Playing = CallSoundBackend_Internal(
		    [&]
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
// 管理する処理とリソースを順序どおり終了する。
void FAudioPlayer::Shutdown() noexcept
{
	m_bShutdown = true;
	m_Playbacks.RemoveIf_Internal(
	    [](const DPlayback&)
	    {
		    return true;
	    });
}
} // namespace Dxf
