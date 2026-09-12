#include "Toolbox/UniquePtr.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/AssetService.h"
#include "Dxf/GuardValue.h"
namespace Dxf
{
// 必要な依存関係を受け取り、初期状態を構築する。
// @param Assets アセットを読み込むサービス。
// @param Audio 音声再生のサービス。
// @param Game シーン間で共有するゲーム状態。
FSceneNavigator::FSceneNavigator(FAssetService& Assets, FAudioPlayer& Audio, DGameInstance* Game)
    : m_pAssets(&Assets), m_pAudio(&Audio), m_pGame(Game), m_Lifecycle(Audio)
{
}
// 所有する状態を終了し、必要なリソースを解放する。
FSceneNavigator::~FSceneNavigator()
{
	Shutdown();
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
	m_Storage.SetCurrent_Internal(Toolbox::Move(Next));
	m_LastTransitionError.Reset();
	m_Lifecycle.Activate_Internal(*m_Storage.GetCurrent(), {*m_pAudio, *this, m_pGame, AllocateDomain_Internal()});
	return TResult<bool>::Success(true);
}
// 境界で確定した変更を反映する。
TResult<bool> FSceneNavigator::Commit()
{
	if (m_bBusy || m_bShutdown)
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
	FinishDispatch_Internal();
	return Result;
}
// 保留中のオブジェクト変更を反映する。
TResult<void> FSceneNavigator::CommitObjects()
{
	if (m_bBusy || m_bShutdown)
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
	if (m_bBusy || m_bShutdown)
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
			Result = Scene->Tick_Internal({Input, Time, this, m_pGame, m_pAudio, Scene->GetAudioScope()});
		}
	}
	FinishDispatch_Internal();
	return Result;
}
// 対象の描画を要求する。
// @param Render 現在の描画コンテキスト。
TResult<void> FSceneNavigator::Draw(FRenderContext& Render)
{
	if (m_bBusy || m_bShutdown)
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
	if (m_bBusy)
	{
		m_bShutdownRequested = true;
		RequestQuit();
		return;
	}
	if (m_bShutdown)
	{
		return;
	}
	m_bShutdown = true;
	m_bQuit = true;
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bBusy, true);
	// 現在の状態を取得して有効性を確認する。
	if (auto* Current = GetCurrent())
	{
		m_Lifecycle.Stop_Internal(*Current);
	}
	m_Storage.Clear_Internal();
}
} // namespace Dxf
