#pragma once
#include "Toolbox/UniquePtr.h"
#include "Dxf/SceneStorage.h"
#include "Dxf/SceneLifecycle.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
class FAssetService;
/**
 * シーン遷移の要求と反映を管理する型。
 */
class FSceneNavigator
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Assets アセットを読み込むサービス。
	 * @param Audio 音声再生のサービス。
	 * @param Game シーン間で共有するゲーム状態。
	 */
	FSceneNavigator(FAssetService& Assets, FAudioPlayer& Audio, DGameInstance* Game = nullptr);
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
		/**
		 * 呼び出し先の例外を処理結果へ変換する。
		 */
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
	 * 境界で確定した変更を反映する。
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
	DScene* GetCurrent() const noexcept
	{
		return m_Storage.GetCurrent();
	}
	/**
	 * 直近のシーン遷移のエラーを取得する。
	 */
	const Toolbox::TOptional<FError>& GetLastTransitionError() const noexcept
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
	bool WantsQuit() const noexcept
	{
		return m_bQuit || m_bShutdown;
	}
	/**
	 * 管理する処理とリソースを順序どおり終了する。
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
