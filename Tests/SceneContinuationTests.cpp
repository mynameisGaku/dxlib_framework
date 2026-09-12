#include "Toolbox/UniquePtr.h"
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/GameObject.h"
#include "Dxf/GameScene.h"
#include "Dxf/RenderSystem2D.h"
#include "Dxf/SceneNavigator.h"
#include "Toolbox/Function.h"
#include "Toolbox/Utility.h"

using namespace Dxf;
using namespace Dxf::Testing;

namespace
{
/**
 * シーンの生成から破棄までの観測回数。
 */
struct FSceneCounts
{
	/**
	 * 対象を構築した回数。
	 */
	Toolbox::int32 Construct = 0;
	/**
	 * 初期化フックを呼んだ回数。
	 */
	Toolbox::int32 Initialize = 0;
	/**
	 * シーン開始フックを呼んだ回数。
	 */
	Toolbox::int32 Enter = 0;
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
	Toolbox::int32 Stop = 0;
	/**
	 * 対象を破棄した回数。
	 */
	Toolbox::int32 Destroy = 0;
};

/**
 * シーンの処理回数を外部カウンターへ記録する。
 */
class DSceneProbe final : public DGameScene
{
public:
	/**
	 * 検証に必要な依存先と初期状態を設定する。
	 */
	explicit DSceneProbe(FSceneCounts& Counts, Toolbox::TFunction<void()> Destruct = {},
	                     Toolbox::TFunction<void()> Tick = {}, Toolbox::TFunction<void()> Draw = {})
	    : m_pCounts(&Counts), m_Destruct(Toolbox::Move(Destruct)), m_Tick(Toolbox::Move(Tick)),
	      m_Draw(Toolbox::Move(Draw))
	{
		++m_pCounts->Construct;
	}
	/**
	 * 所有データを解放し、必要な破棄の観測を行う。
	 */
	~DSceneProbe() override
	{
		++m_pCounts->Destroy;
		if (m_Destruct)
		{
			m_Destruct();
		}
	}

protected:
	/**
	 * 初期化の呼び出しを観測し、指定した検証条件を適用する。
	 */
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounts->Initialize;
		return {};
	}
	/**
	 * 遷移先へ入ったタイミングを観測する。
	 */
	void OnEnter(const FSceneActivationContext&) noexcept override
	{
		++m_pCounts->Enter;
	}
	/**
	 * 更新の呼び出しを観測し、指定された処理を実行する。
	 */
	void OnTick(const FTickContext&) override
	{
		++m_pCounts->Tick;
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
		++m_pCounts->Draw;
		if (m_Draw)
		{
			m_Draw();
		}
	}
	/**
	 * 終了処理の呼び出しを観測する。
	 */
	void OnDeinitialize() noexcept override
	{
		++m_pCounts->Stop;
	}

private:
	/**
	 * 外部のライフサイクル集計への参照。
	 */
	FSceneCounts* m_pCounts;
	/**
	 * デストラクター中の再入操作を再現するコールバック。
	 */
	Toolbox::TFunction<void()> m_Destruct;
	/**
	 * 更新中の再入操作を再現するコールバック。
	 */
	Toolbox::TFunction<void()> m_Tick;
	/**
	 * 描画中の再入操作を再現するコールバック。
	 */
	Toolbox::TFunction<void()> m_Draw;
};

/**
 * アクターの初期化・更新・破棄を外部から観測する。
 */
class DActorProbe final : public DGameObject
{
public:
	/**
	 * 検証に必要な依存先と初期状態を設定する。
	 */
	explicit DActorProbe(FSceneCounts& Counts, Toolbox::TFunction<void()> Tick = {},
	                     Toolbox::TFunction<void()> Init = {})
	    : m_pCounts(&Counts), m_Tick(Toolbox::Move(Tick)), m_Init(Toolbox::Move(Init))
	{
	}

protected:
	/**
	 * 初期化の呼び出しを観測し、指定した検証条件を適用する。
	 */
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounts->Initialize;
		if (m_Init)
		{
			m_Init();
		}
		return {};
	}
	/**
	 * 更新の呼び出しを観測し、指定された処理を実行する。
	 */
	void OnTick(const FTickContext&) override
	{
		++m_pCounts->Tick;
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
		++m_pCounts->Draw;
	}
	/**
	 * 終了処理の呼び出しを観測する。
	 */
	void OnDeinitialize() noexcept override
	{
		++m_pCounts->Stop;
	}

