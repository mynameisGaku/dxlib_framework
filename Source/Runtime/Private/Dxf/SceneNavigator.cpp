#include "Dxf/SceneNavigator.h"
#include "Toolbox/UniquePtr.h"
#include "Dxf/AssetService.h"
#include "Dxf/GuardValue.h"
#include "Toolbox/Log.h"
namespace Dxf
{
namespace
{
// 有効化前のSceneと未公開Scopeを、例外・中断経路でも後始末する。
class FSceneCandidateGuard
{
public:
	// Nextの所有権は呼び出し元が持ち、ガードは借用だけを行う。
	FSceneCandidateGuard(Toolbox::TUniquePtr<DScene>& Next, FTaskDispatcher* Tasks) noexcept
	    : m_pNext(&Next), m_pTasks(Tasks)
	{
	}
	// 中断した候補だけを終了し、現Sceneの所属には触れない。
	~FSceneCandidateGuard()
	{
		// ScopeはOnEnter前まで外へ公開しないため、未使用の登録だけを失効させる。
		if (m_pTasks && m_Scope.IsValid())
		{
			m_pTasks->DestroyScope(m_Scope);
		}
		if (*m_pNext)
		{
			(*m_pNext)->Shutdown_Internal();
		}
	}
	// 同じ候補を二重に後始末しない。
	FSceneCandidateGuard(const FSceneCandidateGuard&) = delete;
	FSceneCandidateGuard& operator=(const FSceneCandidateGuard&) = delete;
	// 初期化成功後に確保し、失敗しても遷移元には触れない。
	bool CreateScope()
	{
		if (!m_pTasks)
		{
			return true;
		}
		m_Scope = m_pTasks->CreateScope();
		return m_Scope.IsValid();
	}
	// 終了フックによるDispatcher停止も、有効化前に確認する。
	bool CanActivate() const noexcept
	{
		return !m_pTasks || !m_pTasks->IsCanceled(m_Scope);
	}
	// 現在Sceneへ所属の管理を渡す。
	FTaskScope ReleaseScope() noexcept
	{
		const FTaskScope Scope = m_Scope;
		m_Scope = {};
		return Scope;
	}

private:
	// 呼び出し元にある遷移候補の所有権。
	Toolbox::TUniquePtr<DScene>* m_pNext;
	// Applicationが所有する非同期窓口。
	FTaskDispatcher* m_pTasks;
	// まだSceneへ公開していない所属。
	FTaskScope m_Scope;
};
} // namespace
// 必要な依存関係を受け取り、初期状態を構築する。
// @param Assets アセットを読み込むサービス。
// @param Audio 音声再生のサービス。
// @param Game シーン間で共有するゲーム状態。
FSceneNavigator::FSceneNavigator(FAssetService& Assets, FAudioPlayer& Audio, DGameInstance* Game,
                                 FTaskDispatcher* Tasks)
    : m_pAssets(&Assets), m_pAudio(&Audio), m_pGame(Game), m_pTasks(Tasks), m_Lifecycle(Audio)
{
	if (m_pTasks && !m_pTasks->IsOwnerThread())
	{
		throw Toolbox::FException("Scene navigator and task dispatcher require the same owner thread");
	}
}
// 所有する状態を終了し、必要なリソースを解放する。
FSceneNavigator::~FSceneNavigator()
{
	Shutdown();
}
// Taskの自己待機や反映中のScene破棄を起こさない境界かを調べる。
bool FSceneNavigator::CanSynchronizeTasks_Internal() const noexcept
{
	return !m_pTasks || m_pTasks->CanSynchronize();
}
// 失効ハンドルは停止フック・デストラクタの終了まで保持し、再投入を拒否する。
bool FSceneNavigator::RetireCurrentScope_Internal() noexcept
{
	return !m_pTasks || !m_SceneScope.IsValid() || m_pTasks->RetireScope(m_SceneScope);
}
// 次の処理境界でのシーン変更を要求する。
// @param Scene 対象のシーン。
TResult<void> FSceneNavigator::RequestChange(Toolbox::TUniquePtr<DScene> Scene)
{
	if (!CanAcceptRequest_Internal())
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Navigator stopped");
	}
	if (!Scene || Scene->GetState() != ELifecycleState::Pending || Scene->IsDestroyRequested())
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Expected a new scene");
	}
	// 待機シーンのデストラクター中は再置換と通知を禁止し、終了処理は復帰後へ遅延する。
	//
	// 処理に入る前の実行中フラグ。
	const bool bWasBusy = m_bBusy;
	{
		// ディスパッチ中の状態を復元するガード。
		TGuardValue DispatchGuard(m_bBusy, true);
		// 要求処理中の状態を復元するガード。
		TGuardValue RequestGuard(m_bReplacingPending, true);
		m_Storage.SetPending_Internal(Toolbox::Move(Scene));
	}
	if (!bWasBusy)
	{
		FinishDispatch_Internal();
	}
	return CanAcceptRequest_Internal()
	           ? TResult<void>{}
	           : TResult<void>::Failure(EErrorCode::InvalidState, "Scene request interrupted by shutdown");
}
// 境界で確定した変更を反映する。
TResult<bool> FSceneNavigator::Commit_Internal()
{
	// 次に割り当てる番号。
	auto Next = m_Storage.TakePending_Internal();
	if (!Next)
	{
		return TResult<bool>::Success(false);
	}
	// 初期化の失敗や例外でも候補を終了する。
	FSceneCandidateGuard Candidate(Next, m_pTasks);
	// 準備済みのシーン。
	TResult<void> Prepared;
	{
		// 準備中の状態を復元するガード。
		TGuardValue PreparingGuard(m_pPreparing, Next.Get());
		Prepared = m_Lifecycle.Prepare_Internal(*Next, {*m_pAssets});
	}
	if (!Prepared)
	{
		m_LastTransitionError = Prepared.Error();
		return TResult<bool>::Failure(Prepared.Error());
	}
	if (WantsQuit() || m_bShutdownRequested)
	{
		// 準備だけのシーンには音声スコープがないため、全体スコープ0を停止しない。
		Next->Shutdown_Internal();
		return TResult<bool>::Success(false);
	}
	if (Next->IsDestroyRequested())
	{
		Next->Shutdown_Internal();
		m_LastTransitionError = FError{EErrorCode::InvalidState, "Scene destroyed during preparation"};
		return TResult<bool>::Failure(*m_LastTransitionError);
	}
	// 新Scopeの確保失敗では旧SceneのScopeを取り消さない。
	try
	{
		if (!Candidate.CreateScope())
		{
			m_LastTransitionError = FError{EErrorCode::InvalidState, "Scene task scope could not be created"};
			return TResult<bool>::Failure(*m_LastTransitionError);
		}
	}
	catch (...)
	{
		m_LastTransitionError = FError{EErrorCode::InvalidState, "Scene task scope allocation failed"};
		return TResult<bool>::Failure(*m_LastTransitionError);
	}
	// OnExit・子の終了・Scene破棄より前に、準備完了と捕捉の解放を確認する。
	if (!RetireCurrentScope_Internal())
	{
		DXF_LOG_ERROR("Scene", "Retiring scope %u/%llu failed; the current scene is kept",
		              static_cast<unsigned>(m_SceneScope.Index), static_cast<unsigned long long>(m_SceneScope.Generation));
		m_LastTransitionError = FError{EErrorCode::InvalidState, "Scene task retirement was not completed"};
		return TResult<bool>::Failure(*m_LastTransitionError);
	}
	if (WantsQuit() || m_bShutdownRequested)
	{
		return TResult<bool>::Success(false);
	}
	// 現在の状態を取得して有効性を確認する。
	if (auto* Current = m_Storage.GetCurrent())
	{
		m_Lifecycle.Stop_Internal(*Current);
	}
	if (WantsQuit() || m_bShutdownRequested)
	{
		Next->Shutdown_Internal();
		return TResult<bool>::Success(false);
	}
	if (!Candidate.CanActivate())
	{
		// 旧Sceneの終了後は継続できないため、停止された窓口で新Sceneを開始しない。
		m_bShutdownRequested = true;
		RequestQuit();
		return TResult<bool>::Success(false);
	}
	m_Storage.SetCurrent_Internal(Toolbox::Move(Next));
	m_SceneScope = Candidate.ReleaseScope();
	// 差し替えで走る旧Sceneのデストラクタも終了を要求できる。
	if (WantsQuit() || m_bShutdownRequested || (m_pTasks && m_pTasks->IsCanceled(m_SceneScope)))
	{
		m_bShutdownRequested = true;
		RequestQuit();
		return TResult<bool>::Success(false);
	}
	m_LastTransitionError.Reset();
	DXF_LOG_INFO("Scene", "Activating scene with task scope %u/%llu", static_cast<unsigned>(m_SceneScope.Index),
	             static_cast<unsigned long long>(m_SceneScope.Generation));
	m_Lifecycle.Activate_Internal(*m_Storage.GetCurrent(),
	                             {*m_pAudio, *this, m_pGame, AllocateDomain_Internal(), m_pTasks, m_SceneScope});
	return TResult<bool>::Success(true);
}
// 境界で確定した変更を反映する。
TResult<bool> FSceneNavigator::Commit()
{
	if (!CanSynchronizeTasks_Internal() || m_bBusy || m_bShutdown)
	{
		return TResult<bool>::Failure(EErrorCode::InvalidState, "Invalid or reentrant scene commit");
	}
	if (WantsQuit())
	{
		return TResult<bool>::Success(false);
	}
	// 処理結果。
	auto Result = TResult<bool>::Success(false);
	{
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		Result = Commit_Internal();
	}
	if (!Result)
	{
		DXF_LOG_WARNING("Scene", "Scene transition failed: %s", Result.Error().Message.CStr());
	}
	FinishDispatch_Internal();
	return Result;
}
// 保留中のオブジェクト変更を反映する。
TResult<void> FSceneNavigator::CommitObjects()
{
	if (!CanSynchronizeTasks_Internal() || m_bBusy || m_bShutdown)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid scene boundary");
	}
	if (WantsQuit())
	{
		return {};
	}
	// 処理結果。
	TResult<void> Result;
	{
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		// 対象のシーンを取得して有効性を確認する。
		if (auto* Scene = GetCurrent())
		{
			// 子のライフサイクルグループを取得して有効性を確認する。
			if (auto* Children = Scene->GetChildren_Internal())
			{
				Children->FreezeBoundary_Internal();
				Result = Children->CommitBoundary_Internal({*m_pAssets});
			}
		}
	}
	FinishDispatch_Internal();
	return Result;
}
// 更新対象へフレーム更新を通知する。
// @param Time フレームの時間情報。
// @param Input フレームの入力情報。
TResult<void> FSceneNavigator::Tick(FFrameTime Time, const FInputSnapshot& Input)
{
	if (!CanSynchronizeTasks_Internal() || m_bBusy || m_bShutdown)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or reentrant scene tick");
	}
	if (WantsQuit())
	{
		return {};
	}
	// 処理結果。
	TResult<void> Result;
	{
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		// 対象のシーンを取得して有効性を確認する。
		if (auto* Scene = GetCurrent())
		{
			Time = Scene->GetClock().Advance(Time);
			Result = Scene->Tick_Internal({Input, Time, this, m_pGame, m_pAudio, Scene->GetAudioScope(),
			                               m_pTasks, m_SceneScope});
		}
	}
	FinishDispatch_Internal();
	return Result;
}
// 対象の描画を要求する。
// @param Render 現在の描画コンテキスト。
TResult<void> FSceneNavigator::Draw(FRenderContext& Render)
{
	if (!CanSynchronizeTasks_Internal() || m_bBusy || m_bShutdown)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or reentrant scene draw");
	}
	if (WantsQuit())
	{
		return {};
	}
	// 処理結果。
	TResult<void> Result;
	{
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		// 対象のシーンを取得して有効性を確認する。
		if (auto* Scene = GetCurrent())
		{
			Result = Scene->Draw_Internal(Render);
		}
	}
	FinishDispatch_Internal();
	return Result;
}
// 通知処理を完了して保留中の変更を反映する。
void FSceneNavigator::FinishDispatch_Internal() noexcept
{
	if (m_bShutdownRequested)
	{
		Shutdown();
	}
}
// アプリケーションの正常終了を要求する。
void FSceneNavigator::RequestQuit() noexcept
{
	m_bQuit = true;
	// 要求の伝播ではフラグだけを変更し、実行中の通知先を安全な削除境界まで存続させる。
	//
	// 現在の状態を取得して有効性を確認する。
	if (auto* Current = GetCurrent())
	{
		Current->RequestDestroy_Internal();
	}
	if (m_pPreparing)
	{
		m_pPreparing->RequestDestroy_Internal();
	}
}
// 管理する処理とリソースを順序どおり終了する。
void FSceneNavigator::Shutdown() noexcept
{
	// WorkerからはSceneに触れない。終了要求は所有スレッドのCommitへ渡す。
	if (m_pTasks && !m_pTasks->IsOwnerThread())
	{
		return;
	}
	if (m_bShutdown)
	{
		return;
	}
	m_bShutdownRequested = true;
	RequestQuit();
	if (m_bBusy || !CanSynchronizeTasks_Internal())
	{
		return;
	}
	// 捕捉のデストラクタからの再入でも、終了はこの境界だけで完了させる。
	TGuardValue Guard(m_bBusy, true);
	if (!RetireCurrentScope_Internal())
	{
		return;
	}
	m_bShutdown = true;
	if (auto* Current = GetCurrent())
	{
		m_Lifecycle.Stop_Internal(*Current);
	}
	m_Storage.Clear_Internal();
	m_SceneScope = {};
}
} // namespace Dxf
