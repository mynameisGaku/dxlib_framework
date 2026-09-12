#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/GameObjectCollection.h"
#include "Dxf/GameObject.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/RenderSystem2D.h"
#include <functional>
#include <stdexcept>

using namespace Dxf;
using namespace Dxf::Testing;

namespace
{
class DPlainObject final : public DObject {};

class DDestructionCallback final : public DObject
{
public:
	explicit DDestructionCallback(std::function<void()> Callback) : m_Callback(std::move(Callback)) {}
	~DDestructionCallback() override { m_Callback(); }
private:
	std::function<void()> m_Callback;
};

struct FHookCounts
{
	int Initialize = 0;
	int Tick = 0;
	int Draw = 0;
	int Stop = 0;
	int Enter = 0;
};

class DHookObject final : public DGameObject
{
public:
	DHookObject(FHookCounts& Counts, std::function<void()> Init = {}, std::function<void()> Tick = {},
		std::function<void()> Draw = {})
		: m_pCounts(&Counts), m_Init(std::move(Init)), m_Tick(std::move(Tick)), m_Draw(std::move(Draw)) {}
protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounts->Initialize;
		if (m_Init) { m_Init(); }
		return {};
	}
	void OnTick(const FTickContext&) override
	{
		++m_pCounts->Tick;
		if (m_Tick) { m_Tick(); }
	}
	void OnDraw(FRenderContext&) const override
	{
		++m_pCounts->Draw;
		if (m_Draw) { m_Draw(); }
	}
	void OnDeinitialize() noexcept override { ++m_pCounts->Stop; }
private:
	FHookCounts* m_pCounts;
	std::function<void()> m_Init;
	std::function<void()> m_Tick;
	std::function<void()> m_Draw;
};

class DPreparationScene final : public DScene
{
public:
	DPreparationScene(FHookCounts& Counts, std::function<void()> Init = {})
		: m_pCounts(&Counts), m_Init(std::move(Init)) {}
protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounts->Initialize;
		if (m_Init) { m_Init(); }
		return {};
	}
	void OnEnter(const FSceneActivationContext&) noexcept override { ++m_pCounts->Enter; }
	void OnDeinitialize() noexcept override { ++m_pCounts->Stop; }
private:
	FHookCounts* m_pCounts;
	std::function<void()> m_Init;
};
}

TEST("Slot removal invalidates generation before running a reentrant destructor")
{
	TSlotMap<DObject> Storage;
	TObjectHandle<DObject> Replacement;
	auto Original = Storage.Insert(std::make_unique<DDestructionCallback>([&]()
	{
		Replacement = Storage.Insert(std::make_unique<DPlainObject>());
	}));
	REQUIRE(Storage.Remove(Original));
	REQUIRE(!Original);
	REQUIRE(Replacement);
	REQUIRE(Storage.Size() == 1);
	REQUIRE(Storage.Remove(Replacement));
	REQUIRE(Storage.Size() == 0);
}

TEST("Collection shutdown in Tick prevents later objects from ticking")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FHookCounts First;
	FHookCounts Second;
	FGameObjectCollection Objects;
	REQUIRE(Objects.Spawn<DHookObject>(First, std::function<void()>{}, [&]() { Objects.Shutdown_Internal(); }));
	REQUIRE(Objects.Spawn<DHookObject>(Second));
	REQUIRE(Objects.CommitBoundary_Internal({Assets}));
	FInputSnapshot Input;
	REQUIRE(Objects.Tick_Internal({Input, {}}));
	REQUIRE(First.Tick == 1);
	REQUIRE(Second.Tick == 0);
	REQUIRE(First.Stop == 1);
	REQUIRE(Second.Stop == 1);
	REQUIRE(Objects.Size() == 0);
}

TEST("Collection shutdown during preparation skips remaining initializations")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FHookCounts First;
	FHookCounts Second;
	FGameObjectCollection Objects;
	REQUIRE(Objects.Spawn<DHookObject>(First, [&]() { Objects.Shutdown_Internal(); }));
	REQUIRE(Objects.Spawn<DHookObject>(Second));
	REQUIRE(Objects.CommitBoundary_Internal({Assets}));
	REQUIRE(First.Initialize == 1);
	REQUIRE(Second.Initialize == 0);
	REQUIRE(First.Stop == 1);
	REQUIRE(Second.Stop == 0);
	REQUIRE(Objects.Size() == 0);
}

