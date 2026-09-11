#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/Application.h"
#include "SandboxGame.h"
using namespace Dxf;
using namespace Dxf::Testing;
using namespace Dxf::Sandbox;
namespace
{
FBackendServices SandboxServices_Internal(FFakeBackend& Backend)
{
    return {Backend, Backend, Backend, Backend, Backend, Backend};
}
}
TEST("Sandbox uses actual scene object and component code to move and draw")
{
    FFakeBackend Backend;
    FApplication App(SandboxServices_Internal(Backend), {}, std::make_unique<DSandboxGameInstance>());
    REQUIRE(App.Start(std::make_unique<DSandboxScene>("Assets")));
    auto* Scene = App.GetScenes().GetCurrent()->TryCast<DSandboxScene>(); REQUIRE(Scene);
    auto Player = Scene->GetPlayer(); REQUIRE(Player.Get());
    const float StartX = Player.Get()->GetPosition().X;
    Backend.GetTrace().Input.Keys[static_cast<std::size_t>(EKey::D)] = true;
    REQUIRE(App.Step(0)); REQUIRE(App.Step(0.1));
    REQUIRE(Player.Get()->GetPosition().X > StartX);
    REQUIRE(Backend.GetTrace().DrawHandles.size() == 2);
}
TEST("Sandbox switches scene while game state survives and old handles expire")
{
    FFakeBackend Backend;
    FApplication App(SandboxServices_Internal(Backend), {}, std::make_unique<DSandboxGameInstance>());
    REQUIRE(App.Start(std::make_unique<DSandboxScene>("Assets")));
    auto OldPlayer = App.GetScenes().GetCurrent()->TryCast<DSandboxScene>()->GetPlayer();
    REQUIRE(App.GetGameInstance()->TryCast<DSandboxGameInstance>()->GetSceneVisits() == 1);
    Backend.GetTrace().Input.Keys[static_cast<std::size_t>(EKey::Enter)] = true;
    REQUIRE(App.Step(0)); REQUIRE(OldPlayer.Get());
    Backend.GetTrace().Input.Keys[static_cast<std::size_t>(EKey::Enter)] = false;
    REQUIRE(App.Step(0.016)); REQUIRE(!OldPlayer.Get());
    REQUIRE(App.GetGameInstance()->TryCast<DSandboxGameInstance>()->GetSceneVisits() == 2);
}
TEST("Sandbox audio is scene scoped and Escape shuts down the application")
{
    FFakeBackend Backend;
    FApplication App(SandboxServices_Internal(Backend), {}, std::make_unique<DSandboxGameInstance>());
    REQUIRE(App.Start(std::make_unique<DSandboxScene>("Assets")));
    Backend.GetTrace().Input.Keys[static_cast<std::size_t>(EKey::Space)] = true;
    REQUIRE(App.Step(0)); REQUIRE(Backend.GetTrace().Clones == 1);
    Backend.GetTrace().Input.Keys[static_cast<std::size_t>(EKey::Escape)] = true;
    auto Quit = App.Step(0.016); REQUIRE(Quit && !Quit.Value()); REQUIRE(Backend.GetTrace().Sounds.empty());
}
TEST("Sandbox propagates missing assets instead of entering a half-constructed scene")
{
    FFakeBackend Backend; Backend.GetTrace().bFailTexture = true;
    FApplication App(SandboxServices_Internal(Backend));
    REQUIRE(!App.Start(std::make_unique<DSandboxScene>("MissingAssets")));
    REQUIRE(Backend.GetTrace().Textures.empty() && Backend.GetTrace().Sounds.empty());
}