private:
	/**
	 * 外部のライフサイクル集計への参照。
	 */
	FSceneCounts* m_pCounts;
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
 * 指定したフックで例外を発生させる。
 */
class DThrowingScene final : public DScene
{
public:
	/**
	 * 検証に必要な依存先と初期状態を設定する。
	 */
	DThrowingScene()
	{
		throw Toolbox::FException("scene constructor failed");
	}
};
} // namespace

TEST("scene factory exceptions become results without replacing the current scene")
{
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	/**
	 * 現在選択されている対象。
	 */
	FSceneCounts Current;
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Current));
	REQUIRE(Scenes.Commit());
	/**
	 * 検証対象の操作が返した成否と値。
	 */
	auto Result = Scenes.RequestChange<DThrowingScene>();
	REQUIRE(!Result && Result.Error().Code == EErrorCode::UserException);
	REQUIRE(Scenes.GetCurrent() != nullptr && Current.Stop == 0);
}

TEST("stopped scene navigator rejects a factory before invoking its constructor")
{
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	/**
	 * ライフサイクルの観測回数。
	 */
	FSceneCounts Counts;
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	Scenes.Shutdown();
	REQUIRE(!Scenes.RequestChange<DSceneProbe>(Counts));
	REQUIRE(Counts.Construct == 0);
}

TEST("destroy requested pending scene is rejected at request time")
{
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	/**
	 * 検証対象のシーン。
	 */
	auto Scene = Toolbox::MakeUnique<DScene>();
	Scene->RequestDestroy_Internal();
	REQUIRE(!Scenes.RequestChange(Toolbox::Move(Scene)));
}

TEST("pending scene destructor cannot commit a transition reentrantly")
{
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	/**
	 * 削除または置換する前の登録。
	 */
	FSceneCounts Old;
	FSceneCounts Next;
	/**
	 * 操作が想定どおり拒否されたか。
	 */
	bool bRejected = false;
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Old,
	                                          [&]
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
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	/**
	 * 削除または置換する前の登録。
	 */
	FSceneCounts Old;
	FSceneCounts Next;
	FSceneCounts Nested;
	/**
	 * 操作が想定どおり拒否されたか。
	 */
	bool bRejected = false;
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Old,
	                                          [&]
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
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	/**
	 * 削除または置換する前の登録。
	 */
	FSceneCounts Old;
	FSceneCounts Next;
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Old,
	                                          [&]
	                                          {
		                                          Scenes.Shutdown();
	                                          }));
	REQUIRE(!Scenes.RequestChange<DSceneProbe>(Next));
	REQUIRE(Scenes.WantsQuit());
	REQUIRE(Scenes.GetCurrent() == nullptr);
	REQUIRE(Next.Initialize == 0 && Next.Destroy == 1);
}

TEST("quit before commit never initializes the queued scene")
{
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	FSceneCounts Next;
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DSceneProbe>(Next));
	Scenes.RequestQuit();
	REQUIRE(Scenes.Commit());
	REQUIRE(Next.Initialize == 0);
}

TEST("quit from scene update suppresses its child object updates")
{
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	/**
	 * シーンのライフサイクル観測値。
	 */
	FSceneCounts SceneCounts;
	/**
	 * アクターのライフサイクル観測値。
	 */
	FSceneCounts ActorCounts;
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	/**
	 * 検証対象のシーン。
	 */
	auto Scene = Toolbox::MakeUnique<DSceneProbe>(SceneCounts, Toolbox::TFunction<void()>{},
	                                              [&]
	                                              {
		                                              Scenes.RequestQuit();
	                                              });
	REQUIRE(Scene->Spawn<DActorProbe>(ActorCounts));
	REQUIRE(Scenes.RequestChange(Toolbox::Move(Scene)));
	REQUIRE(Scenes.Commit());
	REQUIRE(Scenes.Tick({}, {}));
	REQUIRE(SceneCounts.Tick == 1 && ActorCounts.Tick == 0);
}

