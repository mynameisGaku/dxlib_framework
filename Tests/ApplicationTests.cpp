#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/Application.h"
#include "Dxf/AppRunner.h"
#include "Dxf/GameScene.h"
#include <algorithm>
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
struct FAppObservation
{
    int Ticks = 0;
    int Draws = 0;
    int Stops = 0;
    int Destructs = 0;
    bool bInsideCallback = false;
    bool bDestroyedInsideCallback = false;
    bool bInputPressed = false;
    bool bFailInitialize = false;
    std::function<void(const FTickContext&)> Tick;
    std::function<void(FRenderContext&)> Draw;
};
class DObservedAppScene final : public DGameScene
{
public:
    explicit DObservedAppScene(FAppObservation& Observation) : m_pObservation(&Observation) {}
    ~DObservedAppScene() override
    {
        ++m_pObservation->Destructs;
        m_pObservation->bDestroyedInsideCallback |= m_pObservation->bInsideCallback;
    }
protected:
    TResult<void> OnInitialize(const FInitContext&) override
    {
        return m_pObservation->bFailInitialize
            ? TResult<void>::Failure(EErrorCode::InitializationFailed, "scene initialization failed") : TResult<void>{};
    }
    void OnTick(const FTickContext& Context) override
    {
        ++m_pObservation->Ticks;
        m_pObservation->bInputPressed = Context.Input.Pressed(EKey::Space);
        m_pObservation->bInsideCallback = true;
        if (m_pObservation->Tick) { m_pObservation->Tick(Context); }
        m_pObservation->bInsideCallback = false;
    }
    void OnDraw(FRenderContext& Render) const override
    {
        ++m_pObservation->Draws;
        if (m_pObservation->Draw) { m_pObservation->Draw(Render); }
    }
    void OnDeinitialize() noexcept override { ++m_pObservation->Stops; }
private:
    FAppObservation* m_pObservation;
};
FBackendServices MakeServices_Internal(FFakeBackend& Backend)
{
    return {Backend, Backend, Backend, Backend, Backend, Backend};
}
}
TEST("Application frame polls input once, dispatches and presents")
{
    FFakeBackend Backend; FAppObservation Observation; FApplication App(MakeServices_Internal(Backend));
    REQUIRE(App.Start(std::make_unique<DObservedAppScene>(Observation)));
    Backend.GetTrace().Input.Keys[static_cast<std::size_t>(EKey::Space)] = true;
    auto Step = App.Step(0.0); REQUIRE(Step && Step.Value());
    REQUIRE(Observation.Ticks == 1 && Observation.Draws == 1 && Observation.bInputPressed);
    REQUIRE(Backend.GetTrace().Presentations == 1);
    REQUIRE(App.Step(0.01)); REQUIRE(!Observation.bInputPressed);
    App.Shutdown(); REQUIRE(Observation.Stops == 1 && Observation.Destructs == 1);
    REQUIRE(Backend.GetTrace().Events.back() == "shutdown");
}
TEST("Application rolls back failed platform initialization without double shutdown")
{
    FFakeBackend Backend; Backend.GetTrace().bFailPlatform = true; FApplication App(MakeServices_Internal(Backend));
    REQUIRE(!App.Start(std::make_unique<DScene>()));
    App.Shutdown();
    REQUIRE(std::count(Backend.GetTrace().Events.begin(), Backend.GetTrace().Events.end(), "shutdown") == 0);
    REQUIRE(!App.Start(std::make_unique<DScene>()));
}
TEST("Application rolls back failed initial scene and ends session")
{
    FFakeBackend Backend; FAppObservation Observation; Observation.bFailInitialize = true;
    FApplication App(MakeServices_Internal(Backend));
    REQUIRE(!App.Start(std::make_unique<DObservedAppScene>(Observation)));
    REQUIRE(Observation.Stops == 1 && Observation.Destructs == 1);
    REQUIRE(Backend.GetTrace().Events.back() == "shutdown");
}
TEST("Application retains current scene after a later failed transition")
{
    FFakeBackend Backend; FAppObservation First, Failed; Failed.bFailInitialize = true;
    FApplication App(MakeServices_Internal(Backend));
    REQUIRE(App.Start(std::make_unique<DObservedAppScene>(First)));
    REQUIRE(App.GetScenes().RequestChange<DObservedAppScene>(Failed));
    auto Step = App.Step(0); REQUIRE(Step && Step.Value());
    REQUIRE(First.Ticks == 1 && First.Stops == 0 && Failed.Stops == 1);
    REQUIRE(App.GetScenes().GetLastTransitionError().has_value());
}
TEST("Application defers shutdown until the user callback has returned")
{
    FFakeBackend Backend; FAppObservation Observation; FApplication App(MakeServices_Internal(Backend));
    Observation.Tick = [&](const FTickContext&) { App.Shutdown(); REQUIRE(Observation.Destructs == 0); };
    REQUIRE(App.Start(std::make_unique<DObservedAppScene>(Observation)));
    auto Step = App.Step(0); REQUIRE(Step && !Step.Value());
    REQUIRE(Observation.Destructs == 1 && !Observation.bDestroyedInsideCallback);
    REQUIRE(Backend.GetTrace().Presentations == 0);
}
TEST("Application rejects recursive Step without ending the outer frame")
{
    FFakeBackend Backend; FAppObservation Observation; FApplication App(MakeServices_Internal(Backend));
    Observation.Tick = [&](const FTickContext&) { REQUIRE(!App.Step(0.01)); };
    REQUIRE(App.Start(std::make_unique<DObservedAppScene>(Observation)));
    auto Step = App.Step(0); REQUIRE(Step && Step.Value()); REQUIRE(Backend.GetTrace().Presentations == 1);
}
TEST("Application converts user tick exceptions and releases all services")
{
    FFakeBackend Backend; FAppObservation Observation; FApplication App(MakeServices_Internal(Backend));
    Observation.Tick = [](const FTickContext&) { throw std::runtime_error("user tick"); };
    REQUIRE(App.Start(std::make_unique<DObservedAppScene>(Observation)));
    REQUIRE(!App.Step(0)); REQUIRE(Observation.Stops == 1); REQUIRE(Backend.GetTrace().Events.back() == "shutdown");
}
TEST("Application stops cleanly when event pumping reports quit")
{
    FFakeBackend Backend; FApplication App(MakeServices_Internal(Backend));
    REQUIRE(App.Start(std::make_unique<DScene>())); Backend.GetTrace().bQuit = true;
    auto Step = App.Step(0); REQUIRE(Step && !Step.Value()); REQUIRE(Backend.GetTrace().Presentations == 0);
}
TEST("Application rejects backwards frame time before executing another frame")
{
    FFakeBackend Backend; FApplication App(MakeServices_Internal(Backend));
    REQUIRE(App.Start(std::make_unique<DScene>())); REQUIRE(App.Step(10));
    REQUIRE(!App.Step(9)); REQUIRE(Backend.GetTrace().Presentations == 1);
}
TEST("Application invalidates surviving asset references before ending DxLib")
{
    FFakeBackend Backend; FApplication App(MakeServices_Internal(Backend));
    REQUIRE(App.Start(std::make_unique<DScene>()));
    auto Texture = App.GetAssets().LoadTexture("player.bmp").Value();
    App.Shutdown(); REQUIRE(!Texture.IsValid());
    const auto& Events = Backend.GetTrace().Events;
    REQUIRE(std::find(Events.begin(), Events.end(), "delete-texture") < std::find(Events.begin(), Events.end(), "shutdown"));
}
TEST("AppRunner executes until a scene requests quit")
{
    FFakeBackend Backend; FAppObservation Observation; FApplication App(MakeServices_Internal(Backend));
    Observation.Tick = [](const FTickContext& Context) { Context.Scenes->RequestQuit(); };
    FAppRunner Runner; auto Result = Runner.Run(App, std::make_unique<DObservedAppScene>(Observation));
    REQUIRE(Result); REQUIRE(Observation.Ticks == 1 && Observation.Destructs == 1);
}
