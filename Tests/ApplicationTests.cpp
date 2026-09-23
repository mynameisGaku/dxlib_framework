#include "Toolbox/UniquePtr.h"
#include "Support/Test.h"
#include "Support/ModelSmokeStep.h"
#include "Support/FakeBackend.h"
#include "Dxf/Application.h"
#include "Dxf/AppRunner.h"
#include "Dxf/GameScene.h"
#include "Toolbox/Algorithm.h"
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
// アプリ経由の更新・描画・終了と再入状態の観測結果。
struct FAppObservation
{
	// 更新フックの呼び出し回数。
	Toolbox::int32 Ticks = 0;
	// 描画フックの呼び出し回数。
	Toolbox::int32 Draws = 0;
	// 終了フックの呼び出し回数。
	Toolbox::int32 Stops = 0;
	// デストラクターの呼び出し回数。
	Toolbox::int32 Destructs = 0;
	// コールバックを実行中か。
	bool bInsideCallback = false;
	// コールバックが終わる前に破棄されたか。
	bool bDestroyedInsideCallback = false;
	// 更新時に押下イベントが届いたか。
	bool bInputPressed = false;
	// 初期化失敗を発生させるか。
	bool bFailInitialize = false;
	// 更新時に実行する検証用処理。
	Toolbox::TFunction<void(const FTickContext&)> Tick;
	// 描画時に実行する検証用処理。
	Toolbox::TFunction<void(FRenderContext&)> Draw;
};
// アプリからシーンへの呼び出しと入力状態を記録する。
class DObservedAppScene final : public DGameScene
{
public:
	// 検証に必要な依存先と初期状態を設定する。
	explicit DObservedAppScene(FAppObservation& Observation) : m_pObservation(&Observation)
	{
	}
	// 所有データを解放し、必要な破棄の観測を行う。
	~DObservedAppScene() override
	{
		++m_pObservation->Destructs;
		m_pObservation->bDestroyedInsideCallback |= m_pObservation->bInsideCallback;
	}

protected:
	// 初期化の呼び出しを観測し、指定した検証条件を適用する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		return m_pObservation->bFailInitialize
		           ? TResult<void>::Failure(EErrorCode::InitializationFailed, "scene initialization failed")
		           : TResult<void>{};
	}
	// 更新の呼び出しを観測し、指定された処理を実行する。
	void OnTick(const FTickContext& Context) override
	{
		++m_pObservation->Ticks;
		m_pObservation->bInputPressed = Context.Input.WasPressed(EKey::Space);
		m_pObservation->bInsideCallback = true;
		if (m_pObservation->Tick)
		{
			m_pObservation->Tick(Context);
		}
		m_pObservation->bInsideCallback = false;
	}
	// 描画の呼び出しを観測し、指定された処理を実行する。
	void OnDraw(FRenderContext& Render) const override
	{
		++m_pObservation->Draws;
		if (m_pObservation->Draw)
		{
			m_pObservation->Draw(Render);
		}
	}
	// 終了処理の呼び出しを観測する。
	void OnDeinitialize() noexcept override
	{
		++m_pObservation->Stops;
	}

