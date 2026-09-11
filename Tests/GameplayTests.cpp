#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/GameScene.h"
#include "Dxf/AssetService.h"
#include "Dxf/GameObject.h"
#include "Dxf/GameObjectComponent.h"
#include "Dxf/GameObjectCollection.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/GameInstance.h"
#include "Dxf/RenderSystem2D.h"
#include <functional>
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
struct FCounters
{
    int Initialize = 0;
    int Tick = 0;
    int Draw = 0;
    int Deinitialize = 0;
    int Destruct = 0;
    int Enter = 0;
    int Exit = 0;
    bool bFailInitialize = false;
    std::vector<std::string> Order;
};
class DCountingComponent final : public DGameObjectComponent
{
public:
    explicit DCountingComponent(FCounters& Counters, std::function<void()> Tick = {}, std::function<void()> Init = {})
        : m_pCounters(&Counters), m_Tick(std::move(Tick)), m_Init(std::move(Init)) {}
    ~DCountingComponent() override { ++m_pCounters->Destruct; }
protected:
    TResult<void> OnInitialize(const FInitContext&) override
    { ++m_pCounters->Initialize; if (m_Init) { m_Init(); } return m_pCounters->bFailInitialize ? TResult<void>::Failure(EErrorCode::InitializationFailed, "component init") : TResult<void>{}; }
    void OnTick(const FTickContext&) override { ++m_pCounters->Tick; if (m_Tick) { m_Tick(); } }
    void OnDraw(FRenderContext&) const override { ++m_pCounters->Draw; }
    void OnDeinitialize() noexcept override { ++m_pCounters->Deinitialize; m_pCounters->Order.push_back("component-stop"); }
private:
    FCounters* m_pCounters;
    std::function<void()> m_Tick;
    std::function<void()> m_Init;
};
class DCountingObject : public DGameObject
{
public:
    explicit DCountingObject(FCounters& Counters, std::function<void()> Tick = {}, std::function<void()> Init = {}, std::function<void()> Stop = {})
        : m_pCounters(&Counters), m_Tick(std::move(Tick)), m_Init(std::move(Init)), m_Stop(std::move(Stop)) {}
    ~DCountingObject() override { ++m_pCounters->Destruct; }
protected:
    TResult<void> OnInitialize(const FInitContext&) override
    { ++m_pCounters->Initialize; if (m_Init) { m_Init(); } return m_pCounters->bFailInitialize ? TResult<void>::Failure(EErrorCode::InitializationFailed, "object init") : TResult<void>{}; }
    void OnTick(const FTickContext&) override { ++m_pCounters->Tick; if (m_Tick) { m_Tick(); } }
    void OnDraw(FRenderContext&) const override { ++m_pCounters->Draw; }
    void OnDeinitialize() noexcept override
    { ++m_pCounters->Deinitialize; m_pCounters->Order.push_back("object-stop"); if (m_Stop) { m_Stop(); } }
private:
    FCounters* m_pCounters;
    std::function<void()> m_Tick;
    std::function<void()> m_Init;
    std::function<void()> m_Stop;
};
class DCountingScene : public DGameScene
{
public:
    explicit DCountingScene(FCounters& Counters, std::function<void()> Enter = {}) : m_pCounters(&Counters), m_Enter(std::move(Enter)) {}
    ~DCountingScene() override { ++m_pCounters->Destruct; }
protected:
    TResult<void> OnInitialize(const FInitContext&) override
    { ++m_pCounters->Initialize; return m_pCounters->bFailInitialize ? TResult<void>::Failure(EErrorCode::InitializationFailed, "scene init") : TResult<void>{}; }
    void OnTick(const FTickContext&) override { ++m_pCounters->Tick; }
    void OnDraw(FRenderContext&) const override { ++m_pCounters->Draw; }
    void OnDeinitialize() noexcept override { ++m_pCounters->Deinitialize; }
    void OnEnter(const FSceneActivationContext&) noexcept override { ++m_pCounters->Enter; if (m_Enter) { m_Enter(); } }
    void OnExit() noexcept override { ++m_pCounters->Exit; }
private:
    FCounters* m_pCounters;
    std::function<void()> m_Enter;
};
class FWorldFixture
{
public:
    FWorldFixture() : m_Assets(m_Backend, m_Backend, m_Backend), m_Audio(m_Backend), m_Navigator(m_Assets, m_Audio), m_Renderer(m_Backend) {}
    FInitContext GetInit() { return {m_Assets}; }
    FTickContext GetTick() { FFrameTime Time; Time.DeltaSeconds = 0.016; Time.UnscaledDeltaSeconds = 0.016; return {m_Input, Time}; }
    FAssetService& GetAssets() { return m_Assets; }
    FSceneNavigator& GetScenes() { return m_Navigator; }
    FRenderSystem2D& GetRenderer() { return m_Renderer; }
    const FInputSnapshot& GetInput() const { return m_Input; }
private:
    FFakeBackend m_Backend;
    FAssetService m_Assets;
    FAudioPlayer m_Audio;
    FSceneNavigator m_Navigator;
    FRenderSystem2D m_Renderer;
    FInputSnapshot m_Input;
};
}
TEST("Spawn remains pending until boundary and initializes exactly once")
{
    FWorldFixture World; FCounters Counts; FGameObjectCollection Objects;
    auto Object = Objects.Spawn<DCountingObject>(Counts).Value();
    REQUIRE(Objects.Tick_Internal(World.GetTick())); REQUIRE(Counts.Tick == 0);
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
    REQUIRE(Objects.Tick_Internal(World.GetTick())); REQUIRE(Counts.Initialize == 1 && Counts.Tick == 1);
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); REQUIRE(Counts.Initialize == 1); REQUIRE(Object.Get());
}
TEST("GameObject override cannot bypass automatic component dispatch")
{
    FWorldFixture World; FCounters Parent; FCounters Child; FGameObjectCollection Objects;
    auto Object = Objects.Spawn<DCountingObject>(Parent).Value();
    auto Component = Object.Get()->AddComponent<DCountingComponent>(Child).Value();
    REQUIRE(Component.Get()->GetOwner() == Object.Get());
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
    REQUIRE(Objects.Tick_Internal(World.GetTick()));
    REQUIRE(World.GetRenderer().BeginFrame(640, 480)); REQUIRE(Objects.Draw_Internal(World.GetRenderer().GetContext())); REQUIRE(World.GetRenderer().EndFrame());
    REQUIRE(Parent.Tick == 1 && Child.Tick == 1); REQUIRE(Parent.Draw == 1 && Child.Draw == 1);
}
TEST("Spawn during tick is initialized only at the next boundary")
{
    FWorldFixture World; FCounters A; FCounters B; FGameObjectCollection Objects; bool bSpawned = false;
    REQUIRE(Objects.Spawn<DCountingObject>(A, [&] { if (!bSpawned) { bSpawned = true; REQUIRE(Objects.Spawn<DCountingObject>(B)); } }));
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
    REQUIRE(Objects.Tick_Internal(World.GetTick())); REQUIRE(B.Initialize == 0 && B.Tick == 0);
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); REQUIRE(Objects.Tick_Internal(World.GetTick())); REQUIRE(B.Initialize == 1 && B.Tick == 1);
}
TEST("Destroying a later sibling suppresses its remaining callbacks immediately")
{
    FWorldFixture World; FCounters A; FCounters B; FGameObjectCollection Objects; TObjectHandle<DCountingObject> Victim;
    REQUIRE(Objects.Spawn<DCountingObject>(A, [&] { Objects.Destroy(Victim); }));
    Victim = Objects.Spawn<DCountingObject>(B).Value();
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); REQUIRE(Objects.Tick_Internal(World.GetTick()));
    REQUIRE(!Victim.Get()); REQUIRE(B.Tick == 0); REQUIRE(B.Destruct == 0);
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); REQUIRE(B.Deinitialize == 1 && B.Destruct == 1);
}
TEST("Self destruction in tick skips owned components and later draw")
{
    FWorldFixture World; FCounters A; FCounters B; FGameObjectCollection Objects; TObjectHandle<DCountingObject> Self;
    Self = Objects.Spawn<DCountingObject>(A, [&] { Self.Get()->Destroy(); }).Value();
    REQUIRE(Self.Get()->AddComponent<DCountingComponent>(B));
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); REQUIRE(Objects.Tick_Internal(World.GetTick()));
    REQUIRE(B.Tick == 0); REQUIRE(World.GetRenderer().BeginFrame(640, 480)); REQUIRE(Objects.Draw_Internal(World.GetRenderer().GetContext())); REQUIRE(A.Draw == 0 && B.Draw == 0);
}
TEST("Destroy before initialization never calls user initialization or shutdown hooks")
{
    FWorldFixture World; FCounters Counts; FGameObjectCollection Objects;
    auto Handle = Objects.Spawn<DCountingObject>(Counts).Value(); REQUIRE(Objects.Destroy(Handle));
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
    REQUIRE(Counts.Initialize == 0 && Counts.Deinitialize == 0 && Counts.Destruct == 1);
}
TEST("Initialization failure rolls back the object and invalidates the handle")
{
    FWorldFixture World; FCounters Counts; Counts.bFailInitialize = true; FGameObjectCollection Objects;
    auto Handle = Objects.Spawn<DCountingObject>(Counts).Value();
    Objects.FreezeBoundary_Internal(); REQUIRE(!Objects.CommitBoundary_Internal(World.GetInit()));
    REQUIRE(!Handle.Get()); REQUIRE(Counts.Deinitialize == 1 && Counts.Destruct == 1);
}
TEST("Component initialization failure rolls back its parent transaction")
{
    FWorldFixture World; FCounters Parent; FCounters Child; Child.bFailInitialize = true; FGameObjectCollection Objects;
    auto Handle = Objects.Spawn<DCountingObject>(Parent).Value(); REQUIRE(Handle.Get()->AddComponent<DCountingComponent>(Child));
    Objects.FreezeBoundary_Internal(); REQUIRE(!Objects.CommitBoundary_Internal(World.GetInit()));
    REQUIRE(Parent.Deinitialize == 1 && Child.Deinitialize == 1); REQUIRE(!Handle.Get());
}
TEST("Shutdown is idempotent and stops components before their owner")
{
    FWorldFixture World; FCounters Counts; FGameObjectCollection Objects;
    auto Handle = Objects.Spawn<DCountingObject>(Counts).Value(); REQUIRE(Handle.Get()->AddComponent<DCountingComponent>(Counts));
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
    Objects.Shutdown_Internal(); Objects.Shutdown_Internal();
    REQUIRE(Counts.Order == std::vector<std::string>({"component-stop", "object-stop"})); REQUIRE(Counts.Deinitialize == 2);
    REQUIRE(!Objects.Spawn<DCountingObject>(Counts));
}
TEST("Mutations submitted from shutdown wait for the next boundary")
{
    FWorldFixture World; FCounters A; FCounters B; FGameObjectCollection Objects;
    auto Handle = Objects.Spawn<DCountingObject>(A, std::function<void()>{}, std::function<void()>{}, [&] { (void)Objects.Spawn<DCountingObject>(B); }).Value();
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); Objects.Destroy(Handle);
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); REQUIRE(B.Initialize == 0);
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); REQUIRE(B.Initialize == 1);
}
TEST("Children added to an existing object during another initializer wait for a new boundary")
{
    FWorldFixture World; FCounters A; FCounters B; FCounters Child; FGameObjectCollection Objects;
    auto Existing = Objects.Spawn<DCountingObject>(A).Value(); Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
    REQUIRE(Objects.Spawn<DCountingObject>(B, std::function<void()>{}, [&] { REQUIRE(Existing.Get()->AddComponent<DCountingComponent>(Child)); }));
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); REQUIRE(Child.Initialize == 0);
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); REQUIRE(Child.Initialize == 1);
}
TEST("Update ordering is deterministic after slot reuse")
{
    FWorldFixture World; FCounters Counts; FGameObjectCollection Objects; std::vector<int> Order;
    auto Old = Objects.Spawn<DCountingObject>(Counts).Value();
    REQUIRE(Objects.Spawn<DCountingObject>(Counts, [&] { Order.push_back(2); }));
    Objects.Destroy(Old); Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
    REQUIRE(Objects.Spawn<DCountingObject>(Counts, [&] { Order.push_back(3); }));
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit())); REQUIRE(Objects.Tick_Internal(World.GetTick()));
    REQUIRE(Order == std::vector<int>({2, 3}));
}
TEST("Pause skips normal objects but honors tick-when-paused components")
{
    FWorldFixture World; FCounters Parent; FCounters Child; FGameObjectCollection Objects;
    auto Handle = Objects.Spawn<DCountingObject>(Parent).Value();
    auto Component = Handle.Get()->AddComponent<DCountingComponent>(Child).Value(); Component.Get()->SetTickWhenPaused(true);
    Objects.FreezeBoundary_Internal(); REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
    auto Context = World.GetTick(); Context.Time.bPaused = true; Context.Time.DeltaSeconds = 0.0;
    REQUIRE(Objects.Tick_Internal(Context)); REQUIRE(Parent.Tick == 0 && Child.Tick == 1);
}
TEST("Scene override cannot bypass object dispatch")
{
    FWorldFixture World; FCounters SceneCounts; FCounters ObjectCounts;
    auto Scene = std::make_unique<DCountingScene>(SceneCounts); REQUIRE(Scene->Spawn<DCountingObject>(ObjectCounts));
    REQUIRE(World.GetScenes().RequestChange(std::move(Scene))); REQUIRE(World.GetScenes().Commit());
    REQUIRE(World.GetScenes().Tick(World.GetTick().Time, World.GetInput()));
    REQUIRE(SceneCounts.Tick == 1 && ObjectCounts.Tick == 1); World.GetScenes().Shutdown();
}
TEST("Scene switch preserves the active scene until commit")
{
    FWorldFixture World; FCounters A; FCounters B;
    REQUIRE(World.GetScenes().RequestChange(std::make_unique<DCountingScene>(A))); REQUIRE(World.GetScenes().Commit());
    auto* Old = World.GetScenes().GetCurrent(); REQUIRE(World.GetScenes().RequestChange(std::make_unique<DCountingScene>(B)));
    REQUIRE(World.GetScenes().GetCurrent() == Old); REQUIRE(B.Initialize == 0);
    REQUIRE(World.GetScenes().Commit()); REQUIRE(A.Exit == 1 && A.Deinitialize == 1 && A.Destruct == 1); REQUIRE(B.Enter == 1);
    World.GetScenes().Shutdown();
}
TEST("Failed scene preparation leaves current scene active")
{
    FWorldFixture World; FCounters A; FCounters B; B.bFailInitialize = true;
    REQUIRE(World.GetScenes().RequestChange(std::make_unique<DCountingScene>(A))); REQUIRE(World.GetScenes().Commit());
    auto* Old = World.GetScenes().GetCurrent(); REQUIRE(World.GetScenes().RequestChange(std::make_unique<DCountingScene>(B)));
    REQUIRE(!World.GetScenes().Commit()); REQUIRE(World.GetScenes().GetCurrent() == Old);
    REQUIRE(A.Exit == 0 && B.Enter == 0 && B.Deinitialize == 1 && B.Destruct == 1);
    World.GetScenes().Shutdown();
}
TEST("A request made inside OnEnter survives for the next scene boundary")
{
    FWorldFixture World; FCounters A; FCounters B;
    auto Scene = std::make_unique<DCountingScene>(A, [&] { (void)World.GetScenes().RequestChange(std::make_unique<DCountingScene>(B)); });
    REQUIRE(World.GetScenes().RequestChange(std::move(Scene))); REQUIRE(World.GetScenes().Commit()); REQUIRE(B.Initialize == 0);
    REQUIRE(World.GetScenes().Commit()); REQUIRE(B.Enter == 1); World.GetScenes().Shutdown();
}
TEST("Multiple pending scene requests use last-request-wins without initializing discarded scenes")
{
    FWorldFixture World; FCounters A; FCounters B;
    REQUIRE(World.GetScenes().RequestChange(std::make_unique<DCountingScene>(A)));
    REQUIRE(World.GetScenes().RequestChange(std::make_unique<DCountingScene>(B))); REQUIRE(A.Destruct == 1 && A.Initialize == 0);
    REQUIRE(World.GetScenes().Commit()); REQUIRE(B.Enter == 1); World.GetScenes().Shutdown();
}
TEST("Handles from a destroyed scene cannot resolve into the next scene")
{
    FWorldFixture World; FCounters SceneCounts; FCounters Counts;
    auto First = std::make_unique<DCountingScene>(SceneCounts); auto Old = First->Spawn<DCountingObject>(Counts).Value();
    REQUIRE(World.GetScenes().RequestChange(std::move(First))); REQUIRE(World.GetScenes().Commit()); REQUIRE(Old.Get());
    auto Second = std::make_unique<DCountingScene>(SceneCounts); auto New = Second->Spawn<DCountingObject>(Counts).Value();
    REQUIRE(World.GetScenes().RequestChange(std::move(Second))); REQUIRE(World.GetScenes().Commit());
    REQUIRE(!Old.Get()); REQUIRE(New.Get()); World.GetScenes().Shutdown();
}
TEST("GameObject initialization exception is converted into an error and cleaned up")
{
    FWorldFixture World; FCounters Counts; FGameObjectCollection Objects;
    auto Handle = Objects.Spawn<DCountingObject>(Counts, std::function<void()>{}, [] { throw std::runtime_error("init exception"); }).Value();
    Objects.FreezeBoundary_Internal(); auto Result = Objects.CommitBoundary_Internal(World.GetInit());
    REQUIRE(!Result && Result.Error().Code == EErrorCode::UserException); REQUIRE(!Handle.Get()); REQUIRE(Counts.Deinitialize == 1);
}
