#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/InputMap.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/GameScene.h"
#include "Dxf/SpriteRendererComponent.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderSystem2D.h"
using namespace Dxf;
using namespace Dxf::Testing;

TEST("Actions combine keyboard mouse and gamepad without duplicate press edges")
{
	FInputMap Map;
	REQUIRE(Map.Bind("Fire", EKey::Space));
	REQUIRE(Map.BindMouse("Fire", EMouseButton::Left));
	REQUIRE(Map.BindPad("Fire", 0, 0));
	FInputStateTracker Tracker;
	FRawInput Raw;
	Raw.MouseButtons[0] = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.WasPressed("Fire"));
	Raw.MouseButtons[0] = false;
	Raw.Pads[0].bConnected = true;
	Raw.Pads[0].Buttons[0] = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.IsDown("Fire"));
	REQUIRE(!Map.WasPressed("Fire"));
	Raw.Pads[0].bConnected = false;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.WasReleased("Fire"));
}

TEST("Invalid action bindings are rejected without creating actions")
{
	FInputMap Map;
	REQUIRE(!Map.Bind("", EKey::Space));
	REQUIRE(!Map.Bind("Invalid", EKey::Count));
	REQUIRE(!Map.BindMouse("Invalid", EMouseButton::Count));
	REQUIRE(!Map.BindPad("Invalid", 4, 0));
	REQUIRE(!Map.BindPad("Invalid", 0, 16));
	REQUIRE(!Map.Unbind("Invalid"));
}

TEST("Input actions support rebinding and opposed digital axes")
{
	FInputMap Map;
	REQUIRE(Map.Bind("Left", EKey::A));
	REQUIRE(Map.Bind("Right", EKey::D));
	FInputStateTracker Tracker;
	FRawInput Raw;
	Raw.Keys[static_cast<std::size_t>(EKey::A)] = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.GetAxis("Left", "Right") == -1.0f);
	Raw.Keys[static_cast<std::size_t>(EKey::D)] = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.GetAxis("Left", "Right") == 0.0f);
	REQUIRE(Map.Unbind("Left"));
	REQUIRE(Map.GetAxis("Left", "Right") == 1.0f);
	Map.Clear();
	REQUIRE(!Map.IsDown("Right"));
}

namespace
{
class DTestObject final : public DGameObject {};
class DTestComponent final : public DGameObjectComponent {};
class DOtherComponent final : public DGameObjectComponent {};
}

TEST("Typed component lookup returns pending handles and excludes destruction requests")
{
	DTestObject Object;
	auto Component = Object.AddComponent<DTestComponent>().Value();
	REQUIRE(Object.AddComponent<DOtherComponent>());
	REQUIRE(Object.FindComponent<DTestComponent>() == Component);
	REQUIRE(Object.GetComponents<DGameObjectComponent>().size() == 2);
	REQUIRE(Object.RemoveComponent(Component));
	REQUIRE(!Object.FindComponent<DTestComponent>());
	REQUIRE(Object.GetComponents<DGameObjectComponent>().size() == 1);
}

TEST("Scene object queries remain non-owning across scene shutdown")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	DGameScene Scene;
	auto Object = Scene.Spawn<DTestObject>().Value();
	REQUIRE(Scene.FindObject<DTestObject>() == Object);
	REQUIRE(Scene.GetObjects<DGameObject>().size() == 1);
	REQUIRE(Scene.Initialize_Internal({Assets}));
	Scene.Shutdown_Internal();
	REQUIRE(!Object);
	REQUIRE(!Scene.FindObject<DTestObject>());
}

TEST("Built-in sprite component draws through automatic child dispatch")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FRenderSystem2D Renderer(Backend);
	DTestObject Object;
	auto Sprite = Object.AddComponent<DSpriteRendererComponent>(Assets.LoadTexture("a.bmp").Value()).Value();
	Sprite.Get()->GetPosition() = {12.0f, 24.0f};
	Sprite.Get()->GetOptions().Opacity = 0.5f;
	REQUIRE(Object.Initialize_Internal({Assets}));
	REQUIRE(Renderer.BeginFrame(100, 100));
	REQUIRE(Object.Draw_Internal(Renderer.GetContext()));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().DrawHandles.size() == 1);
	REQUIRE(Backend.GetTrace().Opacities[0] == 0.5f);
	Object.Shutdown_Internal();
}

TEST("Unconfigured sprite component is inert instead of issuing an invalid draw")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FRenderSystem2D Renderer(Backend);
	DTestObject Object;
	REQUIRE(Object.AddComponent<DSpriteRendererComponent>());
	REQUIRE(Object.Initialize_Internal({Assets}));
	REQUIRE(Renderer.BeginFrame(100, 100));
	REQUIRE(Object.Draw_Internal(Renderer.GetContext()));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().DrawHandles.empty());
	Object.Shutdown_Internal();
}
