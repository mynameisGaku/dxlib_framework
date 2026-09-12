#include "Support/Test.h"
#include "NativeSmoke/Smoke.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/NativeRun.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderSystem2D.h"
#include "Dxf/AudioPlayer.h"
#include "DxLib.h"
#include "Toolbox/Utility.h"
using namespace Dxf;
TEST("Native platform selects UTF8 and rejects a second active session")
{
	DxLib::Trace = {};
	FDxLibPlatform First, Second;
	REQUIRE(First.Initialize({}));
	REQUIRE(DxLib::Trace.CharCode == DX_CHARCODEFORMAT_UTF8);
	REQUIRE(!Second.Initialize({}));
	First.Shutdown();
	REQUIRE(Second.Initialize({}));
	Second.Shutdown();
	REQUIRE(DxLib::Trace.Ends == 2);
}
TEST("Native failed DxLib initialization releases the session lease and partial state")
{
	DxLib::Trace = {};
	DxLib::Trace.bFailInit = true;
	/**
	 * 起動とイベント処理を提供するプラットフォーム。
	 */
	FDxLibPlatform Platform;
	REQUIRE(!Platform.Initialize({}));
	REQUIRE(DxLib::Trace.Ends == 1);
	DxLib::Trace.bFailInit = false;
	REQUIRE(Platform.Initialize({}));
}
TEST("Native texture load forwards UTF8 paths and cleans up a failed size query")
{
	DxLib::Trace = {};
	/**
	 * 生存中の画像資源番号。
	 */
	FDxLibTextureBackend Textures;
	REQUIRE(Textures.LoadTexture("素材/画像.bmp", {}));
	REQUIRE(DxLib::Trace.Path == "素材/画像.bmp");
	DxLib::Trace.bFailSize = true;
	REQUIRE(!Textures.LoadTexture("broken.bmp", {}));
	REQUIRE(DxLib::Trace.DeletedGraphs == 1);
}
TEST("Native input maps keys, mouse and normalized pad axes without keyboard emulation")
{
	DxLib::Trace = {};
	DxLib::Trace.Keys[KEY_INPUT_A] = 1;
	DxLib::Trace.MouseButtons = MOUSE_INPUT_LEFT;
	DxLib::Trace.PadCount = 1;
	DxLib::Trace.PadButtons = PAD_INPUT_1;
	DxLib::Trace.PadX = 500;
	DxLib::Trace.PadY = -1000;
	/**
	 * 検証で配信する入力状態。
	 */
	FDxLibInputSource Input;
	/**
	 * 検証する処理の状態。
	 */
	auto State = Input.Poll();
	REQUIRE(State);
	REQUIRE(State.Value().Keys[static_cast<Toolbox::size_t>(EKey::A)]);
	REQUIRE(State.Value().MouseButtons[0] && State.Value().Pads[0].Buttons[0]);
	REQUIRE(State.Value().Pads[0].LeftX == 0.5f && State.Value().Pads[0].LeftY == -1.0f);
	REQUIRE(!State.Value().Pads[1].bConnected);
	REQUIRE(DxLib::Trace.LastPad == DX_INPUT_PAD1);
}
TEST("Native input failure propagates and disconnected devices remain neutral")
{
	DxLib::Trace = {};
	/**
	 * 検証で配信する入力状態。
	 */
	FDxLibInputSource Input;
	DxLib::Trace.bFailKeys = true;
	REQUIRE(!Input.Poll());
	DxLib::Trace.bFailKeys = false;
	DxLib::Trace.bActive = false;
	/**
	 * 検証する処理の状態。
	 */
	auto State = Input.Poll();
	REQUIRE(State && !State.Value().bFocused && !State.Value().Pads[0].bConnected);
}
TEST("Native sound loader selects independent memory and stream storage explicitly")
{
	DxLib::Trace = {};
	/**
	 * 音声資源ごとの再生状態。
	 */
	FDxLibSoundBackend Sounds;
	REQUIRE(Sounds.LoadSound("se.wav", {}));
	REQUIRE(DxLib::Trace.LoadedSoundType == DX_SOUNDDATATYPE_MEMNOPRESS);
	REQUIRE(Sounds.LoadSound("bgm.ogg", {ESoundStorage::Stream}));
	REQUIRE(DxLib::Trace.LoadedSoundType == DX_SOUNDDATATYPE_FILE);
	REQUIRE(DxLib::Trace.SoundType == DX_SOUNDDATATYPE_MEMNOPRESS);
	REQUIRE(Sounds.SetSoundVolume(1, 0.5f));
	REQUIRE(DxLib::Trace.Volume == 128);
	REQUIRE(!Sounds.SetSoundVolume(1, 2.0f));
}
TEST("Native render applies per-command color and opacity without leaking sprite tint into text")
{
	DxLib::Trace = {};
	/**
	 * 複数の機能を提供する検証用バックエンド群。
	 */
	FDxLibBackends Backends;
	/**
	 * 各機能の依存先を束ねた参照。
	 */
	auto Services = Backends.GetServices();
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Services.Textures, Services.Sounds, Services.Fonts);
	/**
	 * 検証で使用する画像資源。
	 */
	auto Texture = Assets.LoadTexture("sprite.bmp").Value();
	/**
	 * 検証で使用するフォント資源。
	 */
	auto Font = Assets.LoadFont().Value();
	/**
	 * 描画を実行する検証用レンダラー。
	 */
	FRenderSystem2D Renderer(Services.Renderer);
	REQUIRE(Renderer.BeginFrame(640, 480));
	/**
	 * 描画時に適用するスタイル。
	 */
	FSpriteDrawOptions Style;
	Style.Opacity = 0.5f;
	Style.Color = {128, 64, 32, 128};
	REQUIRE(Renderer.GetContext().Draw(Texture, {10, 20}, Style));
	REQUIRE(Renderer.Flush());
	REQUIRE(DxLib::Trace.Alpha == 64 && DxLib::Trace.Brightness[0] == 128);
	REQUIRE(Renderer.GetContext().DrawText(Font, "日本語", {0, 0}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(DxLib::Trace.Brightness[0] == 255 && DxLib::Trace.Text == "日本語");
}
TEST("Native sprite geometry supports pivot scale and both flip directions")
{
	DxLib::Trace = {};
	/**
	 * 複数の機能を提供する検証用バックエンド群。
	 */
	FDxLibBackends Backends;
	/**
	 * 各機能の依存先を束ねた参照。
	 */
	auto Services = Backends.GetServices();
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Services.Textures, Services.Sounds, Services.Fonts);
	/**
	 * 検証で使用する画像資源。
	 */
	auto Texture = Assets.LoadTexture("sprite.bmp").Value();
	/**
	 * 描画を実行する検証用レンダラー。
	 */
	FRenderSystem2D Renderer(Services.Renderer);
	REQUIRE(Renderer.BeginFrame(100, 100));
	/**
	 * 描画時に適用するスタイル。
	 */
	FSpriteDrawOptions Style;
	Style.Pivot = {32, 32};
	Style.bFlipX = true;
	Style.bFlipY = true;
	REQUIRE(Renderer.GetContext().Draw(Texture, {50, 50}, Style));
	REQUIRE(Renderer.Flush());
	REQUIRE(DxLib::Trace.Vertices[0] == 82 && DxLib::Trace.Vertices[1] == 82);
	REQUIRE(DxLib::Trace.Vertices[4] == 18 && DxLib::Trace.Vertices[5] == 18);
}
TEST("Native backend errors abort presentation and preserve explicit target state")
{
	DxLib::Trace = {};
	/**
	 * 検証用のバックエンド。
	 */
	FDxLibRenderBackend Backend;
	/**
	 * 描画を実行する検証用レンダラー。
	 */
	FRenderSystem2D Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(100, 100));
	DxLib::Trace.bFailDraw = true;
	REQUIRE(Renderer.GetContext().FillRectangle({0, 0, 10, 10}));
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(DxLib::Trace.Presentations == 0);
}
TEST("Native clear does not silently claim arbitrary alpha clear support")
{
	DxLib::Trace = {};
	/**
	 * 検証用のバックエンド。
	 */
	FDxLibRenderBackend Backend;
	REQUIRE(!Backend.Clear({1, 2, 3, 64}));
	REQUIRE(Backend.Clear({1, 2, 3, 255}));
}
namespace
{
/**
 * 一回の更新後に終了を要求する。
 */
class DQuitAfterOneTickScene final : public DScene
{
protected:
	/**
	 * 更新の呼び出しを観測し、指定された処理を実行する。
	 */
	void OnTick(const FTickContext& Context) override
	{
		Context.Scenes->RequestQuit();
	}
};
} // namespace
TEST("Native convenience Run owns the session and exits on a scene quit request")
{
	DxLib::Trace = {};
	REQUIRE(Run<DQuitAfterOneTickScene>({}));
	REQUIRE(DxLib::Trace.Ends == 1);
}

