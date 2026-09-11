#pragma once
#include "Dxf/Result.h"
namespace Dxf
{
struct FTextureLoadOptions
{
    bool bUse3D = true;
};
struct FTextureAllocation
{
    int NativeHandle = -1;
    int Width = 0;
    int Height = 0;
};
enum class ESoundStorage { Memory, Stream };
struct FSoundLoadOptions
{
    ESoundStorage Storage = ESoundStorage::Memory;
};
struct FFontOptions
{
    std::string Family = "Meiryo";
    int Size = 20;
    int Thickness = 4;
    bool bAntialias = true;
};
class ITextureBackend
{
public:
    virtual ~ITextureBackend() = default;
    virtual TResult<FTextureAllocation> LoadTexture(const std::string& Path, const FTextureLoadOptions& Options) = 0;
    virtual TResult<FTextureAllocation> CreateRenderTarget(int Width, int Height, bool bAlpha) = 0;
    virtual void DeleteTexture(int Handle) noexcept = 0;
};
class IFontBackend
{
public:
    virtual ~IFontBackend() = default;
    virtual TResult<int> CreateFont(const FFontOptions& Options) = 0;
    virtual void DeleteFont(int Handle) noexcept = 0;
};
class ISoundBackend
{
public:
    virtual ~ISoundBackend() = default;
    virtual TResult<int> LoadSound(const std::string& Path, const FSoundLoadOptions& Options) = 0;
    virtual TResult<int> DuplicateSound(int Handle) = 0;
    virtual TResult<void> StartSound(int Handle, bool bLoop) = 0;
    virtual void StopSound(int Handle) noexcept = 0;
    virtual void DeleteSound(int Handle) noexcept = 0;
    virtual TResult<void> SetSoundVolume(int Handle, float Volume) = 0;
    virtual TResult<bool> IsSoundPlaying(int Handle) = 0;
};
}
