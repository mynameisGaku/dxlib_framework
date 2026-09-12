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
