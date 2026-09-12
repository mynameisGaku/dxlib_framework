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
	// 正規化前の入力データ。
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
	// 正規化前の入力データ。
	FRawInput Raw;
	Raw.Keys[static_cast<Toolbox::size_t>(EKey::A)] = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.GetAxis("Left", "Right") == -1.0f);
	Raw.Keys[static_cast<Toolbox::size_t>(EKey::D)] = true;
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
// オブジェクト登録と型判定を検証する最小実装。
class DTestObject final : public DGameObject
{
};
// コンポーネント登録と型判定を検証する最小実装。
class DTestComponent final : public DGameObjectComponent
{
};
// 型変換を拒否すべき別種のコンポーネント。
class DOtherComponent final : public DGameObjectComponent
{
};
} // namespace

TEST("Typed component lookup returns pending handles and excludes destruction requests")
{
	// 検証対象のオブジェクト。
	DTestObject Object;
	// 検証対象のコンポーネント。
	auto Component = Object.AddComponent<DTestComponent>().Value();
	REQUIRE(Object.AddComponent<DOtherComponent>());
	REQUIRE(Object.FindComponent<DTestComponent>() == Component);
	REQUIRE(Object.GetComponents<DGameObjectComponent>().Size() == 2);
	REQUIRE(Object.RemoveComponent(Component));
	REQUIRE(!Object.FindComponent<DTestComponent>());
	REQUIRE(Object.GetComponents<DGameObjectComponent>().Size() == 1);
}

TEST("Scene object queries remain non-owning across scene shutdown")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証対象のシーン。
	DGameScene Scene;
	// 検証対象のオブジェクト。
	auto Object = Scene.Spawn<DTestObject>().Value();
	REQUIRE(Scene.FindObject<DTestObject>() == Object);
	REQUIRE(Scene.GetObjects<DGameObject>().Size() == 1);
	REQUIRE(Scene.Initialize_Internal({Assets}));
	Scene.Shutdown_Internal();
	REQUIRE(!Object);
	REQUIRE(!Scene.FindObject<DTestObject>());
}

TEST("Built-in sprite component draws through automatic child dispatch")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem2D Renderer(Backend);
	// 検証対象のオブジェクト。
	DTestObject Object;
	// 検証するスプライト。
	auto Sprite = Object.AddComponent<DSpriteRendererComponent>(Assets.LoadTexture("a.bmp").Value()).Value();
	Sprite.Get()->GetPosition() = {12.0f, 24.0f};
	Sprite.Get()->GetOptions().Opacity = 0.5f;
	REQUIRE(Object.Initialize_Internal({Assets}));
	REQUIRE(Renderer.BeginFrame(100, 100));
	REQUIRE(Object.Draw_Internal(Renderer.GetContext()));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().DrawHandles.Size() == 1);
	REQUIRE(Backend.GetTrace().Opacities[0] == 0.5f);
	Object.Shutdown_Internal();
}

TEST("Unconfigured sprite component is inert instead of issuing an invalid draw")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem2D Renderer(Backend);
	// 検証対象のオブジェクト。
	DTestObject Object;
	REQUIRE(Object.AddComponent<DSpriteRendererComponent>());
	REQUIRE(Object.Initialize_Internal({Assets}));
	REQUIRE(Renderer.BeginFrame(100, 100));
	REQUIRE(Object.Draw_Internal(Renderer.GetContext()));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().DrawHandles.IsEmpty());
	Object.Shutdown_Internal();
}
