#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/GameObject.h"
#include "Dxf/GameScene.h"
#include "Dxf/RenderSystem2D.h"
#include "Dxf/SceneNavigator.h"
#include <functional>
#include <stdexcept>

using namespace Dxf;
using namespace Dxf::Testing;

namespace
{
struct FSceneCounts
{
	int Construct = 0;
	int Initialize = 0;
	int Enter = 0;
	int Tick = 0;
	int Draw = 0;
	int Stop = 0;
	int Destroy = 0;
};

class DSceneProbe final : public DGameScene
{
public:
	explicit DSceneProbe(FSceneCounts& Counts, std::function<void()> Destruct = {},
		std::function<void()> Tick = {}, std::function<void()> Draw = {})
		: m_pCounts(&Counts), m_Destruct(std::move(Destruct)), m_Tick(std::move(Tick)), m_Draw(std::move(Draw))
	{
		++m_pCounts->Construct;
	}
	~DSceneProbe() override
	{
		++m_pCounts->Destroy;
		if (m_Destruct)
		{
			m_Destruct();
		}
	}
protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounts->Initialize;
		return {};
	}
	void OnEnter(const FSceneActivationContext&) noexcept override
	{
		++m_pCounts->Enter;
	}
	void OnTick(const FTickContext&) override
	{
		++m_pCounts->Tick;
		if (m_Tick)
		{
			m_Tick();
		}
	}
	void OnDraw(FRenderContext&) const override
	{
		++m_pCounts->Draw;
		if (m_Draw)
		{
			m_Draw();
		}
	}
	void OnDeinitialize() noexcept override
	{
		++m_pCounts->Stop;
	}
private:
	FSceneCounts* m_pCounts;
	std::function<void()> m_Destruct;
	std::function<void()> m_Tick;
	std::function<void()> m_Draw;
};

class DActorProbe final : public DGameObject
{
public:
	explicit DActorProbe(FSceneCounts& Counts, std::function<void()> Tick = {}, std::function<void()> Init = {})
		: m_pCounts(&Counts), m_Tick(std::move(Tick)), m_Init(std::move(Init))
	{
	}
protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounts->Initialize;
		if (m_Init)
		{
			m_Init();
		}
		return {};
	}
	void OnTick(const FTickContext&) override
	{
		++m_pCounts->Tick;
		if (m_Tick)
		{
			m_Tick();
		}
	}
	void OnDraw(FRenderContext&) const override
	{
		++m_pCounts->Draw;
	}
	void OnDeinitialize() noexcept override
	{
		++m_pCounts->Stop;
	}
private:
	FSceneCounts* m_pCounts;
	std::function<void()> m_Tick;
	std::function<void()> m_Init;
};

class DThrowingScene final : public DScene
{
public:
	DThrowingScene()
	{
		throw std::runtime_error("scene constructor failed");
	}
};
}

TEST("scene factory exceptions become results without replacing the current scene")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FSceneCounts Current;
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Current));
	REQUIRE(Scenes.Commit());
	auto Result = Scenes.RequestChange<DThrowingScene>();
	REQUIRE(!Result && Result.Error().Code == EErrorCode::UserException);
	REQUIRE(Scenes.GetCurrent() != nullptr && Current.Stop == 0);
}

TEST("stopped scene navigator rejects a factory before invoking its constructor")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FSceneCounts Counts;
	FSceneNavigator Scenes(Assets, Audio);
	Scenes.Shutdown();
	REQUIRE(!Scenes.RequestChange<DSceneProbe>(Counts));
	REQUIRE(Counts.Construct == 0);
}

TEST("destroy requested pending scene is rejected at request time")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FSceneNavigator Scenes(Assets, Audio);
	auto Scene = std::make_unique<DScene>();
	Scene->RequestDestroy_Internal();
	REQUIRE(!Scenes.RequestChange(std::move(Scene)));
}

TEST("pending scene destructor cannot commit a transition reentrantly")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FSceneCounts Old;
	FSceneCounts Next;
	bool bRejected = false;
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Old, [&]
	{
		bRejected = !Scenes.Commit();
	}));
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Next));
	REQUIRE(bRejected);
	REQUIRE(Next.Initialize == 0);
	REQUIRE(Scenes.Commit());
	REQUIRE(Next.Enter == 1);
}