TEST("Native platform rejects embedded NUL or malformed UTF8 window titles")
{
	DxLib::Trace = {};
	/**
	 * 起動とイベント処理を提供するプラットフォーム。
	 */
	FDxLibPlatform Platform;
	/**
	 * 起動または描画の設定。
	 */
	FWindowSettings Settings;
	Settings.Title = Toolbox::FString("title\0hidden", 12);
	REQUIRE(!Platform.Initialize(Settings));
	Settings.Title = Toolbox::FString("\xed\xa0\x80", 3);
	REQUIRE(!Platform.Initialize(Settings));
	Settings.Title = "日本語のウィンドウ";
	REQUIRE(Platform.Initialize(Settings));
}

TEST("Native smoke exercises asset loading render targets input audio and ordered teardown")
{
	DxLib::Trace = {};
	/**
	 * スモーク検証で確認した項目。
	 */
	auto Report = Dxf::Testing::RunNativeSmoke(DXF_TEST_ASSET_DIR, true);
	REQUIRE(Report);
	REQUIRE(Report.Value().Frames == 12);
	REQUIRE(Report.Value().bJapanesePathTested && Report.Value().bAudioTested);
	REQUIRE(Report.Value().bResourcesInvalidated);
	REQUIRE(DxLib::Trace.Ends == 1 && DxLib::Trace.Presentations == 12);
}

TEST("Native smoke reports a missing asset directory without starting DxLib")
{
	DxLib::Trace = {};
	REQUIRE(!Dxf::Testing::RunNativeSmoke("no-such-dxf-smoke-assets", false));
	REQUIRE(DxLib::Trace.Ends == 0);
}
