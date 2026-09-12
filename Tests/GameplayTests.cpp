#include "Toolbox/UniquePtr.h"
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
#include "Toolbox/Function.h"
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
/**
 * 対象のライフサイクルを観測する集計値。
 */
struct FCounters
{
	/**
	 * 初期化フックを呼んだ回数。
	 */
	Toolbox::int32 Initialize = 0;
	/**
	 * 更新フックを呼んだ回数。
	 */
	Toolbox::int32 Tick = 0;
	/**
	 * 描画フックを呼んだ回数。
	 */
	Toolbox::int32 Draw = 0;
	/**
	 * 終了処理フックを呼んだ回数。
	 */
	Toolbox::int32 Deinitialize = 0;
	/**
	 * デストラクターが呼ばれた回数。
	 */
	Toolbox::int32 Destruct = 0;
	/**
	 * シーン開始フックを呼んだ回数。
	 */
	Toolbox::int32 Enter = 0;
	/**
	 * シーン終了フックを呼んだ回数。
	 */
	Toolbox::int32 Exit = 0;
	/**
	 * 初期化失敗を発生させるか。
	 */
	bool bFailInitialize = false;
	/**
	 * 実際に記録された呼び出し順序。
	 */
	Toolbox::TVector<Toolbox::FString> Order;
};
/**
 * コンポーネントの生存数とライフサイクルを数える。
 */
class DCountingComponent final : public DGameObjectComponent
{
public:
	/**
	 * 検証に必要な依存先と初期状態を設定する。
	 */
	explicit DCountingComponent(FCounters& Counters, Toolbox::TFunction<void()> Tick = {},
	                            Toolbox::TFunction<void()> Init = {})
	    : m_pCounters(&Counters), m_Tick(Toolbox::Move(Tick)), m_Init(Toolbox::Move(Init))
	{
	}
	/**
	 * 所有データを解放し、必要な破棄の観測を行う。
	 */
	~DCountingComponent() override
	{
		++m_pCounters->Destruct;
	}

protected:
	/**
	 * 初期化の呼び出しを観測し、指定した検証条件を適用する。
	 */
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounters->Initialize;
		if (m_Init)
		{
			m_Init();
		}
		return m_pCounters->bFailInitialize ? TResult<void>::Failure(EErrorCode::InitializationFailed, "component init")
		                                    : TResult<void>{};
	}
	/**
	 * 更新の呼び出しを観測し、指定された処理を実行する。
	 */
	void OnTick(const FTickContext&) override
	{
		++m_pCounters->Tick;
		if (m_Tick)
		{
			m_Tick();
		}
	}
	/**
	 * 描画の呼び出しを観測し、指定された処理を実行する。
	 */
	void OnDraw(FRenderContext&) const override
	{
		++m_pCounters->Draw;
	}
	/**
	 * 終了処理の呼び出しを観測する。
	 */
	void OnDeinitialize() noexcept override
	{
		++m_pCounters->Deinitialize;
		m_pCounters->Order.PushBack("component-stop");
	}

private:
	/**
	 * 外部のライフサイクル集計への参照。
	 */
	FCounters* m_pCounters;
	/**
	 * 更新中の再入操作を再現するコールバック。
	 */
	Toolbox::TFunction<void()> m_Tick;
	/**
	 * 初期化中の再入操作を再現するコールバック。
	 */
	Toolbox::TFunction<void()> m_Init;
};
/**
 * オブジェクトの各フックの呼び出しを数える。
 */