TEST("pending scene destructor cannot overwrite the replacement request")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FSceneCounts Old;
	FSceneCounts Next;
	FSceneCounts Nested;
	bool bRejected = false;
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Old, [&]
	{
		bRejected = !Scenes.RequestChange<DSceneProbe>(Nested);
	}));
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Next));
	REQUIRE(bRejected);
	REQUIRE(Scenes.Commit());
	REQUIRE(Next.Enter == 1 && Nested.Enter == 0);
}

TEST("shutdown from pending scene destructor rejects the interrupted replacement")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FSceneCounts Old;
	FSceneCounts Next;
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Old, [&] { Scenes.Shutdown(); }));
	REQUIRE(!Scenes.RequestChange<DSceneProbe>(Next));
	REQUIRE(Scenes.WantsQuit());
	REQUIRE(Scenes.GetCurrent() == nullptr);
	REQUIRE(Next.Initialize == 0 && Next.Destroy == 1);
}

TEST("quit before commit never initializes the queued scene")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FSceneCounts Next;
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Next));
	Scenes.RequestQuit();
	REQUIRE(Scenes.Commit());
	REQUIRE(Next.Initialize == 0);
}

TEST("quit from scene update suppresses its child object updates")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FSceneCounts SceneCounts;
	FSceneCounts ActorCounts;
	FSceneNavigator Scenes(Assets, Audio);
	auto Scene = std::make_unique<DSceneProbe>(SceneCounts, std::function<void()>{}, [&] { Scenes.RequestQuit(); });
	REQUIRE(Scene->Spawn<DActorProbe>(ActorCounts));
	REQUIRE(Scenes.RequestChange(std::move(Scene)));
	REQUIRE(Scenes.Commit());
	REQUIRE(Scenes.Tick({}, {}));
	REQUIRE(SceneCounts.Tick == 1 && ActorCounts.Tick == 0);
}

TEST("quit from one object update suppresses later siblings in the same scene")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FSceneCounts SceneCounts;
	FSceneCounts First;
	FSceneCounts Second;
	FSceneNavigator Scenes(Assets, Audio);
	auto Scene = std::make_unique<DSceneProbe>(SceneCounts);
	REQUIRE(Scene->Spawn<DActorProbe>(First, [&] { Scenes.RequestQuit(); }));
	REQUIRE(Scene->Spawn<DActorProbe>(Second));
	REQUIRE(Scenes.RequestChange(std::move(Scene)));
	REQUIRE(Scenes.Commit());
	REQUIRE(Scenes.Tick({}, {}));
	REQUIRE(First.Tick == 1 && Second.Tick == 0);
}

TEST("shutdown from scene drawing suppresses child drawing and deinitializes once")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FRenderSystem2D Render(Backend);
	FSceneCounts SceneCounts;
	FSceneCounts ActorCounts;
	FSceneNavigator Scenes(Assets, Audio);
	auto Scene = std::make_unique<DSceneProbe>(SceneCounts, std::function<void()>{}, std::function<void()>{},
		[&] { Scenes.Shutdown(); });
	REQUIRE(Scene->Spawn<DActorProbe>(ActorCounts));
	REQUIRE(Scenes.RequestChange(std::move(Scene)));
	REQUIRE(Scenes.Commit());
	REQUIRE(Render.BeginFrame(320, 240));
	REQUIRE(Scenes.Draw(Render.GetContext()));
	REQUIRE(ActorCounts.Draw == 0);
	REQUIRE(SceneCounts.Stop == 1 && ActorCounts.Stop == 1);
	REQUIRE(Scenes.GetCurrent() == nullptr);
	Render.CancelFrame();
}

TEST("quit while preparing an object cancels the remaining scene initialization")
{
	FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
	FAudioPlayer Audio(Backend);
	FSceneCounts SceneCounts;
	FSceneCounts First;
	FSceneCounts Second;
	FSceneNavigator Scenes(Assets, Audio);
	auto Scene = std::make_unique<DSceneProbe>(SceneCounts);
	REQUIRE(Scene->Spawn<DActorProbe>(First, std::function<void()>{}, [&] { Scenes.RequestQuit(); }));
	REQUIRE(Scene->Spawn<DActorProbe>(Second));
	REQUIRE(Scenes.RequestChange(std::move(Scene)));
	const auto Transition = Scenes.Commit();
	REQUIRE(!Transition || !Transition.Value());
	REQUIRE(First.Initialize == 1 && Second.Initialize == 0);
	REQUIRE(SceneCounts.Enter == 0);
	REQUIRE(First.Stop == 1 && Second.Stop == 0);
}
