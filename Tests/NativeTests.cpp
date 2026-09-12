#include "Support/Test.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/NativeRun.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderSystem2D.h"
#include "Dxf/AudioPlayer.h"
#include "DxLib.h"
#include <cmath>
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
	FDxLibPlatform Platform;
	REQUIRE(!Platform.Initialize({}));
	REQUIRE(DxLib::Trace.Ends == 1);
	DxLib::Trace.bFailInit = false;
	REQUIRE(Platform.Initialize({}));
}
TEST("Native texture load forwards UTF8 paths and cleans up a failed size query")
{
	DxLib::Trace = {};
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
	FDxLibInputSource Input;
	auto State = Input.Poll();
	REQUIRE(State);
	REQUIRE(State.Value().Keys[static_cast<std::size_t>(EKey::A)]);
	REQUIRE(State.Value().MouseButtons[0] && State.Value().Pads[0].Buttons[0]);
	REQUIRE(State.Value().Pads[0].LeftX == 0.5f && State.Value().Pads[0].LeftY == -1.0f);
	REQUIRE(!State.Value().Pads[1].bConnected);
	REQUIRE(DxLib::Trace.LastPad == DX_INPUT_PAD1);
}
TEST("Native input failure propagates and disconnected devices remain neutral")
{
	DxLib::Trace = {};
	FDxLibInputSource Input;
	DxLib::Trace.bFailKeys = true;
	REQUIRE(!Input.Poll());
	DxLib::Trace.bFailKeys = false;
	DxLib::Trace.bActive = false;
	auto State = Input.Poll();
	REQUIRE(State && !State.Value().bFocused && !State.Value().Pads[0].bConnected);
}
TEST("Native sound loader selects independent memory and stream storage explicitly")
{
	DxLib::Trace = {};
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
	FDxLibBackends Backends;
	auto Services = Backends.GetServices();
	FAssetService Assets(Services.Textures, Services.Sounds, Services.Fonts);
	auto Texture = Assets.LoadTexture("sprite.bmp").Value();
	auto Font = Assets.LoadFont().Value();
	FRenderSystem2D Renderer(Services.Renderer);
	REQUIRE(Renderer.BeginFrame(640, 480));
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
	FDxLibBackends Backends;
	auto Services = Backends.GetServices();
	FAssetService Assets(Services.Textures, Services.Sounds, Services.Fonts);
	auto Texture = Assets.LoadTexture("sprite.bmp").Value();
	FRenderSystem2D Renderer(Services.Renderer);
	REQUIRE(Renderer.BeginFrame(100, 100));
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
	FDxLibRenderBackend Backend;
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
	FDxLibRenderBackend Backend;
	REQUIRE(!Backend.Clear({1, 2, 3, 64}));
	REQUIRE(Backend.Clear({1, 2, 3, 255}));
}
namespace
{
class DQuitAfterOneTickScene final : public DScene
{
protected:
	void OnTick(const FTickContext& Context) override
	{
		Context.Scenes->RequestQuit();
	}
};
}
TEST("Native convenience Run owns the session and exits on a scene quit request")
{
	DxLib::Trace = {};
	REQUIRE(Run<DQuitAfterOneTickScene>({}));
	REQUIRE(DxLib::Trace.Ends == 1);
}

TEST("Native platform rejects embedded NUL or malformed UTF8 window titles")
{
	DxLib::Trace = {};
	FDxLibPlatform Platform;
	FWindowSettings Settings;
	Settings.Title = std::string("title\0hidden", 12);
	REQUIRE(!Platform.Initialize(Settings));
	Settings.Title = std::string("\xed\xa0\x80", 3);
	REQUIRE(!Platform.Initialize(Settings));
	Settings.Title = "日本語のウィンドウ";
	REQUIRE(Platform.Initialize(Settings));
}