class DCountingObject : public DGameObject
{
public:
	/**
	 * 検証に必要な依存先と初期状態を設定する。
	 */
	explicit DCountingObject(FCounters& Counters, Toolbox::TFunction<void()> Tick = {},
	                         Toolbox::TFunction<void()> Init = {}, Toolbox::TFunction<void()> Stop = {})
	    : m_pCounters(&Counters), m_Tick(Toolbox::Move(Tick)), m_Init(Toolbox::Move(Init)), m_Stop(Toolbox::Move(Stop))
	{
	}
	/**
	 * 所有データを解放し、必要な破棄の観測を行う。
	 */
	~DCountingObject() override
	{
		++m_pCounters->Destruct;
	}

protected:
	/**
	 * 初期化の呼び出しを観測し、指定した検証条件を適用する。
	 */
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounters->Initialize;
		if (m_Init)
		{
			m_Init();
		}
		return m_pCounters->bFailInitialize ? TResult<void>::Failure(EErrorCode::InitializationFailed, "object init")
		                                    : TResult<void>{};
	}
	/**
	 * 更新の呼び出しを観測し、指定された処理を実行する。
	 */
	void OnTick(const FTickContext&) override
	{
		++m_pCounters->Tick;
		if (m_Tick)
		{
			m_Tick();
		}
	}
	/**
	 * 描画の呼び出しを観測し、指定された処理を実行する。
	 */
	void OnDraw(FRenderContext&) const override
	{
		++m_pCounters->Draw;
	}
	/**
	 * 終了処理の呼び出しを観測する。
	 */
	void OnDeinitialize() noexcept override
	{
		++m_pCounters->Deinitialize;
		m_pCounters->Order.PushBack("object-stop");
		if (m_Stop)
		{
			m_Stop();
		}
	}

private:
	/**
	 * 外部のライフサイクル集計への参照。
	 */
	FCounters* m_pCounters;
	/**
	 * 更新中の再入操作を再現するコールバック。
	 */
	Toolbox::TFunction<void()> m_Tick;
	/**
	 * 初期化中の再入操作を再現するコールバック。
	 */
	Toolbox::TFunction<void()> m_Init;
	/**
	 * 終了処理中の再入操作を再現するコールバック。
	 */
	Toolbox::TFunction<void()> m_Stop;
};
/**
 * シーンの各フックの呼び出しを数える。
 */
class DCountingScene : public DGameScene
{
public:
	/**
	 * 検証に必要な依存先と初期状態を設定する。
	 */
	explicit DCountingScene(FCounters& Counters, Toolbox::TFunction<void()> Enter = {})
	    : m_pCounters(&Counters), m_Enter(Toolbox::Move(Enter))
	{
	}
	/**
	 * 所有データを解放し、必要な破棄の観測を行う。
	 */
	~DCountingScene() override
	{
		++m_pCounters->Destruct;
	}

protected:
	/**
	 * 初期化の呼び出しを観測し、指定した検証条件を適用する。
	 */
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounters->Initialize;
		return m_pCounters->bFailInitialize ? TResult<void>::Failure(EErrorCode::InitializationFailed, "scene init")
		                                    : TResult<void>{};
	}
	/**
	 * 更新の呼び出しを観測し、指定された処理を実行する。
	 */
	void OnTick(const FTickContext&) override
	{
		++m_pCounters->Tick;
	}
	/**
	 * 描画の呼び出しを観測し、指定された処理を実行する。
	 */
	void OnDraw(FRenderContext&) const override
	{
		++m_pCounters->Draw;
	}
	/**
	 * 終了処理の呼び出しを観測する。
	 */
	void OnDeinitialize() noexcept override
	{
		++m_pCounters->Deinitialize;
	}
	/**
	 * 遷移先へ入ったタイミングを観測する。
	 */
	void OnEnter(const FSceneActivationContext&) noexcept override
	{
		++m_pCounters->Enter;
		if (m_Enter)
		{
			m_Enter();
		}
	}
	/**
	 * 遷移元を出るタイミングを観測する。
	 */
	void OnExit() noexcept override
	{
		++m_pCounters->Exit;
	}

private:
	/**
	 * 外部のライフサイクル集計への参照。
	 */
	FCounters* m_pCounters;
	/**
	 * シーン開始中の再入操作を再現するコールバック。
	 */
	Toolbox::TFunction<void()> m_Enter;
};
/**
 * ワールドの検証に必要な依存先と状態をまとめる。
 */
