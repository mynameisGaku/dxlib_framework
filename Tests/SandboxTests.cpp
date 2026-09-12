#include "Toolbox/UniquePtr.h"
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/Application.h"
#include "SandboxGame.h"
using namespace Dxf;
using namespace Dxf::Testing;
using namespace Dxf::Sandbox;
namespace
{
// サンドボックス検証用のサービス参照をまとめる。
FBackendServices SandboxServices_Internal(FFakeBackend& Backend)
{
	return {Backend, Backend, Backend, Backend, Backend, Backend};
}
} // namespace
TEST("Sandbox uses actual scene object and component code to move and draw")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証するアプリケーション。
	FApplication App(SandboxServices_Internal(Backend), {}, Toolbox::MakeUnique<DSandboxGameInstance>());
	REQUIRE(App.Start(Toolbox::MakeUnique<DSandboxScene>("Assets")));
	// 検証対象のシーン。
	auto* Scene = App.GetScenes().GetCurrent()->TryCast<DSandboxScene>();
	REQUIRE(Scene);
	// 音声再生を操作するプレイヤー。
	auto Player = Scene->GetPlayer();
	REQUIRE(Player.Get());
	const Toolbox::f32 StartX = Player.Get()->GetPosition().X;
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::D)] = true;
	REQUIRE(App.Step(0));
	REQUIRE(App.Step(0.1));
	REQUIRE(Player.Get()->GetPosition().X > StartX);
	REQUIRE(Backend.GetTrace().DrawHandles.Size() == 2);
}
TEST("Sandbox switches scene while game state survives and old handles expire")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証するアプリケーション。
	FApplication App(SandboxServices_Internal(Backend), {}, Toolbox::MakeUnique<DSandboxGameInstance>());
	REQUIRE(App.Start(Toolbox::MakeUnique<DSandboxScene>("Assets")));
	// 停止や置換後の旧プレイヤー。
	auto OldPlayer = App.GetScenes().GetCurrent()->TryCast<DSandboxScene>()->GetPlayer();
	REQUIRE(App.GetGameInstance()->TryCast<DSandboxGameInstance>()->GetSceneVisits() == 1);
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::Enter)] = true;
	REQUIRE(App.Step(0));
	REQUIRE(OldPlayer.Get());
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::Enter)] = false;
	REQUIRE(App.Step(0.016));
	REQUIRE(!OldPlayer.Get());
	REQUIRE(App.GetGameInstance()->TryCast<DSandboxGameInstance>()->GetSceneVisits() == 2);
}
TEST("Sandbox audio is scene scoped and Escape shuts down the application")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証するアプリケーション。
	FApplication App(SandboxServices_Internal(Backend), {}, Toolbox::MakeUnique<DSandboxGameInstance>());
	REQUIRE(App.Start(Toolbox::MakeUnique<DSandboxScene>("Assets")));
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = true;
	REQUIRE(App.Step(0));
	REQUIRE(Backend.GetTrace().Clones == 1);
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::Escape)] = true;
	auto Quit = App.Step(0.016);
	REQUIRE(Quit && !Quit.Value());
	REQUIRE(Backend.GetTrace().Sounds.IsEmpty());
}
TEST("Sandbox propagates missing assets instead of entering a half-constructed scene")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	Backend.GetTrace().bFailTexture = true;
	// 検証するアプリケーション。
	FApplication App(SandboxServices_Internal(Backend));
	REQUIRE(!App.Start(Toolbox::MakeUnique<DSandboxScene>("MissingAssets")));
	REQUIRE(Backend.GetTrace().Textures.IsEmpty() && Backend.GetTrace().Sounds.IsEmpty());
}
