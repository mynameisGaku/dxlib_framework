#include "Dxf/LifecycleObject.h"
#include "Dxf/GuardValue.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
// 使用に必要な初期化を行う。
// @param Context 処理に必要な実行環境。
TResult<void> DLifecycleObject::Initialize_Internal(const FInitContext& Context)
{
	if (m_State != ELifecycleState::Pending || m_bBusy || m_bDestroyRequested)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Object cannot initialize in this state");
	}
	// 処理結果。
	TResult<void> Result;
	{
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		m_State = ELifecycleState::Initializing;
		m_bInitializationAttempted = true;
		try
		{
			Result = OnInitialize(Context);
			if (Result && m_pChildren && !m_bDestroyRequested)
			{
				m_pChildren->FreezeBoundary_Internal();
				Result = m_pChildren->CommitBoundary_Internal(Context);
			}
		}
		// 呼び出し先の例外を処理結果へ変換する。
		catch (const Toolbox::FException& Error)
		{
			Result = TResult<void>::Failure(EErrorCode::UserException, Error.What());
		}
		catch (...)
		{
			Result = TResult<void>::Failure(EErrorCode::UserException, "Unknown initialization exception");
		}
	}
	if (!Result)
	{
		Shutdown_Internal();
		return Result;
	}
	m_State = ELifecycleState::Active;
	return {};
}
// 更新対象へフレーム更新を通知する。
// @param Context 処理に必要な実行環境。
TResult<void> DLifecycleObject::Tick_Internal(const FTickContext& Context)
{
	if (m_bBusy)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Reentrant object update");
	}
	if (!IsInitialized() || m_bDestroyRequested)
	{
		return {};
	}
	try
	{
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		// 仲介が入力を選んだ場合は、自身と子へその入力を渡す（そのフレームで一度だけ）。
		const FInputSnapshot* Routed = RouteInput_Internal(Context);
		const FTickContext Local{Routed != nullptr ? *Routed : Context.Input, Context.Time, Context.Scenes, Context.Game,
		                         Context.Audio, Context.AudioScope, Context.Tasks, Context.TaskScope};
		if (!Local.Time.bPaused || m_bTickWhenPaused)
		{
			OnTick(Local);
		}
		if (m_pChildren && !m_bDestroyRequested)
		{
			return m_pChildren->Tick_Internal(Local);
		}
		return {};
	}
	// 呼び出し先の例外を処理結果へ変換する。
	catch (const Toolbox::FException& Error)
	{
		return TResult<void>::Failure(EErrorCode::UserException, Error.What());
	}
	catch (...)
	{
		return TResult<void>::Failure(EErrorCode::UserException, "Unknown update exception");
	}
}
// 更新対象へ固定時間更新を通知する。
// @param Context 処理に必要な実行環境。
TResult<void> DLifecycleObject::FixedTick_Internal(const FFixedTickContext& Context)
{
	if (m_bBusy)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Reentrant object update");
	}
	if (!IsInitialized() || m_bDestroyRequested)
	{
		return {};
	}
	try
	{
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		if (!Context.bPaused || m_bTickWhenPaused)
		{
			OnFixedTick(Context);
		}
		if (m_pChildren && !m_bDestroyRequested)
		{
			return m_pChildren->FixedTick_Internal(Context);
		}
		return {};
	}
	// 呼び出し先の例外を処理結果へ変換する。
	catch (const Toolbox::FException& Error)
	{
		return TResult<void>::Failure(EErrorCode::UserException, Error.What());
	}
	catch (...)
	{
		return TResult<void>::Failure(EErrorCode::UserException, "Unknown update exception");
	}
}
// 対象の描画を要求する。
// @param Context 処理に必要な実行環境。
TResult<void> DLifecycleObject::Draw_Internal(FRenderContext& Context)
{
	if (m_bBusy)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Reentrant object draw");
	}
	if (!IsInitialized() || m_bDestroyRequested || !m_bVisible)
	{
		return {};
	}
	try
	{
		// 処理終了時に状態を戻すガード。
		TGuardValue Guard(m_bBusy, true);
		OnDraw(Context);
		if (m_pChildren && !m_bDestroyRequested)
		{
			return m_pChildren->Draw_Internal(Context);
		}
		return {};
	}
	// 呼び出し先の例外を処理結果へ変換する。
	catch (const Toolbox::FException& Error)
	{
		return TResult<void>::Failure(EErrorCode::UserException, Error.What());
	}
	catch (...)
	{
		return TResult<void>::Failure(EErrorCode::UserException, "Unknown draw exception");
	}
}
// 管理する処理とリソースを順序どおり終了する。
void DLifecycleObject::Shutdown_Internal() noexcept
{
	if (m_bBusy)
	{
		RequestDestroy_Internal();
		return;
	}
	if (m_State == ELifecycleState::Stopped)
	{
		return;
	}
	// 処理終了時に状態を戻すガード。
	TGuardValue Guard(m_bBusy, true);
	m_State = ELifecycleState::Stopping;
	if (m_pChildren)
	{
		m_pChildren->Shutdown_Internal();
	}
	if (m_bInitializationAttempted)
	{
		OnDeinitialize();
	}
	m_State = ELifecycleState::Stopped;
}
} // namespace Dxf