private:
	// 外部の観測結果への非所有参照。
	FAppObservation* m_pObservation;
};
// 一つの疑似バックエンドを全サービスへ接続する。
FBackendServices MakeServices_Internal(FFakeBackend& Backend)
{
	return {Backend, Backend, Backend, Backend, Backend, Backend};
}
} // namespace
TEST("Application frame polls input once, dispatches and presents")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// コールバックから書き込む観測結果。
	FAppObservation Observation;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	REQUIRE(App.Start(Toolbox::MakeUnique<DObservedAppScene>(Observation)));
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = true;
	// 一フレーム処理の成否と継続状態。
	auto Step = App.Step(0.0);
	REQUIRE(Step && Step.Value());
	REQUIRE(Observation.Ticks == 1 && Observation.Draws == 1 && Observation.bInputPressed);
	REQUIRE(Backend.GetTrace().Presentations == 1);
	REQUIRE(App.Step(0.01));
	REQUIRE(!Observation.bInputPressed);
	App.Shutdown();
	REQUIRE(Observation.Stops == 1 && Observation.Destructs == 1);
	REQUIRE(Backend.GetTrace().Events.Back() == "shutdown");
}
TEST("Application rolls back failed platform initialization without double shutdown")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	Backend.GetTrace().bFailPlatform = true;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	REQUIRE(!App.Start(Toolbox::MakeUnique<DScene>()));
	App.Shutdown();
	REQUIRE(Toolbox::Count(Backend.GetTrace().Events.Begin(), Backend.GetTrace().Events.End(), "shutdown") == 0);
	REQUIRE(!App.Start(Toolbox::MakeUnique<DScene>()));
}
TEST("Application rolls back failed initial scene and ends session")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// コールバックから書き込む観測結果。
	FAppObservation Observation;
	Observation.bFailInitialize = true;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	REQUIRE(!App.Start(Toolbox::MakeUnique<DObservedAppScene>(Observation)));
	REQUIRE(Observation.Stops == 1 && Observation.Destructs == 1);
	REQUIRE(Backend.GetTrace().Events.Back() == "shutdown");
}
TEST("Application retains current scene after a later failed transition")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	FAppObservation First, Failed;
	Failed.bFailInitialize = true;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	REQUIRE(App.Start(Toolbox::MakeUnique<DObservedAppScene>(First)));
	REQUIRE(App.GetScenes().RequestChange<DObservedAppScene>(Failed));
	// 一フレーム処理の成否と継続状態。
	auto Step = App.Step(0);
	REQUIRE(Step && Step.Value());
	REQUIRE(First.Ticks == 1 && First.Stops == 0 && Failed.Stops == 1);
	REQUIRE(App.GetScenes().GetLastTransitionError().HasValue());
}
TEST("Application defers shutdown until the user callback has returned")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// コールバックから書き込む観測結果。
	FAppObservation Observation;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	Observation.Tick = [&](const FTickContext&)
	{
		App.Shutdown();
		REQUIRE(Observation.Destructs == 0);
	};
	REQUIRE(App.Start(Toolbox::MakeUnique<DObservedAppScene>(Observation)));
	// 一フレーム処理の成否と継続状態。
	auto Step = App.Step(0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(Observation.Destructs == 1 && !Observation.bDestroyedInsideCallback);
	REQUIRE(Backend.GetTrace().Presentations == 0);
}
TEST("Application rejects recursive Step without ending the outer frame")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// コールバックから書き込む観測結果。
	FAppObservation Observation;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	Observation.Tick = [&](const FTickContext&)
	{
		REQUIRE(!App.Step(0.01));
	};
	REQUIRE(App.Start(Toolbox::MakeUnique<DObservedAppScene>(Observation)));
	// 一フレーム処理の成否と継続状態。
	auto Step = App.Step(0);
	REQUIRE(Step && Step.Value());
	REQUIRE(Backend.GetTrace().Presentations == 1);
}
TEST("Application converts user tick exceptions and releases all services")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// コールバックから書き込む観測結果。
	FAppObservation Observation;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	Observation.Tick = [](const FTickContext&)
	{
		throw Toolbox::FException("user tick");
	};
	REQUIRE(App.Start(Toolbox::MakeUnique<DObservedAppScene>(Observation)));
	REQUIRE(!App.Step(0));
	REQUIRE(Observation.Stops == 1);
	REQUIRE(Backend.GetTrace().Events.Back() == "shutdown");
}
TEST("Application stops cleanly when event pumping reports quit")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	REQUIRE(App.Start(Toolbox::MakeUnique<DScene>()));
	Backend.GetTrace().bQuit = true;
	// 一フレーム処理の成否と継続状態。
	auto Step = App.Step(0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(Backend.GetTrace().Presentations == 0);
}
TEST("Application rejects backwards frame time before executing another frame")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	REQUIRE(App.Start(Toolbox::MakeUnique<DScene>()));
	REQUIRE(App.Step(10));
	REQUIRE(!App.Step(9));
	REQUIRE(Backend.GetTrace().Presentations == 1);
}
TEST("Application invalidates surviving asset references before ending DxLib")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	REQUIRE(App.Start(Toolbox::MakeUnique<DScene>()));
	// 検証で使用する画像資源。
	auto Texture = App.GetAssets().LoadTexture("player.bmp").Value();
	App.Shutdown();
	REQUIRE(!Texture.IsValid());
	// 実行した操作の順序。
	const auto& Events = Backend.GetTrace().Events;
	REQUIRE(Toolbox::Find(Events.Begin(), Events.End(), "delete-texture") <
	        Toolbox::Find(Events.Begin(), Events.End(), "shutdown"));
}
TEST("AppRunner executes until a scene requests quit")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// コールバックから書き込む観測結果。
	FAppObservation Observation;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	Observation.Tick = [](const FTickContext& Context)
	{
		Context.Scenes->RequestQuit();
	};
	// フレーム進行を実行するランナー。
	FAppRunner Runner;
	// 検証対象の操作が返した成否と値。
	auto Result = Runner.Run(App, Toolbox::MakeUnique<DObservedAppScene>(Observation));
	REQUIRE(Result);
	REQUIRE(Observation.Ticks == 1 && Observation.Destructs == 1);
}
TEST("Application shutdown requested during draw waits for the draw callback and cancels presentation")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// コールバックから書き込む観測結果。
	FAppObservation Observation;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	Observation.Draw = [&](FRenderContext&)
	{
		Observation.bInsideCallback = true;
		App.Shutdown();
		REQUIRE(Observation.Destructs == 0);
		Observation.bInsideCallback = false;
	};
	REQUIRE(App.Start(Toolbox::MakeUnique<DObservedAppScene>(Observation)));
	// 一フレーム処理の成否と継続状態。
	auto Step = App.Step(0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(!Observation.bDestroyedInsideCallback && Observation.Destructs == 1);
	REQUIRE(Backend.GetTrace().Presentations == 0);
}
TEST("Scene navigator rejects a commit inside a scene draw callback")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// コールバックから書き込む観測結果。
	FAppObservation Observation;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	Observation.Draw = [&](FRenderContext&)
	{
		REQUIRE(!App.GetScenes().Commit());
	};
	REQUIRE(App.Start(Toolbox::MakeUnique<DObservedAppScene>(Observation)));
	REQUIRE(App.Step(0));
	REQUIRE(Backend.GetTrace().Presentations == 1 && Observation.Destructs == 0);
}
TEST("Scene navigator shutdown from tick does not destroy the callback receiver until return")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// コールバックから書き込む観測結果。
	FAppObservation Observation;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend));
	Observation.Tick = [&](const FTickContext& Context)
	{
		Context.Scenes->Shutdown();
		REQUIRE(Observation.Destructs == 0);
	};
	REQUIRE(App.Start(Toolbox::MakeUnique<DObservedAppScene>(Observation)));
	// 一フレーム処理の成否と継続状態。
	auto Step = App.Step(0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(!Observation.bDestroyedInsideCallback && Observation.Destructs == 1);
}
namespace
{
// ゲーム初期化や終了の順序を記録する。
class DOrderedGame final : public DGameInstance
{
public:
	// 検証に必要な依存先と初期状態を設定する。
	explicit DOrderedGame(Toolbox::TVector<Toolbox::FString>& Events) : m_pEvents(&Events)
	{
	}

protected:
	// 終了処理の呼び出しを観測する。
	void OnDeinitialize() noexcept override
	{
		m_pEvents->PushBack("game-stop");
	}

private:
	// 外部のイベント履歴への参照。
	Toolbox::TVector<Toolbox::FString>* m_pEvents;
};
// シーン遷移とフックの順序を記録する。
class DOrderedScene final : public DScene
{
public:
	// 検証に必要な依存先と初期状態を設定する。
	explicit DOrderedScene(Toolbox::TVector<Toolbox::FString>& Events) : m_pEvents(&Events)
	{
	}

protected:
	// 終了処理の呼び出しを観測する。
	void OnDeinitialize() noexcept override
	{
		m_pEvents->PushBack("scene-stop");
	}

private:
	// 外部のイベント履歴への参照。
	Toolbox::TVector<Toolbox::FString>* m_pEvents;
};
} // namespace
TEST("Application stops scene before game instance and platform last")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 実行した操作の順序。
	auto& Events = Backend.GetTrace().Events;
	// 検証するアプリケーション。
	FApplication App(MakeServices_Internal(Backend), {}, Toolbox::MakeUnique<DOrderedGame>(Events));
	REQUIRE(App.Start(Toolbox::MakeUnique<DOrderedScene>(Events)));
	App.Shutdown();
	// 検証対象のシーン。
	const auto Scene = Toolbox::Find(Events.Begin(), Events.End(), "scene-stop");
	// 検証対象のゲーム。
	const auto Game = Toolbox::Find(Events.Begin(), Events.End(), "game-stop");
	// 起動とイベント処理を提供するプラットフォーム。
	const auto Platform = Toolbox::Find(Events.Begin(), Events.End(), "shutdown");
	REQUIRE(Scene < Game && Game < Platform);
}

