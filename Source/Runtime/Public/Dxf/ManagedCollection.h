#pragma once
#include "Toolbox/UniquePtr.h"
#include "Dxf/ManagedLifecycle.h"
#include "Dxf/ManagedUpdater.h"
#include "Dxf/ManagedDrawDispatcher.h"
#include "Dxf/GuardValue.h"
namespace Dxf
{
/**
 * 所有・更新・ライフサイクルの各機能を結合する窓口。個別の規則は各担当へ委譲する。
 */
template <typename T> class TManagedCollection : public ILifecycleGroup
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	TManagedCollection() : m_Lifecycle(m_Storage), m_Updater(m_Storage), m_DrawDispatcher(m_Storage)
	{
	}
	/**
	 * 所有する状態を終了し、必要なリソースを解放する。
	 */
	~TManagedCollection() override
	{
		Shutdown_Internal();
	}
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	TManagedCollection(const TManagedCollection&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	TManagedCollection& operator=(const TManagedCollection&) = delete;
	/**
	 * ゲームオブジェクトを生成して登録する。
	 * @param Args 生成先へ転送する引数。
	 */
	template <typename U, typename... TArgs> TResult<TObjectHandle<U>> Spawn(TArgs&&... Args)
	{
		static_assert(Toolbox::IsBaseOf<T, U>);
		if (!m_bAccepting || m_bShutdownRequested)
		{
			return TResult<TObjectHandle<U>>::Failure(EErrorCode::InvalidState, "Collection stopped");
		}
		try
		{
			// オブジェクト。
			auto Object = Toolbox::MakeUnique<U>(Toolbox::Forward<TArgs>(Args)...);
			Object->SetCreationOrder_Internal(m_NextCreationOrder++);
			PrepareObject_Internal(*Object);
			// 利用者のコンストラクターが別の参照経由で終了を要求する場合に備える。
			if (!m_bAccepting || m_bShutdownRequested)
			{
				return TResult<TObjectHandle<U>>::Failure(EErrorCode::InvalidState,
				                                          "Collection stopped during construction");
			}
			return TResult<TObjectHandle<U>>::Success(m_Storage.Insert(Toolbox::Move(Object)).template Cast<U>());
		}
		// 呼び出し先の例外を処理結果へ変換する。
		catch (const Toolbox::FException& Error)
		{
			return TResult<TObjectHandle<U>>::Failure(EErrorCode::UserException, Error.What());
		}
		catch (...)
		{
			return TResult<TObjectHandle<U>>::Failure(EErrorCode::UserException, "Unknown constructor exception");
		}
	}
	/**
	 * 対象の破棄を要求する。
	 * @param Handle ハンドル。
	 */
	template <typename U> bool Destroy(const TObjectHandle<U>& Handle) noexcept
	{
		// オブジェクト。
		T* Object = m_Storage.Find_Internal(Handle.GetId());
		if (!Object || Object->IsDestroyRequested())
		{
			return false;
		}
		Object->RequestDestroy_Internal();
		return true;
	}
	/**
	 * 指定型の最初の有効なオブジェクトを探す。
	 */
	template <typename U> TObjectHandle<U> FindFirst() const noexcept
	{
		return m_Storage.template FindFirst<U>();
	}
	/**
	 * 指定型の有効なオブジェクトをすべて集める。
	 */
	template <typename U> Toolbox::TVector<TObjectHandle<U>> FindAll() const
	{
		return m_Storage.template FindAll<U>();
	}
	/**
	 * 有効な要素数を取得する。
	 */
	FORCEINLINE Toolbox::size_t Size() const noexcept
	{
		return m_Storage.Size();
	}
	/**
	 * 利用者の処理を呼ぶ前に今回の変更対象を固定する。
	 */
	void FreezeBoundary_Internal() override
	{
		if (!m_bBusy && m_bAccepting)
		{
			m_Lifecycle.FreezeBoundary_Internal();
		}
	}
	/**
	 * 更新境界でシーンの変更を反映する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> CommitBoundary_Internal(const FInitContext& Context) override
	{
		if (m_bBusy)
		{
			return BusyError_Internal();
		}
		if (!m_bAccepting)
		{
			return {};
		}
		// 処理結果。
		TResult<void> Result;
		{
			// 処理終了時に状態を戻すガード。
			TGuardValue Guard(m_bBusy, true);
			Result = m_Lifecycle.CommitBoundary_Internal(Context);
		}
		FinishDispatch_Internal();
		return Result;
	}
	/**
	 * 更新対象へフレーム更新を通知する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> Tick_Internal(const FTickContext& Context) override
	{
		if (m_bBusy)
		{
			return BusyError_Internal();
		}
		if (!m_bAccepting)
		{
			return {};
		}
		// 処理結果。
		TResult<void> Result;
		{
			// 処理終了時に状態を戻すガード。
			TGuardValue Guard(m_bBusy, true);
			Result = m_Updater.Tick_Internal(Context);
		}
		FinishDispatch_Internal();
		return Result;
	}
	/**
	 * 更新対象へ固定時間更新を通知する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> FixedTick_Internal(const FFixedTickContext& Context) override
	{
		if (m_bBusy)
		{
			return BusyError_Internal();
		}
		if (!m_bAccepting)
		{
			return {};
		}
		// 処理結果。
		TResult<void> Result;
		{
			// 処理終了時に状態を戻すガード。
			TGuardValue Guard(m_bBusy, true);
			Result = m_Updater.FixedTick_Internal(Context);
		}
		FinishDispatch_Internal();
		return Result;
	}
	/**
	 * 対象の描画を要求する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> Draw_Internal(FRenderContext& Context) override
	{
		if (m_bBusy)
		{
			return BusyError_Internal();
		}
		if (!m_bAccepting)
		{
			return {};
		}
		// 処理結果。
		TResult<void> Result;
		{
			// 処理終了時に状態を戻すガード。
			TGuardValue Guard(m_bBusy, true);
			Result = m_DrawDispatcher.Draw_Internal(Context);
		}
		FinishDispatch_Internal();
		return Result;
	}
	/**
	 * 子を含む停止要求を記録する。
	 */
	void RequestStop_Internal() noexcept override
	{
		m_bShutdownRequested = true;
		m_Storage.ForEach_Internal(
		    [](T& Object)
		    {
			    Object.RequestDestroy_Internal();
		    });
	}
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	void Shutdown_Internal() noexcept override
	{
		if (m_bBusy)
		{
			// 通知を直ちに止め、削除は実行中のコールバックが戻るまで遅延する。
			RequestStop_Internal();
			return;
		}
		if (!m_bAccepting)
		{
			return;
		}
		m_bAccepting = false;
		m_bShutdownRequested = false;
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		m_Lifecycle.Shutdown_Internal();
	}

protected:
	/**
	 * オブジェクトを使用可能な状態まで初期化する。
	 */
	virtual void PrepareObject_Internal(T&)
	{
	}

private:
	/**
	 * 再入による実行不可を示すエラーを生成する。
	 */
	static TResult<void> BusyError_Internal()
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Reentrant collection dispatch");
	}
	/**
	 * 通知処理を完了して保留中の変更を反映する。
	 */
	void FinishDispatch_Internal() noexcept
	{
		if (m_bShutdownRequested)
		{
			Shutdown_Internal();
		}
	}
	/**
	 * オブジェクトの格納先。
	 */
	TSlotMap<T> m_Storage;
	/**
	 * 初期化と終了の制御器。
	 */
	TManagedLifecycle<T> m_Lifecycle;
	/**
	 * フレーム更新の実行器。
	 */
	TManagedUpdater<T> m_Updater;
	/**
	 * 描画通知の実行器。
	 */
	TManagedDrawDispatcher<T> m_DrawDispatcher;
	/**
	 * 次に割り当てる生成順序。
	 */
	Toolbox::uint64 m_NextCreationOrder = 1;
	/**
	 * 新しいオブジェクトの生成を受け付けるか。
	 */
	bool m_bAccepting = true;
	/**
	 * 処理の実行中か。
	 */
	bool m_bBusy = false;
	/**
	 * 終了要求が出されているか。
	 */
	bool m_bShutdownRequested = false;
};
} // namespace Dxf