class FWorldFixture
{
public:
	/**
	 * 検証に必要な依存先と初期状態を設定する。
	 */
	FWorldFixture()
	    : m_Assets(m_Backend, m_Backend, m_Backend), m_Audio(m_Backend), m_Navigator(m_Assets, m_Audio),
	      m_Renderer(m_Backend)
	{
	}
	/**
	 * 初期化の観測結果を返す。
	 */
	FInitContext GetInit()
	{
		return {m_Assets};
	}
	/**
	 * 更新の観測結果を返す。
	 */
	FTickContext GetTick()
	{
		/**
		 * サンプリングしたフレーム時刻。
		 */
		FFrameTime Time;
		Time.DeltaSeconds = 0.016;
		Time.UnscaledDeltaSeconds = 0.016;
		return {m_Input, Time};
	}
	/**
	 * 検証用の資源管理への参照を返す。
	 */
	FAssetService& GetAssets()
	{
		return m_Assets;
	}
	/**
	 * 検証用のシーン管理への参照を返す。
	 */
	FSceneNavigator& GetScenes()
	{
		return m_Navigator;
	}
	/**
	 * 検証用の描画機能への参照を返す。
	 */
	FRenderSystem2D& GetRenderer()
	{
		return m_Renderer;
	}
	const FInputSnapshot& GetInput() const
	{
		return m_Input;
	}

private:
	/**
	 * 検証対象が参照するバックエンド。
	 */
	FFakeBackend m_Backend;
	/**
	 * 検証用の資源管理。
	 */
	FAssetService m_Assets;
	/**
	 * 検証用の音声サービス。
	 */
	FAudioPlayer m_Audio;
	/**
	 * シーン遷移の操作先。
	 */
	FSceneNavigator m_Navigator;
	/**
	 * 検証用の描画サービス。
	 */
	FRenderSystem2D m_Renderer;
	/**
	 * 検証用の入力サービス。
	 */
	FInputSnapshot m_Input;
};
} // namespace
TEST("Spawn remains pending until boundary and initializes exactly once")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * ライフサイクルの観測回数。
	 */
	FCounters Counts;
	FGameObjectCollection Objects;
	/**
	 * 検証対象のオブジェクト。
	 */
	auto Object = Objects.Spawn<DCountingObject>(Counts).Value();
	REQUIRE(Objects.Tick_Internal(World.GetTick()));
	REQUIRE(Counts.Tick == 0);
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Objects.Tick_Internal(World.GetTick()));
	REQUIRE(Counts.Initialize == 1 && Counts.Tick == 1);
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Counts.Initialize == 1);
	REQUIRE(Object.Get());
}
TEST("GameObject override cannot bypass automatic component dispatch")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * 子の所有権を持つ対象。
	 */
	FCounters Parent;
	/**
	 * 親に属する対象。
	 */
	FCounters Child;
	FGameObjectCollection Objects;
	/**
	 * 検証対象のオブジェクト。
	 */
	auto Object = Objects.Spawn<DCountingObject>(Parent).Value();
	/**
	 * 検証対象のコンポーネント。
	 */
	auto Component = Object.Get()->AddComponent<DCountingComponent>(Child).Value();
	REQUIRE(Component.Get()->GetOwner() == Object.Get());
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Objects.Tick_Internal(World.GetTick()));
	REQUIRE(World.GetRenderer().BeginFrame(640, 480));
	REQUIRE(Objects.Draw_Internal(World.GetRenderer().GetContext()));
	REQUIRE(World.GetRenderer().EndFrame());
	REQUIRE(Parent.Tick == 1 && Child.Tick == 1);
	REQUIRE(Parent.Draw == 1 && Child.Draw == 1);
}
TEST("Spawn during tick is initialized only at the next boundary")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	FCounters A;
	FCounters B;
	FGameObjectCollection Objects;
	/**
	 * コールバック中に新しい対象を生成したか。
	 */
	bool bSpawned = false;
	REQUIRE(Objects.Spawn<DCountingObject>(A,
	                                       [&]
	                                       {
		                                       if (!bSpawned)
		                                       {
			                                       bSpawned = true;
			                                       REQUIRE(Objects.Spawn<DCountingObject>(B));
		                                       }
	                                       }));
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Objects.Tick_Internal(World.GetTick()));
	REQUIRE(B.Initialize == 0 && B.Tick == 0);
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Objects.Tick_Internal(World.GetTick()));
	REQUIRE(B.Initialize == 1 && B.Tick == 1);
}
TEST("Destroying a later sibling suppresses its remaining callbacks immediately")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	FCounters A;
	FCounters B;
	FGameObjectCollection Objects;
	/**
	 * コールバック中に削除する対象。
	 */
	TObjectHandle<DCountingObject> Victim;
	REQUIRE(Objects.Spawn<DCountingObject>(A,
	                                       [&]
	                                       {
		                                       Objects.Destroy(Victim);
	                                       }));
	Victim = Objects.Spawn<DCountingObject>(B).Value();
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Objects.Tick_Internal(World.GetTick()));
	REQUIRE(!Victim.Get());
	REQUIRE(B.Tick == 0);
	REQUIRE(B.Destruct == 0);
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(B.Deinitialize == 1 && B.Destruct == 1);
}
TEST("Self destruction in tick skips owned components and later draw")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	FCounters A;
	FCounters B;
	FGameObjectCollection Objects;
	/**
	 * コールバックが自分を操作するためのハンドル。
	 */
	TObjectHandle<DCountingObject> Self;
	Self = Objects
	           .Spawn<DCountingObject>(A,
	                                   [&]
	                                   {
		                                   Self.Get()->Destroy();
	                                   })
	           .Value();
	REQUIRE(Self.Get()->AddComponent<DCountingComponent>(B));
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Objects.Tick_Internal(World.GetTick()));
	REQUIRE(B.Tick == 0);
	REQUIRE(World.GetRenderer().BeginFrame(640, 480));
	REQUIRE(Objects.Draw_Internal(World.GetRenderer().GetContext()));
	REQUIRE(A.Draw == 0 && B.Draw == 0);
}
TEST("Destroy before initialization never calls user initialization or shutdown hooks")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * ライフサイクルの観測回数。
	 */
	FCounters Counts;
	FGameObjectCollection Objects;
	/**
	 * 生存期間や世代を検証する登録ハンドル。
	 */
	auto Handle = Objects.Spawn<DCountingObject>(Counts).Value();
	REQUIRE(Objects.Destroy(Handle));
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Counts.Initialize == 0 && Counts.Deinitialize == 0 && Counts.Destruct == 1);
}
TEST("Initialization failure rolls back the object and invalidates the handle")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * ライフサイクルの観測回数。
	 */
	FCounters Counts;
	Counts.bFailInitialize = true;
	FGameObjectCollection Objects;
	/**
	 * 生存期間や世代を検証する登録ハンドル。
	 */
	auto Handle = Objects.Spawn<DCountingObject>(Counts).Value();
	Objects.FreezeBoundary_Internal();
	REQUIRE(!Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(!Handle.Get());
	REQUIRE(Counts.Deinitialize == 1 && Counts.Destruct == 1);
}
TEST("Component initialization failure rolls back its parent transaction")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * 子の所有権を持つ対象。
	 */
	FCounters Parent;
	/**
	 * 親に属する対象。
	 */
	FCounters Child;
	Child.bFailInitialize = true;
	FGameObjectCollection Objects;
	/**
	 * 生存期間や世代を検証する登録ハンドル。
	 */
	auto Handle = Objects.Spawn<DCountingObject>(Parent).Value();
	REQUIRE(Handle.Get()->AddComponent<DCountingComponent>(Child));
	Objects.FreezeBoundary_Internal();
	REQUIRE(!Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Parent.Deinitialize == 1 && Child.Deinitialize == 1);
	REQUIRE(!Handle.Get());
}
TEST("Shutdown is idempotent and stops components before their owner")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * ライフサイクルの観測回数。
	 */
	FCounters Counts;
	FGameObjectCollection Objects;
	/**
	 * 生存期間や世代を検証する登録ハンドル。
	 */
	auto Handle = Objects.Spawn<DCountingObject>(Counts).Value();
	REQUIRE(Handle.Get()->AddComponent<DCountingComponent>(Counts));
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	Objects.Shutdown_Internal();
	Objects.Shutdown_Internal();
	REQUIRE(Counts.Order == Toolbox::TVector<Toolbox::FString>({"component-stop", "object-stop"}));
	REQUIRE(Counts.Deinitialize == 2);
	REQUIRE(!Objects.Spawn<DCountingObject>(Counts));
}
TEST("Mutations submitted from shutdown wait for the next boundary")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	FCounters A;
	FCounters B;
	FGameObjectCollection Objects;
	/**
	 * 生存期間や世代を検証する登録ハンドル。
	 */
	auto Handle = Objects
	                  .Spawn<DCountingObject>(A, Toolbox::TFunction<void()>{}, Toolbox::TFunction<void()>{},
	                                          [&]
	                                          {
		                                          (void)Objects.Spawn<DCountingObject>(B);
	                                          })
	                  .Value();
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	Objects.Destroy(Handle);
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(B.Initialize == 0);
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(B.Initialize == 1);
}
TEST("Children added to an existing object during another initializer wait for a new boundary")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	FCounters A;
	FCounters B;
	/**
	 * 親に属する対象。
	 */
	FCounters Child;
	FGameObjectCollection Objects;
	auto Existing = Objects.Spawn<DCountingObject>(A).Value();
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Objects.Spawn<DCountingObject>(B, Toolbox::TFunction<void()>{},
	                                       [&]
	                                       {
		                                       REQUIRE(Existing.Get()->AddComponent<DCountingComponent>(Child));
	                                       }));
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Child.Initialize == 0);
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Child.Initialize == 1);
}
TEST("Update ordering is deterministic after slot reuse")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * ライフサイクルの観測回数。
	 */
	FCounters Counts;
	FGameObjectCollection Objects;
	/**
	 * 実際に記録された呼び出し順序。
	 */
	Toolbox::TVector<Toolbox::int32> Order;
	/**
	 * 削除または置換する前の登録。
	 */
	auto Old = Objects.Spawn<DCountingObject>(Counts).Value();
	REQUIRE(Objects.Spawn<DCountingObject>(Counts,
	                                       [&]
	                                       {
		                                       Order.PushBack(2);
	                                       }));
	Objects.Destroy(Old);
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Objects.Spawn<DCountingObject>(Counts,
	                                       [&]
	                                       {
		                                       Order.PushBack(3);
	                                       }));
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	REQUIRE(Objects.Tick_Internal(World.GetTick()));
	REQUIRE(Order == Toolbox::TVector<Toolbox::int32>({2, 3}));
}
TEST("Pause skips normal objects but honors tick-when-paused components")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * 子の所有権を持つ対象。
	 */
	FCounters Parent;
	/**
	 * 親に属する対象。
	 */
	FCounters Child;
	FGameObjectCollection Objects;
	/**
	 * 生存期間や世代を検証する登録ハンドル。
	 */
	auto Handle = Objects.Spawn<DCountingObject>(Parent).Value();
	/**
	 * 検証対象のコンポーネント。
	 */
	auto Component = Handle.Get()->AddComponent<DCountingComponent>(Child).Value();
	Component.Get()->SetTickWhenPaused(true);
	Objects.FreezeBoundary_Internal();
	REQUIRE(Objects.CommitBoundary_Internal(World.GetInit()));
	/**
	 * フック呼び出しへ渡す実行環境。
	 */
	auto Context = World.GetTick();
	Context.Time.bPaused = true;
	Context.Time.DeltaSeconds = 0.0;
	REQUIRE(Objects.Tick_Internal(Context));
	REQUIRE(Parent.Tick == 0 && Child.Tick == 1);
}
TEST("Scene override cannot bypass object dispatch")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * シーンのライフサイクル観測値。
	 */
	FCounters SceneCounts;
	/**
	 * オブジェクトのライフサイクル観測値。
	 */
	FCounters ObjectCounts;
	/**
	 * 検証対象のシーン。
	 */
	auto Scene = Toolbox::MakeUnique<DCountingScene>(SceneCounts);
	REQUIRE(Scene->Spawn<DCountingObject>(ObjectCounts));
	REQUIRE(World.GetScenes().RequestChange(Toolbox::Move(Scene)));
	REQUIRE(World.GetScenes().Commit());
	REQUIRE(World.GetScenes().Tick(World.GetTick().Time, World.GetInput()));
	REQUIRE(SceneCounts.Tick == 1 && ObjectCounts.Tick == 1);
	World.GetScenes().Shutdown();
}
TEST("Scene switch preserves the active scene until commit")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	FCounters A;
	FCounters B;
	REQUIRE(World.GetScenes().RequestChange(Toolbox::MakeUnique<DCountingScene>(A)));
	REQUIRE(World.GetScenes().Commit());
	/**
	 * 削除または置換する前の登録。
	 */
	auto* Old = World.GetScenes().GetCurrent();
	REQUIRE(World.GetScenes().RequestChange(Toolbox::MakeUnique<DCountingScene>(B)));
	REQUIRE(World.GetScenes().GetCurrent() == Old);
	REQUIRE(B.Initialize == 0);
	REQUIRE(World.GetScenes().Commit());
	REQUIRE(A.Exit == 1 && A.Deinitialize == 1 && A.Destruct == 1);
	REQUIRE(B.Enter == 1);
	World.GetScenes().Shutdown();
}
TEST("Failed scene preparation leaves current scene active")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	FCounters A;
	FCounters B;
	B.bFailInitialize = true;
	REQUIRE(World.GetScenes().RequestChange(Toolbox::MakeUnique<DCountingScene>(A)));
	REQUIRE(World.GetScenes().Commit());
	/**
	 * 削除または置換する前の登録。
	 */
	auto* Old = World.GetScenes().GetCurrent();
	REQUIRE(World.GetScenes().RequestChange(Toolbox::MakeUnique<DCountingScene>(B)));
	REQUIRE(!World.GetScenes().Commit());
	REQUIRE(World.GetScenes().GetCurrent() == Old);
	REQUIRE(A.Exit == 0 && B.Enter == 0 && B.Deinitialize == 1 && B.Destruct == 1);
	World.GetScenes().Shutdown();
}
TEST("A request made inside OnEnter survives for the next scene boundary")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	FCounters A;
	FCounters B;
	/**
	 * 検証対象のシーン。
	 */
	auto Scene = Toolbox::MakeUnique<DCountingScene>(A,
	                                                 [&]
	                                                 {
		                                                 (void)World.GetScenes().RequestChange(
		                                                     Toolbox::MakeUnique<DCountingScene>(B));
	                                                 });
	REQUIRE(World.GetScenes().RequestChange(Toolbox::Move(Scene)));
	REQUIRE(World.GetScenes().Commit());
	REQUIRE(B.Initialize == 0);
	REQUIRE(World.GetScenes().Commit());
	REQUIRE(B.Enter == 1);
	World.GetScenes().Shutdown();
}
TEST("Multiple pending scene requests use last-request-wins without initializing discarded scenes")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	FCounters A;
	FCounters B;
	REQUIRE(World.GetScenes().RequestChange(Toolbox::MakeUnique<DCountingScene>(A)));
	REQUIRE(World.GetScenes().RequestChange(Toolbox::MakeUnique<DCountingScene>(B)));
	REQUIRE(A.Destruct == 1 && A.Initialize == 0);
	REQUIRE(World.GetScenes().Commit());
	REQUIRE(B.Enter == 1);
	World.GetScenes().Shutdown();
}
TEST("Handles from a destroyed scene cannot resolve into the next scene")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * シーンのライフサイクル観測値。
	 */
	FCounters SceneCounts;
	/**
	 * ライフサイクルの観測回数。
	 */
	FCounters Counts;
	/**
	 * 最初に生成または登録した対象。
	 */
	auto First = Toolbox::MakeUnique<DCountingScene>(SceneCounts);
	/**
	 * 削除または置換する前の登録。
	 */
	auto Old = First->Spawn<DCountingObject>(Counts).Value();
	REQUIRE(World.GetScenes().RequestChange(Toolbox::Move(First)));
	REQUIRE(World.GetScenes().Commit());
	REQUIRE(Old.Get());
	/**
	 * 二番目に生成または登録した対象。
	 */
	auto Second = Toolbox::MakeUnique<DCountingScene>(SceneCounts);
	/**
	 * 同じ格納先へ再登録した対象。
	 */
	auto New = Second->Spawn<DCountingObject>(Counts).Value();
	REQUIRE(World.GetScenes().RequestChange(Toolbox::Move(Second)));
	REQUIRE(World.GetScenes().Commit());
	REQUIRE(!Old.Get());
	REQUIRE(New.Get());
	World.GetScenes().Shutdown();
}
TEST("GameObject initialization exception is converted into an error and cleaned up")
{
	/**
	 * 検証対象のワールド。
	 */
	FWorldFixture World;
	/**
	 * ライフサイクルの観測回数。
	 */
	FCounters Counts;
	FGameObjectCollection Objects;
	/**
	 * 生存期間や世代を検証する登録ハンドル。
	 */
	auto Handle = Objects
	                  .Spawn<DCountingObject>(Counts, Toolbox::TFunction<void()>{},
	                                          []
	                                          {
		                                          throw Toolbox::FException("init exception");
	                                          })
	                  .Value();
	Objects.FreezeBoundary_Internal();
	/**
	 * 検証対象の操作が返した成否と値。
	 */
	auto Result = Objects.CommitBoundary_Internal(World.GetInit());
	REQUIRE(!Result && Result.Error().Code == EErrorCode::UserException);
	REQUIRE(!Handle.Get());
	REQUIRE(Counts.Deinitialize == 1);
}
