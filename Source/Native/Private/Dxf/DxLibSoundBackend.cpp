#include "Dxf/DxLibSoundBackend.h"
#include "NativeApi.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
// 音声ファイルを読み込む。
// @param Path 読み込むファイルのパス。
// @param Options 処理に適用する設定。
TResult<Toolbox::int32> FDxLibSoundBackend::LoadSound(const Toolbox::FString& Path, const FSoundLoadOptions& Options)
{
	// ネイティブAPIのデータ保持方式。
	const Toolbox::int32 Type =
	    Options.Storage == ESoundStorage::Stream ? DX_SOUNDDATATYPE_FILE : DX_SOUNDDATATYPE_MEMNOPRESS;
	if (DxLib::SetCreateSoundDataType(Type) < 0)
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "SetCreateSoundDataType failed");
	}
	// ハンドル。
	const Toolbox::int32 Handle = DxLib::LoadSoundMem(Path.CStr());
	// 読み込み方式はこのアダプターが管理し、処理後は既定の方式へ戻す。
	//
	// 既定状態への復元結果。
	const Toolbox::int32 Reset = DxLib::SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);
	if (Handle < 0)
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::NotFound, "LoadSoundMem failed: " + Path);
	}
	if (Reset < 0)
	{
		DxLib::DeleteSoundMem(Handle);
		return TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "Sound loading state reset failed");
	}
	return TResult<Toolbox::int32>::Success(Handle);
}
// 独立して再生できる音声ハンドルを複製する。
// @param Handle ハンドル。
TResult<Toolbox::int32> FDxLibSoundBackend::DuplicateSound(Toolbox::int32 Handle)
{
	// 開始した音声の再生ハンドル。
	const Toolbox::int32 Voice = DxLib::DuplicateSoundMem(Handle);
	return Voice < 0 ? TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "DuplicateSoundMem failed")
	                 : TResult<Toolbox::int32>::Success(Voice);
}
// ネイティブ音声の再生を開始する。
// @param Handle ハンドル。
// @param bLoop 繰り返し再生するか。
TResult<void> FDxLibSoundBackend::StartSound(Toolbox::int32 Handle, bool bLoop)
{
	return Detail::CheckNative_Internal(DxLib::PlaySoundMem(Handle, bLoop ? DX_PLAYTYPE_LOOP : DX_PLAYTYPE_BACK, TRUE),
	                                    "PlaySoundMem failed");
}
// ネイティブ音声の再生を停止する。
// @param Handle ハンドル。
void FDxLibSoundBackend::StopSound(Toolbox::int32 Handle) noexcept
{
	if (Handle >= 0)
	{
		DxLib::StopSoundMem(Handle);
	}
}
// ネイティブ音声を解放する。
// @param Handle ハンドル。
void FDxLibSoundBackend::DeleteSound(Toolbox::int32 Handle) noexcept
{
	if (Handle >= 0)
	{
		DxLib::DeleteSoundMem(Handle);
	}
}
// ネイティブ音声の音量を設定する。
// @param Handle ハンドル。
// @param Volume 再生音量。
TResult<void> FDxLibSoundBackend::SetSoundVolume(Toolbox::int32 Handle, Toolbox::f32 Volume)
{
	if (!Toolbox::IsFinite(Volume) || Volume < 0 || Volume > 1)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Volume must be in [0,1]");
	}
	return Detail::CheckNative_Internal(
	    DxLib::ChangeVolumeSoundMem(static_cast<Toolbox::int32>(Toolbox::RoundToLong(Volume * 255.0f)), Handle),
	    "ChangeVolumeSoundMem failed");
}
// ネイティブ音声が再生中かを調べる。
// @param Handle ハンドル。
TResult<bool> FDxLibSoundBackend::IsSoundPlaying(Toolbox::int32 Handle)
{
	// 音声を再生中か。
	const Toolbox::int32 Playing = DxLib::CheckSoundMem(Handle);
	return Playing < 0 ? TResult<bool>::Failure(EErrorCode::BackendFailure, "CheckSoundMem failed")
	                   : TResult<bool>::Success(Playing != 0);
}
} // namespace Dxf
