#pragma once
#include "Toolbox/UniquePtr.h"
#include "Dxf/SceneStorage.h"
#include "Dxf/SceneLifecycle.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Utility.h"
#include "Dxf/TaskDispatcher.h"
namespace Dxf
{
class FAssetService;
/**
 * シーン遷移の要求と反映を管理する型。すべての操作は所有スレッドで行う。
 */
class FSceneNavigator
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Assets アセットを読み込むサービス。
	 * @param Audio 音声再生のサービス。
	 * @param Game シーン間で共有するゲーム状態。
	 * @param Tasks 借用するTask窓口。指定時は同じ所有スレッドで操作し、Navigatorより長く生存させる。
	 */
	FSceneNavigator(FAssetService& Assets, FAudioPlayer& Audio, DGameInstance* Game = nullptr,
	                FTaskDispatcher* Tasks = nullptr);
	/**
	 * 所有する状態を終了し、必要なリソースを解放する。
	 */
	~FSceneNavigator();
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FSceneNavigator(const FSceneNavigator&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FSceneNavigator& operator=(const FSceneNavigator&) = delete;
	/**
	 * 次の処理境界でのシーン変更を要求する。
	 * @param Scene 対象のシーン。
	 */
	TResult<void> RequestChange(Toolbox::TUniquePtr<DScene> Scene);
	/**
	 * 次の処理境界でのシーン変更を要求する。
	 * @param Args 生成先へ転送する引数。
	 */
	template <typename T, typename... TArgs> TResult<void> RequestChange(TArgs&&... Args)
	{
		static_assert(Toolbox::IsBaseOf<DScene, T>);
		if (!CanAcceptRequest_Internal())
		{
			return TResult<void>::Failure(EErrorCode::InvalidState, "Navigator cannot accept a scene request");
		}
		try
		{
			return RequestChange(Toolbox::MakeUnique<T>(Toolbox::Forward<TArgs>(Args)...));
		}
		// 呼び出し先の例外を処理結果へ変換する。
		catch (const Toolbox::FException& Error)
		{
			return TResult<void>::Failure(EErrorCode::UserException, Error.What());
		}
		catch (...)
		{
			return TResult<void>::Failure(EErrorCode::UserException, "Unknown scene constructor exception");
		}
	}
	/**
	 * 境界で確定した変更を反映する。Task反映中や準備中の再入は失敗で拒否する。
	 * 初期化と新Scope確保の成功後、旧Scopeを退役してから終了通知を行う。
	 */
	TResult<bool> Commit();
	/**
	 * 保留中のオブジェクト変更を反映する。
	 */
	TResult<void> CommitObjects();
	/**
	 * 更新対象へフレーム更新を通知する。
	 * @param Time フレームの時間情報。
	 * @param Input フレームの入力情報。
	 */
	TResult<void> Tick(FFrameTime Time, const FInputSnapshot& Input);
	/**
	 * 対象の描画を要求する。
	 * @param Render 現在の描画コンテキスト。
	 */
	TResult<void> Draw(FRenderContext& Render);
	/**
	 * 現在の状態を取得する。
	 */
	FORCEINLINE DScene* GetCurrent() const noexcept
	{
		return m_Storage.GetCurrent();
	}
	/**
	 * 利用者フックまたはSceneの破棄を実行中か。所有スレッドでのみ参照する。
	 */
	FORCEINLINE bool IsDispatching() const noexcept
	{
		return m_bBusy;
	}
	/**
	 * 現在SceneのTask Scope。終了フック中は失効した旧ハンドルを保つ。
	 * OnInitializeには遷移先のScopeを提供しない。Scene用TaskはOnEnter以降に投入する。
	 */
	FORCEINLINE FTaskScope GetTaskScope() const noexcept
	{
		return m_SceneScope;
	}
	/**
	 * 直近のシーン遷移のエラーを取得する。
	 */
	FORCEINLINE const Toolbox::TOptional<FError>& GetLastTransitionError() const noexcept
	{
		return m_LastTransitionError;
	}
	/**
	 * アプリケーションの正常終了を要求する。
	 */
	void RequestQuit() noexcept;
	/**
	 * 正常終了が要求されているかを調べる。
	 */
	FORCEINLINE bool WantsQuit() const noexcept
	{
		return m_bQuit || m_bShutdown;
	}
	/**
	 * 所属Taskの退役後にSceneを終了する。TaskやSceneの通知中は要求だけを記録する。
	 * 遅延時は所有スレッドの安全な境界で再度呼び、完了させる。
	 */
	void Shutdown() noexcept;

private:
	/**
	 * シーン変更要求を受け付けられるかを調べる。
	 */
	bool CanAcceptRequest_Internal() const noexcept
	{
		return !WantsQuit() && !m_bShutdownRequested && !m_bReplacingPending;
	}
	/**
	 * Taskの反映・捕捉破棄に再入せずSceneを操作できるかを調べる。
	 */
	bool CanSynchronizeTasks_Internal() const noexcept;
	/**
	 * 現在SceneのPrepareと捕捉を退役する。失敗時はSceneを停止・破棄しない。
	 */
	bool RetireCurrentScope_Internal() noexcept;
	/**
	 * 境界で確定した変更を反映する。
	 */
	TResult<bool> Commit_Internal();
	/**
	 * 通知処理を完了して保留中の変更を反映する。
	 */
	void FinishDispatch_Internal() noexcept;
	/**
	 * アセットを読み込むサービス。
	 */
	FAssetService* m_pAssets;
	/**
	 * 音声再生のサービス。
	 */
	FAudioPlayer* m_pAudio;
	/**
	 * シーン間で共有するゲーム状態。
	 */
	DGameInstance* m_pGame;
	/**
	 * Applicationが所有するTask窓口。単独利用では未接続。
	 */
	FTaskDispatcher* m_pTasks;
	/**
	 * 有効化した現在Sceneの所属。ポインタのアドレス比較では追跡しない。
	 */
	FTaskScope m_SceneScope;
	/**
	 * 初期化中のシーンへの非所有参照。
	 */
	DScene* m_pPreparing = nullptr;
	/**
	 * オブジェクトの格納先。
	 */
	FSceneStorage m_Storage;
	/**
	 * 初期化と終了の制御器。
	 */
	FSceneLifecycle m_Lifecycle;
	/**
	 * 直近のシーン遷移のエラー。
	 */
	Toolbox::TOptional<FError> m_LastTransitionError;
	/**
	 * 処理の実行中か。
	 */
	bool m_bBusy = false;
	/**
	 * 待機中のシーンを置き換えているか。
	 */
	bool m_bReplacingPending = false;
	/**
	 * 終了処理が完了しているか。
	 */
	bool m_bShutdown = false;
	/**
	 * 終了要求が出されているか。
	 */
	bool m_bShutdownRequested = false;
	/**
	 * 正常終了が要求されているか。
	 */
	bool m_bQuit = false;
};
} // namespace Dxf
