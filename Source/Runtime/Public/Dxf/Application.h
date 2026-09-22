#pragma once
#include "Toolbox/UniquePtr.h"
#include "Dxf/BackendServices.h"
#include "Dxf/DxLibSession.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/RenderSystem.h"
#include "Dxf/GameInstance.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/TaskDispatcher.h"
namespace Dxf
{
/**
 * ウィンドウ・背景色・時間の上限を設定する。
 */
struct FApplicationSettings
{
	/**
	 * ウィンドウの設定。
	 */
	FWindowSettings Window;
	/**
	 * 画面を消去する色。
	 */
	FColor ClearColor{0, 0, 0, 255};
	/**
	 * 一度に進める時間の上限秒数。
	 */
	Toolbox::f64 MaxDeltaSeconds = 0.25;
	/**
	 * 共有実行レーン数。0はOS論理スレッド数、1はWorkerなしの同期実行。
	 */
	Toolbox::uint32 ExecutionThreadCount = 0;
	/**
	 * アセット解決の基準にするProjectRoot。空なら従来の相対パスのまま使う。
	 */
	Toolbox::FString ProjectRoot;
};
/**
 * メインスレッドで一度だけ起動する。コールバック中の終了処理は復帰後へ遅延する。
 */
class FApplication
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Services アプリケーションが利用するサービス。
	 * @param Settings 初期化に使用する設定。
	 * @param Game シーン間で共有するゲーム状態。
	 */
	explicit FApplication(FBackendServices Services, FApplicationSettings Settings = {},
	                      Toolbox::TUniquePtr<DGameInstance> Game = Toolbox::MakeUnique<DGameInstance>());
	/**
	 * 所有する状態を終了し、必要なリソースを解放する。
	 */
	~FApplication();
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FApplication(const FApplication&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FApplication& operator=(const FApplication&) = delete;
	/**
	 * 初期化を行い実行を開始する。
	 * @param InitialScene 最初に開始するシーン。
	 */
	TResult<void> Start(Toolbox::TUniquePtr<DScene> InitialScene);
	/**
	 * falseは正常終了を表す。エラー時は実行を中断し、順序どおり終了処理を行う。
	 * @param NowSeconds 単調増加する現在時刻の秒数。
	 */
	TResult<bool> Step(Toolbox::f64 NowSeconds);
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	void Shutdown() noexcept;
	/**
	 * アプリケーションが実行中かを調べる。
	 */
	FORCEINLINE bool IsRunning() const noexcept
	{
		return m_bStarted && !m_bShutdown && !m_bShutdownRequested;
	}
	/**
	 * シーン遷移の管理窓口を取得する。
	 */
	FORCEINLINE FSceneNavigator& GetScenes() noexcept
	{
		return m_Scenes;
	}
	/**
	 * アセットを読み込むサービスを取得する。
	 */
	FORCEINLINE FAssetService& GetAssets() noexcept
	{
		return m_Assets;
	}
	/**
	 * 描画を統括するサービスを取得する。
	 */
	FORCEINLINE FRenderSystem& GetRenderer() noexcept
	{
		return m_Renderer;
	}
	/**
	 * シーン間で共有するゲーム状態を取得する。
	 */
	FORCEINLINE DGameInstance* GetGameInstance() noexcept
	{
		return m_pGame.Get();
	}
	/**
	 * 共有Job Systemを取得する。所有スレッド以外からも投入できる。
	 */
	FORCEINLINE Toolbox::FJobSystem& GetExecutionJobs() noexcept
	{
		return m_ExecutionJobs;
	}
	/**
	 * 共有Task Dispatcherを取得する。通常の反映はStepの境界で行う。
	 * 手動Pumpを使う場合も、所有スレッドかつScene通知の外で実行する。
	 */
	FORCEINLINE FTaskDispatcher& GetTaskDispatcher() noexcept
	{
		return m_TaskDispatcher;
	}
	/**
	 * 有効化した現在SceneのScopeを返す。OnEnterの前に確定し、終了通知中は失効ハンドルを返す。
	 * Scene破棄完了後は空になる。所有スレッドでのみ取得する。
	 * OnInitialize中は遷移元のScopeであり、遷移先のTask投入には使わない。
	 */
	FORCEINLINE FTaskScope GetSceneScope() noexcept
	{
		return m_Scenes.GetTaskScope();
	}

private:
	/**
	 * 初期化を行い実行を開始する。
	 * @param InitialScene 最初に開始するシーン。
	 */
	TResult<void> Start_Internal(Toolbox::TUniquePtr<DScene> InitialScene);
	/**
	 * 1フレーム分の入力・更新・描画を進める。
	 * @param NowSeconds 単調増加する現在時刻の秒数。
	 */
	TResult<bool> Step_Internal(Toolbox::f64 NowSeconds);
	/**
	 * 正常終了が要求されているかを調べる。
	 */
	bool WantsQuit_Internal() const noexcept;
	/**
	 * OSとウィンドウ機能の呼び出し先。
	 */
	IPlatform* m_pPlatform;
	/**
	 * 初期化に使用する設定。
	 */
	FApplicationSettings m_Settings;
	/**
	 * DxLibの初期化と終了の所有者。
	 */
	FDxLibSession m_Session;
	/**
	 * フレームの入力情報。
	 */
	FInputSystem m_Input;
	/**
	 * アセットを読み込むサービス。
	 */
	FAssetService m_Assets;
	/**
	 * 描画を統括するサービス。
	 */
	FRenderSystem m_Renderer;
	/**
	 * 音声再生のサービス。
	 */
	FAudioPlayer m_Audio;
	/**
	 * 共有実行レーンのJob System。
	 */
	Toolbox::FJobSystem m_ExecutionJobs;
	/**
	 * 共有実行のTask Dispatcher。
	 */
	FTaskDispatcher m_TaskDispatcher;
	/**
	 * シーン間で共有するゲーム状態。
	 */
	Toolbox::TUniquePtr<DGameInstance> m_pGame;
	/**
	 * シーン遷移の管理窓口。
	 */
	FSceneNavigator m_Scenes;
	/**
	 * 経過時間の計測器。
	 */
	FFrameClock m_Clock;
	/**
	 * 開始処理を一度試行したか。
	 */
	bool m_bAttemptedStart = false;
	/**
	 * 開始処理が完了しているか。
	 */
	bool m_bStarted = false;
	/**
	 * 処理の実行中か。
	 */
	bool m_bBusy = false;
	/**
	 * 終了処理が完了しているか。
	 */
	bool m_bShutdown = false;
	/**
	 * 終了要求が出されているか。
	 */
	bool m_bShutdownRequested = false;
};
} // namespace Dxf