TEST("Model smoke distinguishes continuation stop and injected error")
{
	// 三種類のStep結果を同じ検証関数へ渡す。
	const auto Continue = TResult<bool>::Success(true);
	const auto Stop = TResult<bool>::Success(false);
	const auto Fault = TResult<bool>::Failure(EErrorCode::UserException, "Model instance was released before drawing");
	const auto Other = TResult<bool>::Failure(EErrorCode::BackendFailure, "other first error");
	REQUIRE(ModelSmokeStepMatches(Continue, false, false));
	REQUIRE(!ModelSmokeStepMatches(Stop, false, false));
	REQUIRE(!ModelSmokeStepMatches(Fault, false, true));
	REQUIRE(ModelSmokeStepMatches(Fault, true, true));
	REQUIRE(!ModelSmokeStepMatches(Fault, true, false));
	REQUIRE(!ModelSmokeStepMatches(Stop, true, false));
	REQUIRE(!ModelSmokeStepMatches(Continue, true, true));
	REQUIRE(!ModelSmokeStepMatches(Other, true, true));
}
TEST("Model smoke rejects platform stop before injection and ends scenario before scene request")
{
	// 実Applicationを動かし、Platform境界だけを記録用に置き換える。
	FFakeBackend Backend;
	FAppObservation Observation;
	FApplication App(MakeServices_Internal(Backend));
	// この地点へ届く前にイベント処理が終了を返す。
	bool bInjected = false;
	Observation.Draw = [&](FRenderContext&)
	{
		bInjected = true;
	};
	REQUIRE(App.Start(Toolbox::MakeUnique<DObservedAppScene>(Observation)));
	Backend.GetTrace().bQuit = true;
	const auto Result = App.Step(0);
	REQUIRE(Result && !Result.Value());
	REQUIRE(!bInjected && Observation.Draws == 0 && Observation.Stops == 1);
	REQUIRE(Backend.GetTrace().Presentations == 0 && !App.IsRunning());
	// 実描画試験と同じ前提ゲート。失敗時はScene要求を実行しない。
	bool bRequested = false;
	if (ModelSmokeStepMatches(Result, true, bInjected))
	{
		bRequested = true;
		App.GetScenes().RequestChange<DScene>();
	}
	REQUIRE(!bRequested);
	REQUIRE(Result && !Result.Value());
	REQUIRE(Backend.GetTrace().Events.Back() == "shutdown");
}