TEST("quit from one object update suppresses later siblings in the same scene")
{
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	/**
	 * シーンのライフサイクル観測値。
	 */
	FSceneCounts SceneCounts;
	/**
	 * 最初に生成または登録した対象。
	 */
	FSceneCounts First;
	/**
	 * 二番目に生成または登録した対象。
	 */
	FSceneCounts Second;
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	/**
	 * 検証対象のシーン。
	 */
	auto Scene = Toolbox::MakeUnique<DSceneProbe>(SceneCounts);
	REQUIRE(Scene->Spawn<DActorProbe>(First,
	                                  [&]
	                                  {
		                                  Scenes.RequestQuit();
	                                  }));
	REQUIRE(Scene->Spawn<DActorProbe>(Second));
	REQUIRE(Scenes.RequestChange(Toolbox::Move(Scene)));
	REQUIRE(Scenes.Commit());
	REQUIRE(Scenes.Tick({}, {}));
	REQUIRE(First.Tick == 1 && Second.Tick == 0);
}

TEST("shutdown from scene drawing suppresses child drawing and deinitializes once")
{
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	/**
	 * フックへ渡す描画環境。
	 */
	FRenderSystem2D Render(Backend);
	/**
	 * シーンのライフサイクル観測値。
	 */
	FSceneCounts SceneCounts;
	/**
	 * アクターのライフサイクル観測値。
	 */
	FSceneCounts ActorCounts;
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	/**
	 * 検証対象のシーン。
	 */
	auto Scene =
	    Toolbox::MakeUnique<DSceneProbe>(SceneCounts, Toolbox::TFunction<void()>{}, Toolbox::TFunction<void()>{},
	                                     [&]
	                                     {
		                                     Scenes.Shutdown();
	                                     });
	REQUIRE(Scene->Spawn<DActorProbe>(ActorCounts));
	REQUIRE(Scenes.RequestChange(Toolbox::Move(Scene)));
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
	/**
	 * 検証用のバックエンド。
	 */
	FFakeBackend Backend;
	/**
	 * 検証に使用する資源管理。
	 */
	FAssetService Assets(Backend, Backend, Backend);
	/**
	 * 再生状態を管理する音声サービス。
	 */
	FAudioPlayer Audio(Backend);
	/**
	 * シーンのライフサイクル観測値。
	 */
	FSceneCounts SceneCounts;
	/**
	 * 最初に生成または登録した対象。
	 */
	FSceneCounts First;
	/**
	 * 二番目に生成または登録した対象。
	 */
	FSceneCounts Second;
	/**
	 * 遷移と生存期間を管理するシーン一覧。
	 */
	FSceneNavigator Scenes(Assets, Audio);
	/**
	 * 検証対象のシーン。
	 */
	auto Scene = Toolbox::MakeUnique<DSceneProbe>(SceneCounts);
	REQUIRE(Scene->Spawn<DActorProbe>(First, Toolbox::TFunction<void()>{},
	                                  [&]
	                                  {
		                                  Scenes.RequestQuit();
	                                  }));
	REQUIRE(Scene->Spawn<DActorProbe>(Second));
	REQUIRE(Scenes.RequestChange(Toolbox::Move(Scene)));
	/**
	 * シーン遷移の結果。
	 */
	const auto Transition = Scenes.Commit();
	REQUIRE(!Transition || !Transition.Value());
	REQUIRE(First.Initialize == 1 && Second.Initialize == 0);
	REQUIRE(SceneCounts.Enter == 0);
	REQUIRE(First.Stop == 1 && Second.Stop == 0);
}
