#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderSystem.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/DxLibSession.h"
#include "Toolbox/Utility.h"
using namespace Dxf;
using namespace Dxf::Testing;
TEST("Texture cache shares normalized paths")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	auto A = Assets.LoadTexture("Assets/../Assets/player.bmp");
	auto B = Assets.LoadTexture("Assets/player.bmp");
	REQUIRE(A && B);
	REQUIRE(Backend.GetTrace().TextureLoads == 1);
	REQUIRE(A.Value().GetNativeHandle_Internal() == B.Value().GetNativeHandle_Internal());
}
TEST("Texture load options participate in cache identity")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	auto A = Assets.LoadTexture("a.bmp");
	// 検証条件を指定する読み込みオプション。
	FTextureLoadOptions Options;
	Options.bUse3D = false;
	auto B = Assets.LoadTexture("a.bmp", Options);
	REQUIRE(A && B);
	REQUIRE(Backend.GetTrace().TextureLoads == 2);
}
TEST("Texture is freed once when last reference expires")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	{
		auto A = Assets.LoadTexture("a.bmp").Value();
		{
			auto B = A;
			REQUIRE(B.IsValid());
		}
		REQUIRE(Backend.GetTrace().DeletedTextures.IsEmpty());
	}
	REQUIRE(Backend.GetTrace().DeletedTextures.Size() == 1);
}
TEST("Asset shutdown invalidates externally retained references")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 所有側の外に残す参照。
	FTexture External;
	{
		// 検証に使用する資源管理。
		FAssetService Assets(Backend, Backend, Backend);
		External = Assets.LoadTexture("a.bmp").Value();
		Assets.Shutdown();
		REQUIRE(!External.IsValid());
	}
	External = {};
	REQUIRE(Backend.GetTrace().DeletedTextures.Size() == 1);
}
TEST("Failed load is not cached and can be retried")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	Backend.GetTrace().bFailTexture = true;
	REQUIRE(!Assets.LoadTexture("bad.bmp"));
	Backend.GetTrace().bFailTexture = false;
	REQUIRE(Assets.LoadTexture("bad.bmp"));
	REQUIRE(Backend.GetTrace().TextureLoads == 2);
}
TEST("Invalid successful native handle is rejected")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	Backend.GetTrace().bInvalidTexture = true;
	REQUIRE(!Assets.LoadTexture("bad.bmp"));
	REQUIRE(Backend.GetTrace().DeletedTextures.IsEmpty());
}
TEST("Loading after asset shutdown is rejected")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	Assets.Shutdown();
	REQUIRE(!Assets.LoadTexture("a.bmp"));
	REQUIRE(Backend.GetTrace().TextureLoads == 0);
}
TEST("Invalid render target dimensions fail before backend call")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	REQUIRE(!Assets.CreateRenderTarget(0, 100));
	REQUIRE(Backend.GetTrace().Textures.IsEmpty());
}
TEST("Font cache separates sizes and invalidates references")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証条件を指定する読み込みオプション。
	FFontOptions Options;
	auto A = Assets.LoadFont(Options).Value();
	auto B = Assets.LoadFont(Options).Value();
	REQUIRE(A.GetNativeHandle_Internal() == B.GetNativeHandle_Internal());
	Options.Size = 42;
	auto C = Assets.LoadFont(Options).Value();
	REQUIRE(C.GetNativeHandle_Internal() != A.GetNativeHandle_Internal());
	Assets.Shutdown();
	REQUIRE(!A.IsValid());
	REQUIRE(!C.IsValid());
	REQUIRE(Backend.GetTrace().DeletedFonts.Size() == 2);
}
TEST("Render queue sorts layer and order while preserving equal-key insertion")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	auto A = Assets.LoadTexture("a.bmp").Value();
	auto B = Assets.LoadTexture("b.bmp").Value();
	REQUIRE(Renderer.BeginFrame(640, 480));
	FSpriteDrawOptions Back;
	Back.Layer = -1;
	REQUIRE(Renderer.GetContext().Get2D().DrawSprite(A, {}));
	REQUIRE(Renderer.GetContext().Get2D().DrawSprite(B, {}, Back));
	REQUIRE(Renderer.GetContext().Get2D().DrawSprite(B, {}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().DrawHandles ==
	        Toolbox::TVector<Toolbox::int32>(
	            {B.GetNativeHandle_Internal(), A.GetNativeHandle_Internal(), B.GetNativeHandle_Internal()}));
}
TEST("Sprite opacity does not leak to following commands")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	// 検証で使用する画像資源。
	auto Texture = Assets.LoadTexture("a.bmp").Value();
	REQUIRE(Renderer.BeginFrame(640, 480));
	// 透過描画の検証条件。
	FSpriteDrawOptions Transparent;
	Transparent.Opacity = 0.3f;
	REQUIRE(Renderer.GetContext().Get2D().DrawSprite(Texture, {}, Transparent));
	REQUIRE(Renderer.GetContext().Get2D().DrawSprite(Texture, {}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().Opacities == Toolbox::TVector<Toolbox::f32>({0.3f, 1.0f}));
}
TEST("Render commands keep textures alive until execution")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	{
		// 検証で使用する画像資源。
		auto Texture = Assets.LoadTexture("a.bmp").Value();
		REQUIRE(Renderer.GetContext().Get2D().DrawSprite(Texture, {}));
	}
	REQUIRE(Backend.GetTrace().DeletedTextures.IsEmpty());
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().DeletedTextures.Size() == 1);
}
TEST("Native barrier restores state after exception but discards the failed frame")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	// 検証で使用する画像資源。
	auto Texture = Assets.LoadTexture("a.bmp").Value();
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get2D().DrawSprite(Texture, {}));
	// 描画状態リセットの呼び出し回数。
	const Toolbox::int32 Resets = Backend.GetTrace().Resets;
	// 検証対象の操作が返した成否と値。
	auto Result = Renderer.Native(
	    [&]() -> TResult<void>
	    {
		    REQUIRE(Backend.GetTrace().DrawHandles.Size() == 1);
		    Backend.GetTrace().CurrentTarget = 999;
		    throw Toolbox::FException("user draw");
	    });
	REQUIRE(!Result);
	REQUIRE(Backend.GetTrace().CurrentTarget == -1);
	REQUIRE(Backend.GetTrace().Resets > Resets);
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().Presentations == 0);
}
TEST("Drawing render target into itself is rejected")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	// 描画先または遷移先。
	auto Target = Assets.CreateRenderTarget(128, 128).Value();
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.SetRenderTarget(Target));
	REQUIRE(!Renderer.GetContext().Get2D().DrawSprite(Target.AsTexture(), {}));
	REQUIRE(Renderer.SetBackBuffer());
	REQUIRE(Renderer.GetContext().Get2D().DrawSprite(Target.AsTexture(), {}));
	REQUIRE(Renderer.EndFrame());
}
TEST("Invalidated texture fails submission and never reaches backend")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	// 検証で使用する画像資源。
	auto Texture = Assets.LoadTexture("a.bmp").Value();
	Assets.Shutdown();
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(!Renderer.GetContext().Get2D().DrawSprite(Texture, {}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().DrawHandles.IsEmpty());
}
TEST("Invalidation between queue and flush is detected")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	// 検証で使用する画像資源。
	auto Texture = Assets.LoadTexture("a.bmp").Value();
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get2D().DrawSprite(Texture, {}));
	Assets.Shutdown();
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().Presentations == 0);
}
TEST("Nonfinite sprite values are rejected")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	// 検証で使用する画像資源。
	auto Texture = Assets.LoadTexture("a.bmp").Value();
	REQUIRE(Renderer.BeginFrame(640, 480));
	// 検証条件を指定する読み込みオプション。
	FSpriteDrawOptions Options;
	Options.Opacity = Toolbox::TNumericLimits<Toolbox::f32>::QuietNaN();
	REQUIRE(!Renderer.GetContext().Get2D().DrawSprite(Texture, {}, Options));
}
TEST("Render flush prevents reentrant native calls")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	// 検証で使用する画像資源。
	auto Texture = Assets.LoadTexture("a.bmp").Value();
	REQUIRE(Renderer.BeginFrame(640, 480));
	Backend.GetTrace().OnDraw = [&]
	{
		REQUIRE(!Renderer.Native(
		    []
		    {
			    return TResult<void>{};
		    }));
	};
	REQUIRE(Renderer.GetContext().Get2D().DrawSprite(Texture, {}));
	REQUIRE(Renderer.EndFrame());
}
TEST("Each memory sound playback owns a distinct native handle")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 再生状態を管理する音声サービス。
	FAudioPlayer Audio(Backend);
	// 検証で使用する音声資源。
	auto Sound = Assets.LoadSound("shot.wav").Value();
	auto A = Audio.Play(Sound).Value();
	auto B = Audio.Play(Sound).Value();
	REQUIRE(A.GetId() != B.GetId());
	REQUIRE(Backend.GetTrace().Clones == 2);
	REQUIRE(Audio.Stop(A));
	REQUIRE(!A.Get());
	REQUIRE(B.Get());
}
TEST("Stream playback reloads instead of duplicating unsupported sound data")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 再生状態を管理する音声サービス。
	FAudioPlayer Audio(Backend);
	// 検証条件を指定する読み込みオプション。
	FSoundLoadOptions Options;
	Options.Storage = ESoundStorage::Stream;
	// 検証で使用する音声資源。
	auto Sound = Assets.LoadSound("music.ogg", Options).Value();
	REQUIRE(Audio.Play(Sound));
	REQUIRE(Backend.GetTrace().Clones == 0);
	REQUIRE(Backend.GetTrace().SoundLoads == 2);
}
TEST("Failed playback frees the newly created voice")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 再生状態を管理する音声サービス。
	FAudioPlayer Audio(Backend);
	// 検証で使用する音声資源。
	auto Sound = Assets.LoadSound("shot.wav").Value();
	Backend.GetTrace().bFailStart = true;
	REQUIRE(!Audio.Play(Sound));
	REQUIRE(Backend.GetTrace().DeletedSounds.Size() == 1);
	REQUIRE(Sound.IsValid());
}
TEST("Audio scopes stop scene sound without stopping persistent music")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 再生状態を管理する音声サービス。
	FAudioPlayer Audio(Backend);
	// 検証で使用する音声資源。
	auto Sound = Assets.LoadSound("shot.wav").Value();
	// 検証条件を指定する読み込みオプション。
	FPlaybackOptions Options;
	Options.Scope = 11;
	auto A = Audio.Play(Sound, Options).Value();
	auto Global = Audio.Play(Sound).Value();
	Audio.StopScope(11);
	REQUIRE(!A.Get());
	REQUIRE(Global.Get());
}
TEST("Finished voices are reclaimed by Tick")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 再生状態を管理する音声サービス。
	FAudioPlayer Audio(Backend);
	// 検証で使用する音声資源。
	auto Sound = Assets.LoadSound("shot.wav").Value();
	// 音声の再生インスタンス。
	auto Voice = Audio.Play(Sound).Value();
	for (auto& [Handle, bPlaying] : Backend.GetTrace().Sounds)
	{
		(void)Handle;
		bPlaying = false;
	}
	REQUIRE(Audio.Tick());
	REQUIRE(!Voice.Get());
}
TEST("Audio player rejects another player's handle and invalid volume")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer A(Backend);
	FAudioPlayer B(Backend);
	// 検証で使用する音声資源。
	auto Sound = Assets.LoadSound("shot.wav").Value();
	// 音声の再生インスタンス。
	auto Voice = A.Play(Sound).Value();
	REQUIRE(!B.Stop(Voice));
	REQUIRE(!A.SetVolume(Voice, -0.1f));
	REQUIRE(Voice.Get());
}
TEST("Session ends exactly once on success and does not end an uninitialized backend")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	{
		// 実行中のセッション。
		FDxLibSession Session(Backend);
		REQUIRE(Session.Initialize({}));
		Session.Shutdown();
		Session.Shutdown();
	}
	REQUIRE(Backend.GetTrace().Events == Toolbox::TVector<Toolbox::FString>({"init", "shutdown"}));
	// 不正操作が拒否されたか。
	FFakeBackend Failed;
	Failed.GetTrace().bFailPlatform = true;
	{
		// 実行中のセッション。
		FDxLibSession Session(Failed);
		REQUIRE(!Session.Initialize({}));
	}
	REQUIRE(Failed.GetTrace().Events == Toolbox::TVector<Toolbox::FString>({"init"}));
}
TEST("Native state restoration failure aborts the frame")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(!Renderer.Native(
	    [&]() -> TResult<void>
	    {
		    Backend.GetTrace().bFailTarget = true;
		    return {};
	    }));
	Backend.GetTrace().bFailTarget = false;
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().Presentations == 0);
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.EndFrame());
}
TEST("Native handle adoption is allocation-free, noexcept and releases once across moves")
{
	Toolbox::int32 Releases = 0;
	const auto Release = +[](void* Context, Toolbox::int32 Handle) noexcept
	{
		if (Handle >= 0)
		{
			++*static_cast<Toolbox::int32*>(Context);
		}
	};
	static_assert(noexcept(FNativeHandle(1, &Releases, Release)));
	{
		// 最初に生成または登録した対象。
		FNativeHandle First(1, &Releases, Release);
		// 二番目に生成または登録した対象。
		FNativeHandle Second(Toolbox::Move(First));
		// 三番目に生成または登録した対象。
		FNativeHandle Third(2, &Releases, Release);
		Third = Toolbox::Move(Second);
		REQUIRE(Releases == 1);
		Third.Reset();
		Third.Reset();
		REQUIRE(Releases == 2);
		FNativeHandle Invalid(-1, &Releases, Release);
	}
	REQUIRE(Releases == 2);
}
TEST("Scene-facing render context exposes ordered target and native barriers without Application access")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem Renderer(Backend);
	// 描画先または遷移先。
	auto Target = Assets.CreateRenderTarget(64, 64, false).Value();
	REQUIRE(Renderer.BeginFrame(640, 480));
	// フック呼び出しへ渡す実行環境。
	auto& Context = Renderer.GetContext();
	REQUIRE(Context.SetRenderTarget(Target));
	REQUIRE(Context.Get2D().FillRectangle({0, 0, 32, 32}));
	REQUIRE(Context.Native(
	    [&]()
	    {
		    REQUIRE(Backend.GetTrace().CurrentTarget == Target.AsTexture().GetNativeHandle_Internal());
		    return TResult<void>{};
	    }));
	REQUIRE(Context.SetBackBuffer());
	REQUIRE(Context.Get2D().DrawSprite(Target.AsTexture(), {0, 0}));
	REQUIRE(Renderer.EndFrame());
}
TEST("Queue-only render contexts reject unsupported immediate control explicitly")
{
	// 処理順序を検証する待機操作。
	FRenderQueue2D Queue;
	// フック呼び出しへ渡す実行環境。
	FRenderContext Context(Queue);
	REQUIRE(!Context.SetBackBuffer());
	REQUIRE(!Context.Native(
	    []()
	    {
		    return TResult<void>{};
	    }));
}
