#include "Dxf/DxLibSoundBackend.h"
#include "NativeApi.h"
#include <cmath>
namespace Dxf
{
TResult<int> FDxLibSoundBackend::LoadSound(const std::string& Path, const FSoundLoadOptions& Options)
{
    const int Type = Options.Storage == ESoundStorage::Stream ? DX_SOUNDDATATYPE_FILE : DX_SOUNDDATATYPE_MEMNOPRESS;
    if (DxLib::SetCreateSoundDataType(Type) < 0)
    {
        return TResult<int>::Failure(EErrorCode::BackendFailure, "SetCreateSoundDataType failed");
    }
    const int Handle = DxLib::LoadSoundMem(Path.c_str());
    // This adapter owns the loading policy. Reset to its documented baseline, not an unknown external setting.
    const int Reset = DxLib::SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);
    if (Handle < 0)
    {
        return TResult<int>::Failure(EErrorCode::NotFound, "LoadSoundMem failed: " + Path);
    }
    if (Reset < 0)
    {
        DxLib::DeleteSoundMem(Handle);
        return TResult<int>::Failure(EErrorCode::BackendFailure, "Sound loading state reset failed");
    }
    return TResult<int>::Success(Handle);
}
TResult<int> FDxLibSoundBackend::DuplicateSound(int Handle)
{
    const int Voice = DxLib::DuplicateSoundMem(Handle);
    return Voice < 0 ? TResult<int>::Failure(EErrorCode::BackendFailure, "DuplicateSoundMem failed") : TResult<int>::Success(Voice);
}
TResult<void> FDxLibSoundBackend::StartSound(int Handle, bool bLoop)
{
    return Detail::CheckNative_Internal(DxLib::PlaySoundMem(Handle, bLoop ? DX_PLAYTYPE_LOOP : DX_PLAYTYPE_BACK, TRUE), "PlaySoundMem failed");
}
void FDxLibSoundBackend::StopSound(int Handle) noexcept
{
    if (Handle >= 0) { DxLib::StopSoundMem(Handle); }
}
void FDxLibSoundBackend::DeleteSound(int Handle) noexcept
{
    if (Handle >= 0) { DxLib::DeleteSoundMem(Handle); }
}
TResult<void> FDxLibSoundBackend::SetSoundVolume(int Handle, float Volume)
{
    if (!std::isfinite(Volume) || Volume < 0 || Volume > 1)
    {
        return TResult<void>::Failure(EErrorCode::InvalidArgument, "Volume must be in [0,1]");
    }
    return Detail::CheckNative_Internal(DxLib::ChangeVolumeSoundMem(static_cast<int>(std::lround(Volume * 255.0f)), Handle), "ChangeVolumeSoundMem failed");
}
TResult<bool> FDxLibSoundBackend::IsSoundPlaying(int Handle)
{
    const int Playing = DxLib::CheckSoundMem(Handle);
    return Playing < 0 ? TResult<bool>::Failure(EErrorCode::BackendFailure, "CheckSoundMem failed") : TResult<bool>::Success(Playing != 0);
}
}
