#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderSystem2D.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/DxLibSession.h"
#include <limits>
using namespace Dxf;
using namespace Dxf::Testing;
TEST("Texture cache shares normalized paths")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend);
    auto A = Assets.LoadTexture("Assets/../Assets/player.bmp");
    auto B = Assets.LoadTexture("Assets/player.bmp");
    REQUIRE(A && B); REQUIRE(Backend.GetTrace().TextureLoads == 1);
    REQUIRE(A.Value().GetNativeHandle_Internal() == B.Value().GetNativeHandle_Internal());
}
TEST("Texture load options participate in cache identity")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend);
    auto A = Assets.LoadTexture("a.bmp");
    FTextureLoadOptions Options; Options.bUse3D = false;
    auto B = Assets.LoadTexture("a.bmp", Options);
    REQUIRE(A && B); REQUIRE(Backend.GetTrace().TextureLoads == 2);
}
TEST("Texture is freed once when last reference expires")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend);
    { auto A = Assets.LoadTexture("a.bmp").Value(); { auto B = A; REQUIRE(B.IsValid()); } REQUIRE(Backend.GetTrace().DeletedTextures.empty()); }
    REQUIRE(Backend.GetTrace().DeletedTextures.size() == 1);
}
TEST("Asset shutdown invalidates externally retained references")
{
    FFakeBackend Backend; FTexture External;
    { FAssetService Assets(Backend, Backend, Backend); External = Assets.LoadTexture("a.bmp").Value(); Assets.Shutdown(); REQUIRE(!External.IsValid()); }
    External = {}; REQUIRE(Backend.GetTrace().DeletedTextures.size() == 1);
}
TEST("Failed load is not cached and can be retried")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend);
    Backend.GetTrace().bFailTexture = true; REQUIRE(!Assets.LoadTexture("bad.bmp"));
    Backend.GetTrace().bFailTexture = false; REQUIRE(Assets.LoadTexture("bad.bmp"));
    REQUIRE(Backend.GetTrace().TextureLoads == 2);
}
TEST("Invalid successful native handle is rejected")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend);
    Backend.GetTrace().bInvalidTexture = true;
    REQUIRE(!Assets.LoadTexture("bad.bmp")); REQUIRE(Backend.GetTrace().DeletedTextures.empty());
}
TEST("Loading after asset shutdown is rejected")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend);
    Assets.Shutdown(); REQUIRE(!Assets.LoadTexture("a.bmp")); REQUIRE(Backend.GetTrace().TextureLoads == 0);
}
TEST("Invalid render target dimensions fail before backend call")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend);
    REQUIRE(!Assets.CreateRenderTarget(0, 100)); REQUIRE(Backend.GetTrace().Textures.empty());
}
TEST("Font cache separates sizes and invalidates references")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend);
    FFontOptions Options; auto A = Assets.LoadFont(Options).Value(); auto B = Assets.LoadFont(Options).Value();
    REQUIRE(A.GetNativeHandle_Internal() == B.GetNativeHandle_Internal());
    Options.Size = 42; auto C = Assets.LoadFont(Options).Value(); REQUIRE(C.GetNativeHandle_Internal() != A.GetNativeHandle_Internal());
    Assets.Shutdown(); REQUIRE(!A.IsValid()); REQUIRE(!C.IsValid()); REQUIRE(Backend.GetTrace().DeletedFonts.size() == 2);
}
TEST("Render queue sorts layer and order while preserving equal-key insertion")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FRenderSystem2D Renderer(Backend);
    auto A = Assets.LoadTexture("a.bmp").Value(); auto B = Assets.LoadTexture("b.bmp").Value();
    REQUIRE(Renderer.BeginFrame(640, 480));
    FSpriteDrawOptions Back; Back.Layer = -1;
    REQUIRE(Renderer.GetContext().Draw(A, {})); REQUIRE(Renderer.GetContext().Draw(B, {}, Back)); REQUIRE(Renderer.GetContext().Draw(B, {}));
    REQUIRE(Renderer.EndFrame());
    REQUIRE(Backend.GetTrace().DrawHandles == std::vector<int>({B.GetNativeHandle_Internal(), A.GetNativeHandle_Internal(), B.GetNativeHandle_Internal()}));
}
TEST("Sprite opacity does not leak to following commands")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FRenderSystem2D Renderer(Backend);
    auto Texture = Assets.LoadTexture("a.bmp").Value(); REQUIRE(Renderer.BeginFrame(640, 480));
    FSpriteDrawOptions Transparent; Transparent.Opacity = 0.3f;
    REQUIRE(Renderer.GetContext().Draw(Texture, {}, Transparent)); REQUIRE(Renderer.GetContext().Draw(Texture, {})); REQUIRE(Renderer.EndFrame());
    REQUIRE(Backend.GetTrace().Opacities == std::vector<float>({0.3f, 1.0f}));
}
TEST("Render commands keep textures alive until execution")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FRenderSystem2D Renderer(Backend);
    REQUIRE(Renderer.BeginFrame(640, 480));
    { auto Texture = Assets.LoadTexture("a.bmp").Value(); REQUIRE(Renderer.GetContext().Draw(Texture, {})); }
    REQUIRE(Backend.GetTrace().DeletedTextures.empty()); REQUIRE(Renderer.EndFrame()); REQUIRE(Backend.GetTrace().DeletedTextures.size() == 1);
}
TEST("Native barrier flushes earlier commands and restores state after exception")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FRenderSystem2D Renderer(Backend);
    auto Texture = Assets.LoadTexture("a.bmp").Value(); REQUIRE(Renderer.BeginFrame(640, 480));
    REQUIRE(Renderer.GetContext().Draw(Texture, {}));
    const int Resets = Backend.GetTrace().Resets;
    auto Result = Renderer.Native([&]() -> TResult<void>
    {
        REQUIRE(Backend.GetTrace().DrawHandles.size() == 1); Backend.GetTrace().CurrentTarget = 999; throw std::runtime_error("user draw");
    });
    REQUIRE(!Result); REQUIRE(Backend.GetTrace().CurrentTarget == -1); REQUIRE(Backend.GetTrace().Resets > Resets);
    REQUIRE(Renderer.EndFrame());
}
TEST("Drawing render target into itself is rejected")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FRenderSystem2D Renderer(Backend);
    auto Target = Assets.CreateRenderTarget(128, 128).Value(); REQUIRE(Renderer.BeginFrame(640, 480)); REQUIRE(Renderer.SetRenderTarget(Target));
    REQUIRE(!Renderer.GetContext().Draw(Target.AsTexture(), {})); REQUIRE(Renderer.SetBackBuffer());
    REQUIRE(Renderer.GetContext().Draw(Target.AsTexture(), {})); REQUIRE(Renderer.EndFrame());
}
TEST("Invalidated texture fails submission and never reaches backend")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FRenderSystem2D Renderer(Backend);
    auto Texture = Assets.LoadTexture("a.bmp").Value(); Assets.Shutdown(); REQUIRE(Renderer.BeginFrame(640, 480));
    REQUIRE(!Renderer.GetContext().Draw(Texture, {})); REQUIRE(Renderer.EndFrame()); REQUIRE(Backend.GetTrace().DrawHandles.empty());
}
TEST("Invalidation between queue and flush is detected")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FRenderSystem2D Renderer(Backend);
    auto Texture = Assets.LoadTexture("a.bmp").Value(); REQUIRE(Renderer.BeginFrame(640, 480)); REQUIRE(Renderer.GetContext().Draw(Texture, {}));
    Assets.Shutdown(); REQUIRE(!Renderer.EndFrame()); REQUIRE(Backend.GetTrace().Presentations == 0);
}
TEST("Nonfinite sprite values are rejected")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FRenderSystem2D Renderer(Backend);
    auto Texture = Assets.LoadTexture("a.bmp").Value(); REQUIRE(Renderer.BeginFrame(640, 480));
    FSpriteDrawOptions Options; Options.Opacity = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(!Renderer.GetContext().Draw(Texture, {}, Options));
}
TEST("Render flush prevents reentrant native calls")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FRenderSystem2D Renderer(Backend);
    auto Texture = Assets.LoadTexture("a.bmp").Value(); REQUIRE(Renderer.BeginFrame(640, 480));
    Backend.GetTrace().OnDraw = [&] { REQUIRE(!Renderer.Native([] { return TResult<void>{}; })); };
    REQUIRE(Renderer.GetContext().Draw(Texture, {})); REQUIRE(Renderer.EndFrame());
}
TEST("Each memory sound playback owns a distinct native handle")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FAudioPlayer Audio(Backend);
    auto Sound = Assets.LoadSound("shot.wav").Value(); auto A = Audio.Play(Sound).Value(); auto B = Audio.Play(Sound).Value();
    REQUIRE(A.GetId() != B.GetId()); REQUIRE(Backend.GetTrace().Clones == 2);
    REQUIRE(Audio.Stop(A)); REQUIRE(!A.Get()); REQUIRE(B.Get());
}
TEST("Stream playback reloads instead of duplicating unsupported sound data")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FAudioPlayer Audio(Backend);
    FSoundLoadOptions Options; Options.Storage = ESoundStorage::Stream;
    auto Sound = Assets.LoadSound("music.ogg", Options).Value(); REQUIRE(Audio.Play(Sound));
    REQUIRE(Backend.GetTrace().Clones == 0); REQUIRE(Backend.GetTrace().SoundLoads == 2);
}
TEST("Failed playback frees the newly created voice")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FAudioPlayer Audio(Backend);
    auto Sound = Assets.LoadSound("shot.wav").Value(); Backend.GetTrace().bFailStart = true;
    REQUIRE(!Audio.Play(Sound)); REQUIRE(Backend.GetTrace().DeletedSounds.size() == 1); REQUIRE(Sound.IsValid());
}
TEST("Audio scopes stop scene sound without stopping persistent music")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FAudioPlayer Audio(Backend);
    auto Sound = Assets.LoadSound("shot.wav").Value(); FPlaybackOptions Options; Options.Scope = 11;
    auto A = Audio.Play(Sound, Options).Value(); auto Global = Audio.Play(Sound).Value(); Audio.StopScope(11);
    REQUIRE(!A.Get()); REQUIRE(Global.Get());
}
TEST("Finished voices are reclaimed by Tick")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FAudioPlayer Audio(Backend);
    auto Sound = Assets.LoadSound("shot.wav").Value(); auto Voice = Audio.Play(Sound).Value();
    for (auto& [Handle, bPlaying] : Backend.GetTrace().Sounds) { (void)Handle; bPlaying = false; }
    REQUIRE(Audio.Tick()); REQUIRE(!Voice.Get());
}
TEST("Audio player rejects another player's handle and invalid volume")
{
    FFakeBackend Backend; FAssetService Assets(Backend, Backend, Backend); FAudioPlayer A(Backend); FAudioPlayer B(Backend);
    auto Sound = Assets.LoadSound("shot.wav").Value(); auto Voice = A.Play(Sound).Value();
    REQUIRE(!B.Stop(Voice)); REQUIRE(!A.SetVolume(Voice, -0.1f)); REQUIRE(Voice.Get());
}
TEST("Session ends exactly once on success and does not end an uninitialized backend")
{
    FFakeBackend Backend;
    { FDxLibSession Session(Backend); REQUIRE(Session.Initialize({})); Session.Shutdown(); Session.Shutdown(); }
    REQUIRE(Backend.GetTrace().Events == std::vector<std::string>({"init", "shutdown"}));
    FFakeBackend Failed; Failed.GetTrace().bFailPlatform = true;
    { FDxLibSession Session(Failed); REQUIRE(!Session.Initialize({})); }
    REQUIRE(Failed.GetTrace().Events == std::vector<std::string>({"init"}));
}
TEST("Native state restoration failure aborts the frame")
{
    FFakeBackend Backend; FRenderSystem2D Renderer(Backend);
    REQUIRE(Renderer.BeginFrame(640, 480));
    REQUIRE(!Renderer.Native([&]() -> TResult<void> { Backend.GetTrace().bFailTarget = true; return {}; }));
    Backend.GetTrace().bFailTarget = false;
    REQUIRE(!Renderer.EndFrame());
    REQUIRE(Backend.GetTrace().Presentations == 0);
    REQUIRE(Renderer.BeginFrame(640, 480)); REQUIRE(Renderer.EndFrame());
}
TEST("Native handle adoption is allocation-free, noexcept and releases once across moves")
{
    int Releases = 0;
    const auto Release = +[](void* Context, int Handle) noexcept
    {
        if (Handle >= 0) { ++*static_cast<int*>(Context); }
    };
    static_assert(noexcept(FNativeHandle(1, &Releases, Release)));
    {
        FNativeHandle First(1, &Releases, Release);
        FNativeHandle Second(std::move(First));
        FNativeHandle Third(2, &Releases, Release);
        Third = std::move(Second);
        REQUIRE(Releases == 1);
        Third.Reset(); Third.Reset();
        REQUIRE(Releases == 2);
        FNativeHandle Invalid(-1, &Releases, Release);
    }
    REQUIRE(Releases == 2);
}
