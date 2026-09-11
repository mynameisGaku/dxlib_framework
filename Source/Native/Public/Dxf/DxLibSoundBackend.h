#pragma once
#include "Dxf/AssetBackend.h"
namespace Dxf
{
class FDxLibSoundBackend final : public ISoundBackend
{
public:
    TResult<int> LoadSound(const std::string& Path, const FSoundLoadOptions& Options) override;
    TResult<int> DuplicateSound(int Handle) override;
    TResult<void> StartSound(int Handle, bool bLoop) override;
    void StopSound(int Handle) noexcept override;
    void DeleteSound(int Handle) noexcept override;
    TResult<void> SetSoundVolume(int Handle, float Volume) override;
    TResult<bool> IsSoundPlaying(int Handle) override;
};
}
