#pragma once
#include "Dxf/AssetBackend.h"
#include "Dxf/RenderBackend.h"
#include "Dxf/Platform.h"
#include "Dxf/Input.h"
#include <map>
#include <set>
#include <vector>
#include <functional>
namespace Dxf::Testing
{
struct FBackendTrace
{
    int NextHandle = 1;
    int TextureLoads = 0;
    int SoundLoads = 0;
    int Clones = 0;
    int Presentations = 0;
    int Resets = 0;
    int CurrentTarget = -1;
    bool bFailTexture = false;
    bool bInvalidTexture = false;
    bool bFailSound = false;
    bool bFailStart = false;
    bool bFailDraw = false;
    bool bFailPlatform = false;
    bool bFailTarget = false;
    bool bQuit = false;
    bool bFailInput = false;
    std::set<int> Textures;
    std::set<int> Fonts;
    std::map<int, bool> Sounds;
    std::map<int, float> Volumes;
    std::vector<int> DeletedTextures;
    std::vector<int> DeletedSounds;
    std::vector<int> DeletedFonts;
    std::vector<int> DrawHandles;
    std::vector<float> Opacities;
    std::vector<std::string> Events;
    std::function<void()> OnDraw;
    FRawInput Input;
};
class FFakeBackend final : public ITextureBackend, public ISoundBackend, public IFontBackend,
    public IRenderBackend, public IPlatform, public IInputSource
{
public:
    FBackendTrace& GetTrace() noexcept { return m_Trace; }
    const FBackendTrace& GetTrace() const noexcept { return m_Trace; }
    TResult<FTextureAllocation> LoadTexture(const std::string&, const FTextureLoadOptions&) override
    {
        ++m_Trace.TextureLoads;
        if (m_Trace.bFailTexture) { return TResult<FTextureAllocation>::Failure(EErrorCode::NotFound, "missing texture"); }
        if (m_Trace.bInvalidTexture) { return TResult<FTextureAllocation>::Success({-1, 64, 64}); }
        int Handle = m_Trace.NextHandle++; m_Trace.Textures.insert(Handle);
        return TResult<FTextureAllocation>::Success({Handle, 64, 64});
    }
    TResult<FTextureAllocation> CreateRenderTarget(int Width, int Height, bool) override
    {
        int Handle = m_Trace.NextHandle++; m_Trace.Textures.insert(Handle);
        return TResult<FTextureAllocation>::Success({Handle, Width, Height});
    }
    void DeleteTexture(int Handle) noexcept override
    { m_Trace.Textures.erase(Handle); m_Trace.DeletedTextures.push_back(Handle); m_Trace.Events.push_back("delete-texture"); }
    TResult<int> LoadSound(const std::string&, const FSoundLoadOptions&) override
    {
        ++m_Trace.SoundLoads;
        if (m_Trace.bFailSound) { return TResult<int>::Failure(EErrorCode::NotFound, "missing sound"); }
        int Handle = m_Trace.NextHandle++; m_Trace.Sounds[Handle] = false; return TResult<int>::Success(Handle);
    }
    TResult<int> DuplicateSound(int) override
    { ++m_Trace.Clones; int Handle = m_Trace.NextHandle++; m_Trace.Sounds[Handle] = false; return TResult<int>::Success(Handle); }
    TResult<void> StartSound(int Handle, bool) override
    {
        if (m_Trace.bFailStart) { return TResult<void>::Failure(EErrorCode::BackendFailure, "play failed"); }
        m_Trace.Sounds[Handle] = true; return {};
    }
    void StopSound(int Handle) noexcept override { m_Trace.Sounds[Handle] = false; }
    void DeleteSound(int Handle) noexcept override
    { m_Trace.Sounds.erase(Handle); m_Trace.DeletedSounds.push_back(Handle); m_Trace.Events.push_back("delete-sound"); }
    TResult<void> SetSoundVolume(int Handle, float Volume) override { m_Trace.Volumes[Handle] = Volume; return {}; }
    TResult<bool> IsSoundPlaying(int Handle) override { return TResult<bool>::Success(m_Trace.Sounds.at(Handle)); }
    TResult<int> CreateFont(const FFontOptions&) override
    { int Handle = m_Trace.NextHandle++; m_Trace.Fonts.insert(Handle); return TResult<int>::Success(Handle); }
    void DeleteFont(int Handle) noexcept override { m_Trace.Fonts.erase(Handle); m_Trace.DeletedFonts.push_back(Handle); }
    TResult<void> SetTarget(int Handle, int, int) override
    {
        m_Trace.CurrentTarget = Handle;
        if (m_Trace.bFailTarget) { return TResult<void>::Failure(EErrorCode::BackendFailure, "target failed after mutation"); }
        return {};
    }
    TResult<void> Clear(FColor) override { m_Trace.Events.push_back("clear"); return {}; }
    TResult<void> ResetState(int, int) override { ++m_Trace.Resets; return {}; }
    TResult<void> DrawSprite(const FSpriteCommand& Command) override
    {
        m_Trace.DrawHandles.push_back(Command.Texture.GetNativeHandle_Internal());
        m_Trace.Opacities.push_back(Command.Options.Opacity);
        if (m_Trace.OnDraw) { m_Trace.OnDraw(); }
        return m_Trace.bFailDraw ? TResult<void>::Failure(EErrorCode::BackendFailure, "draw failed") : TResult<void>{};
    }
    TResult<void> DrawText(const FTextCommand&) override { return {}; }
    TResult<void> DrawRectangle(const FRectangleCommand&) override { return {}; }
    TResult<void> Present() override { ++m_Trace.Presentations; m_Trace.Events.push_back("present"); return {}; }
    TResult<void> Initialize(const FWindowSettings&) override
    { m_Trace.Events.push_back("init"); return m_Trace.bFailPlatform ? TResult<void>::Failure(EErrorCode::BackendFailure, "init failed") : TResult<void>{}; }
    void Shutdown() noexcept override { m_Trace.Events.push_back("shutdown"); }
    TResult<bool> PumpEvents() override { return TResult<bool>::Success(!m_Trace.bQuit); }
    TResult<FRawInput> Poll() override
    { return m_Trace.bFailInput ? TResult<FRawInput>::Failure(EErrorCode::BackendFailure, "input failed") : TResult<FRawInput>::Success(m_Trace.Input); }
private:
    FBackendTrace m_Trace;
};
}
