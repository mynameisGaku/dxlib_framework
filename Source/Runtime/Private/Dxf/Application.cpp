#include "Dxf/Application.h"
#include "Toolbox/UniquePtr.h"
#include "Dxf/GuardValue.h"
#include "Toolbox/Log.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
// 必要な依存関係を受け取り、初期状態を構築する。
// @param Services アプリケーションが利用するサービス。
// @param Settings 初期化に使用する設定。
// @param Game シーン間で共有するゲーム状態。
FApplication::FApplication(FBackendServices Services, FApplicationSettings Settings,
                           Toolbox::TUniquePtr<DGameInstance> Game)
    : m_pPlatform(&Services.Platform), m_Settings(Toolbox::Move(Settings)), m_Session(Services.Platform),
      m_Input(Services.Input), m_Assets(Services.Textures, Services.Sounds, Services.Fonts, Services.pModels),
      m_Renderer(Services.Renderer), m_Audio(Services.Sounds), m_ExecutionJobs(m_Settings.ExecutionThreadCount),
      m_TaskDispatcher(m_ExecutionJobs), m_pGame(Toolbox::Move(Game)),
      m_Scenes(m_Assets, m_Audio, m_pGame.Get(), &m_TaskDispatcher), m_Clock(m_Settings.MaxDeltaSeconds)
{
	// 全メンバー構築後に共有実行器を結び付け、Scene側へ所有権を渡さない。
	auto Connected = m_Renderer.SetExecutionJobs(m_ExecutionJobs);
	if (!Connected)
	{
		throw Toolbox::FException(Connected.Error().Message);
	}
}
// 所有する状態を終了し、必要なリソースを解放する。
FApplication::~FApplication()
{
	Shutdown();
}
// 初期化を行い実行を開始する。
// @param InitialScene 最初に開始するシーン。
TResult<void> FApplication::Start_Internal(Toolbox::TUniquePtr<DScene> InitialScene)
{
	if (!InitialScene)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "An initial scene is required");
	}
	// DxLibの開始結果。
	auto SessionResult = m_Session.Initialize(m_Settings.Window);
	if (!SessionResult)
	{
		return SessionResult;
	}
	if (!m_Settings.ProjectRoot.IsEmpty())
	{
		// アセット使用より先に一度だけ確定する基準ディレクトリ。
		if (!m_Assets.SetProjectRoot(Toolbox::FPath(m_Settings.ProjectRoot)))
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument,
			                              "ProjectRoot must be an absolute directory");
		}
	}
	if (m_pGame)
	{
		// ゲーム更新の結果。
		auto GameResult = m_pGame->Initialize_Internal({m_Assets});
		if (!GameResult)
		{
			return GameResult;
		}
	}
	if (m_bShutdownRequested)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Shutdown requested during startup");
	}
	// 反映する要求。
	auto Request = m_Scenes.RequestChange(Toolbox::Move(InitialScene));
	if (!Request)
	{
		return Request;
	}
	// 保留中の変更の反映結果。
	auto Commit = m_Scenes.Commit();
	if (!Commit)
	{
		return TResult<void>::Failure(Commit.Error());
	}
	m_bStarted = true;
	return {};
}
// 初期化を行い実行を開始する。
// @param InitialScene 最初に開始するシーン。
TResult<void> FApplication::Start(Toolbox::TUniquePtr<DScene> InitialScene)
{
	if (!m_TaskDispatcher.CanSynchronize() || m_Scenes.IsDispatching() ||
	    m_bAttemptedStart || m_bShutdown || m_bBusy)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Application is single-use or busy");
	}
	m_bAttemptedStart = true;
	// 処理結果。
	TResult<void> Result;
	try
	{
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		Result = Start_Internal(Toolbox::Move(InitialScene));
	}
	// 呼び出し先の例外を処理結果へ変換する。
	catch (const Toolbox::FException& Exception)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, Exception.What());
	}
	catch (...)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, "Unknown startup exception");
	}
	if (!Result)
	{
		DXF_LOG_ERROR("Application", "Start failed: %s", Result.Error().Message.CStr());
	}
	else
	{
		DXF_LOG_INFO("Application", "Started (execution lanes=%u)",
		             static_cast<unsigned>(m_ExecutionJobs.GetExecutionThreadCount()));
	}
	if (!Result || WantsQuit_Internal())
	{
		Shutdown();
	}
	return Result;
}
// 正常終了が要求されているかを調べる。
bool FApplication::WantsQuit_Internal() const noexcept
{
	return m_bShutdownRequested || m_Scenes.WantsQuit();
}
// 1フレーム分の入力・更新・描画を進める。
// @param NowSeconds 単調増加する現在時刻の秒数。
TResult<bool> FApplication::Step_Internal(Toolbox::f64 NowSeconds)
{
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	// OSイベント処理の結果。
	auto Events = m_pPlatform->PumpEvents();
	if (!Events)
	{
		return Events;
	}
	if (!Events.Value())
	{
		return TResult<bool>::Success(false);
	}
	// フレームの時間情報。
	auto Time = m_Clock.Sample(NowSeconds);
	if (!Time)
	{
		return TResult<bool>::Failure(Time.Error());
	}
	// フレームの入力情報。
	auto Input = m_Input.Update();
	if (!Input)
	{
		return TResult<bool>::Failure(Input.Error());
	}
	// シーン遷移の反映結果。
	auto Transition = m_Scenes.Commit();
	// 以前のシーンが有効な間は、置き換えに失敗しても実行を継続できる。
	if (!Transition && !m_Scenes.GetCurrent())
	{
		return TResult<bool>::Failure(Transition.Error());
	}
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	// 所有するオブジェクト群。
	auto Objects = m_Scenes.CommitObjects();
	if (!Objects)
	{
		return TResult<bool>::Failure(Objects.Error());
	}
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	if (m_pGame)
	{
		// ゲーム更新に渡すコンテキスト。
		auto GameTick =
		    m_pGame->Tick_Internal({m_Input.GetSnapshot(), Time.Value(), &m_Scenes, m_pGame.Get(), &m_Audio, 0,
		                               &m_TaskDispatcher, m_TaskDispatcher.GetRootScope()});
		if (!GameTick)
		{
			return TResult<bool>::Failure(GameTick.Error());
		}
	}
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	// 更新に渡すフレーム情報。
	auto Tick = m_Scenes.Tick(Time.Value(), m_Input.GetSnapshot());
	if (!Tick)
	{
		return TResult<bool>::Failure(Tick.Error());
	}
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	// Scene Scopeは切替の中で更新済み。Commit中の終了要求は描画開始前に処理する。
	m_TaskDispatcher.PumpCommits();
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	// 音声再生のサービス。
	auto Audio = m_Audio.Tick();
	if (!Audio)
	{
		return TResult<bool>::Failure(Audio.Error());
	}
	// フレーム開始の結果。
	auto Begin = m_Renderer.BeginFrame(m_Settings.Window.Width, m_Settings.Window.Height, m_Settings.ClearColor);
	if (!Begin)
	{
		return TResult<bool>::Failure(Begin.Error());
	}
	// 描画処理の結果。
	auto Draw = m_Scenes.Draw(m_Renderer.GetContext());
	if (!Draw)
	{
		return TResult<bool>::Failure(Draw.Error());
	}
	if (WantsQuit_Internal())
	{
		m_Renderer.CancelFrame();
		return TResult<bool>::Success(false);
	}
	// 画面提示の結果。
	auto Present = m_Renderer.EndFrame();
	if (!Present)
	{
		return TResult<bool>::Failure(Present.Error());
	}
	m_Assets.CollectUnused();
	return TResult<bool>::Success(!WantsQuit_Internal());
}
// 1フレーム分の入力・更新・描画を進める。
// @param NowSeconds 単調増加する現在時刻の秒数。
TResult<bool> FApplication::Step(Toolbox::f64 NowSeconds)
{
	if (!m_TaskDispatcher.CanSynchronize() || m_Scenes.IsDispatching())
	{
		return TResult<bool>::Failure(EErrorCode::InvalidState, "Application Step requires a safe owner boundary");
	}
	// Step外で手動PumpしたCommitからの終了要求も、安全な次回境界で完了する。
	if (!m_bBusy && !m_bShutdown && m_bShutdownRequested)
	{
		Shutdown();
		return TResult<bool>::Success(false);
	}
	if (m_bBusy || !IsRunning())
	{
		return TResult<bool>::Failure(EErrorCode::InvalidState, "Application is stopped or Step is reentrant");
	}
	// 処理結果。
	auto Result = TResult<bool>::Success(false);
	try
	{
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		Result = Step_Internal(NowSeconds);
	}
	// 呼び出し先の例外を処理結果へ変換する。
	catch (const Toolbox::FException& Exception)
	{
		Result = TResult<bool>::Failure(EErrorCode::UserException, Exception.What());
	}
	catch (...)
	{
		Result = TResult<bool>::Failure(EErrorCode::UserException, "Unknown frame exception");
	}
	if (!Result)
	{
		// フレームを止めた最初の失敗を、終了処理の前に記録する。
		DXF_LOG_ERROR("Application", "Frame failed: %s", Result.Error().Message.CStr());
	}
	if (!Result || !Result.Value() || WantsQuit_Internal())
	{
		Shutdown();
		if (Result)
		{
			Result = TResult<bool>::Success(false);
		}
	}
	return Result;
}
// 管理する処理とリソースを順序どおり終了する。
void FApplication::Shutdown() noexcept
{
	// Applicationの操作は所有スレッド限定。WorkerはCommit経由で終了を要求する。
	if (!m_TaskDispatcher.IsOwnerThread())
	{
		return;
	}
	if (m_bBusy || m_Scenes.IsDispatching() || !m_TaskDispatcher.CanSynchronize())
	{
		DXF_LOG_VERBOSE("Application", "Shutdown deferred to the next owner-thread boundary");
		m_bShutdownRequested = true;
		m_Scenes.RequestQuit();
		if (m_pGame)
		{
			m_pGame->RequestDestroy_Internal();
		}
		return;
	}
	if (m_bShutdown)
	{
		return;
	}
	m_bShutdown = true;
	DXF_LOG_INFO("Application", "Shutting down");
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bBusy, true);
	m_TaskDispatcher.Shutdown();
	m_ExecutionJobs.Shutdown();
	m_Scenes.Shutdown();
	if (m_pGame)
	{
		m_pGame->Shutdown_Internal();
		m_pGame.Reset();
	}
	m_Audio.Shutdown();
	m_Renderer.CancelFrame();
	m_Assets.Shutdown();
	m_Session.Shutdown();
}
} // namespace Dxf