TEST("Standalone object Tick translates user exceptions to TResult")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FHookCounts Counts;
	DHookObject Object(Counts, {}, []() { throw std::runtime_error("tick failure"); });
	REQUIRE(Object.Initialize_Internal({Assets}));
	FInputSnapshot Input;
	auto Result = Object.Tick_Internal({Input, {}});
	REQUIRE(!Result);
	REQUIRE(Result.Error().Code == EErrorCode::UserException);
	Object.Shutdown_Internal();
	REQUIRE(Counts.Stop == 1);
}

TEST("Standalone object Draw translates unknown user exceptions to TResult")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FRenderSystem2D Renderer(Backend);
	FHookCounts Counts;
	DHookObject Object(Counts, {}, {}, []() { throw 42; });
	REQUIRE(Object.Initialize_Internal({Assets}));
	auto Result = Object.Draw_Internal(Renderer.GetContext());
	REQUIRE(!Result);
	REQUIRE(Result.Error().Code == EErrorCode::UserException);
	Object.Shutdown_Internal();
	REQUIRE(Counts.Stop == 1);
}

TEST("Navigator shutdown during preparation never activates the replacement")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FHookCounts Previous;
	FHookCounts Next;
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DPreparationScene>(Previous));
	REQUIRE(Scenes.Commit());
	REQUIRE(Scenes.RequestChange<DPreparationScene>(Next, [&]() { Scenes.Shutdown(); }));
	auto Commit = Scenes.Commit();
	REQUIRE(Commit);
	REQUIRE(!Commit.Value());
	REQUIRE(Next.Enter == 0);
	REQUIRE(Next.Stop == 1);
	REQUIRE(Previous.Stop == 1);
	REQUIRE(Scenes.GetCurrent() == nullptr);
}

TEST("Navigator quit during preparation retains current scene until shutdown")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FHookCounts Previous;
	FHookCounts Next;
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DPreparationScene>(Previous));
	REQUIRE(Scenes.Commit());
	auto* Current = Scenes.GetCurrent();
	REQUIRE(Scenes.RequestChange<DPreparationScene>(Next, [&]() { Scenes.RequestQuit(); }));
	auto Commit = Scenes.Commit();
	REQUIRE(Commit && !Commit.Value());
	REQUIRE(Scenes.GetCurrent() == Current);
	REQUIRE(Next.Enter == 0);
	REQUIRE(Next.Stop == 1);
	Scenes.Shutdown();
}

TEST("Asset paths reject embedded NUL before reaching a backend")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	const std::string Invalid("visible.bmp\0different.bmp", 25);
	REQUIRE(!Assets.LoadTexture(Invalid));
	REQUIRE(!Assets.LoadSound(Invalid));
	REQUIRE(Backend.GetTrace().TextureLoads == 0);
	REQUIRE(Backend.GetTrace().SoundLoads == 0);
}

TEST("Asset paths reject overlong UTF8 surrogate and truncated sequences")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	for (const auto& Invalid : {std::string("\xc0\xaf"), std::string("\xed\xa0\x80"),
		std::string("\xe3\x81"), std::string("\xf4\x90\x80\x80")})
	{
		REQUIRE(!Assets.LoadTexture(Invalid));
		REQUIRE(!Assets.LoadSound(Invalid));
	}
	REQUIRE(Backend.GetTrace().TextureLoads == 0);
	REQUIRE(Backend.GetTrace().SoundLoads == 0);
}

TEST("Asset paths accept valid Japanese and four byte UTF8")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	REQUIRE(Assets.LoadTexture("Assets/\xe7\x94\xbb\xe5\x83\x8f/\xf0\x9f\x90\xa6.bmp"));
}

TEST("Direct loaders reject empty paths and invalid sound storage enums")
{
	FFakeBackend Backend;
	FResourceRegistry Registry;
	FTextureLoader Textures(Backend, Registry);
	FSoundLoader Sounds(Backend, Registry);
	REQUIRE(!Textures.Load("", {}));
	REQUIRE(!Sounds.Load("", {}));
	FSoundLoadOptions Options;
	Options.Storage = static_cast<ESoundStorage>(777);
	REQUIRE(!Sounds.Load("a.wav", Options));
}

TEST("Font families reject embedded NUL and malformed UTF8")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FFontOptions Options;
	Options.Family = std::string("Meiryo\0X", 8);
	REQUIRE(!Assets.LoadFont(Options));
	Options.Family = "\xff";
	REQUIRE(!Assets.LoadFont(Options));
	REQUIRE(Backend.GetTrace().Fonts.empty());
}

TEST("Sound resources from another backend are rejected before duplication")
{
	FFakeBackend First;
	FFakeBackend Second;
	FAssetService Assets(First, First, First);
	FAudioPlayer Audio(Second);
	auto Sound = Assets.LoadSound("a.wav").Value();
	REQUIRE(!Audio.Play(Sound));
	REQUIRE(Second.GetTrace().Clones == 0);
}

TEST("Render execution failure prevents presenting a partial frame")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FRenderSystem2D Renderer(Backend);
	auto Texture = Assets.LoadTexture("a.bmp").Value();
	REQUIRE(Renderer.BeginFrame(100, 100));
	REQUIRE(Renderer.GetContext().Draw(Texture, {}));
	Backend.GetTrace().bFailDraw = true;
	REQUIRE(!Renderer.Flush());
	Backend.GetTrace().bFailDraw = false;
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().Presentations == 0);
	REQUIRE(Renderer.BeginFrame(100, 100));
	REQUIRE(Renderer.GetContext().Draw(Texture, {}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().Presentations == 1);
}

TEST("Resource registry rejects null records")
{
	FResourceRegistry Registry;
	REQUIRE(!Registry.Register(nullptr));
}

TEST("A scene destroyed during preparation is never activated")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FHookCounts Counts;
	FSceneNavigator Scenes(Assets, Audio);
	DPreparationScene* Scene = nullptr;
	auto Pending = std::make_unique<DPreparationScene>(Counts, [&]() { Scene->RequestDestroy_Internal(); });
	Scene = Pending.get();
	REQUIRE(Scenes.RequestChange(std::move(Pending)));
	auto Result = Scenes.Commit();
	REQUIRE(!Result);
	REQUIRE(Counts.Enter == 0);
	REQUIRE(Counts.Stop == 1);
	REQUIRE(Scenes.GetCurrent() == nullptr);
}

TEST("Text rendering rejects malformed UTF8 and embedded NUL before dispatch")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	auto Font = Assets.LoadFont().Value();
	FRenderSystem2D Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(320, 240));
	REQUIRE(!Renderer.GetContext().DrawText(Font, std::string("a\0b", 3), {}));
	REQUIRE(!Renderer.GetContext().DrawText(Font, std::string("\xc0\xaf", 2), {}));
	REQUIRE(Renderer.GetContext().DrawText(Font, "", {}));
	REQUIRE(Renderer.GetContext().DrawText(Font, "日本語", {}));
	REQUIRE(Renderer.EndFrame());
}

namespace
{
class DConstructorStop final : public DGameObject
{
public:
	explicit DConstructorStop(FGameObjectCollection& Collection)
	{
		Collection.Shutdown_Internal();
	}
};
}
TEST("A collection stopped by an object constructor must reject that spawn")
{
	FGameObjectCollection Collection;
	REQUIRE(!Collection.Spawn<DConstructorStop>(Collection));
	REQUIRE(Collection.Size() == 0);
	FHookCounts Counts;
	REQUIRE(!Collection.Spawn<DHookObject>(Counts));
}

TEST("Invalid sound storage must not alias an existing streamed cache entry")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	auto Stream = Assets.LoadSound("music.wav", {ESoundStorage::Stream});
	REQUIRE(Stream);
	REQUIRE(!Assets.LoadSound("music.wav", {static_cast<ESoundStorage>(99)}));
	REQUIRE(Backend.GetTrace().SoundLoads == 1);
}
