// SPDX-License-Identifier: NOASSERTION
#include "Dxf/GameSaveRequest.h"
#include "Dxf/SaveRequestState.h"
#include "Dxf/SaveCodec.h"
#include "Dxf/SaveError.h"
#include "Dxf/GuardValue.h"
#include "Toolbox/FileError.h"
#include "Toolbox/Thread.h"
namespace Dxf
{
using namespace Toolbox;
namespace
{
// 公開前だけ止められる未終端の状態。
bool Pending(EGameSaveRequestState State)
{
	return State == EGameSaveRequestState::Pending || State == EGameSaveRequestState::Writing || State == EGameSaveRequestState::Reading || State == EGameSaveRequestState::Prepared;
}
// 不変値だけをWorkerへ持ち出し、OSの公開処理を実行しない。
ETaskPrepare Prepare(const FSaveStore& Store, const FString& Slot, FSaveDocument Document, const FSavePayloadMigration& Migration, const TSharedPtr<SavePrivate::FRequestState>& State)
{
	{
		FScopedLock Lock(State->Mutex);
		if (State->Status.Load() != static_cast<uint32>(EGameSaveRequestState::Pending))
		{
			return ETaskPrepare::Canceled;
		}
		State->Status.Store(static_cast<uint32>(State->Save ? EGameSaveRequestState::Writing : EGameSaveRequestState::Reading));
	}
	try
	{
		if (State->Save)
		{
			// 取消し済みになってもこのローカル所有権は公開されない。
			FSaveTransaction Transaction = Store.Stage(Slot, Move(Document));
			FScopedLock Lock(State->Mutex);
			if (State->Status.Load() == static_cast<uint32>(EGameSaveRequestState::Writing))
			{
				State->Transaction = Move(Transaction);
			}
		}
		else
		{
			// 保存面とゲーム識別の両方を検証する純粋なCPU結果。
			FSaveLoadResult Loaded = Store.Load(Slot);
			if (!Loaded.Document)
			{
				throw FSaveError(ESaveError::InvalidDocument, "Save slot is empty");
			}
			Loaded.Document = FSaveCodec::Migrate(*Loaded.Document, Document, Migration, Store.GetLimits());
			FScopedLock Lock(State->Mutex);
			if (State->Status.Load() == static_cast<uint32>(EGameSaveRequestState::Reading))
			{
				State->Loaded = Move(Loaded);
			}
		}
	}
	catch (const FException& Error)
	{
		FScopedLock Lock(State->Mutex);
		State->Failed = true;
		SavePrivate::SaveReason(*State, Error.What());
	}
	catch (...)
	{
		FScopedLock Lock(State->Mutex);
		State->Failed = true;
		SavePrivate::SaveReason(*State, "Unexpected save preparation exception");
	}
	// 原因も所有側へ渡すが、取消しを失敗へ上書きしない。
	return Pending(static_cast<EGameSaveRequestState>(State->Status.Load())) ? ETaskPrepare::Success : ETaskPrepare::Canceled;
}
// Task Commitは値受渡しの印だけを付ける。ファイル公開もNavigatorも呼ばない。
bool Deliver(const TSharedPtr<SavePrivate::FRequestState>& State)
{
	FScopedLock Lock(State->Mutex);
	if (Pending(static_cast<EGameSaveRequestState>(State->Status.Load())))
	{
		State->Delivered = true;
		State->Status.Store(static_cast<uint32>(EGameSaveRequestState::Prepared));
	}
	return true;
}
} // namespace
struct FGameSaveRequest::FImpl
{
	// 起動時に確定した保存窓口。
	FSaveStore m_Store;
	// 生存が保証された既存Dispatcherの借用。
	FTaskDispatcher* m_pDispatcher;
	// 自分だけを退役する所属。
	FTaskScope m_RootScope;
	// 最新要求の所属。
	FTaskScope m_CurrentScope;
	// 所有スレッドの識別。
	uint64 m_Owner = FThread::CurrentThreadId();
	// 採用照合に使う最新番号。
	uint64 m_Sequence = 0;
	// 最後に互換性検査を通った読込番号。
	uint64 m_Accepted = 0;
	// 明示退役後は新規要求を拒否する。
	bool m_bRetired = false;
	// Poll再入を拒否する。
	bool m_bPolling = false;
	// CPUの最新結果だけを参照する。
	TSharedPtr<SavePrivate::FRequestState> m_Current;
	// 旧成功値は新規失敗でも保持する。
	TOptional<FSaveLoadResult> m_LastLoaded;
	FImpl(FSaveStore Input, FTaskDispatcher* Borrowed, FTaskScope Parent) : m_Store(Move(Input)), m_pDispatcher(Borrowed)
	{
		RequireOwner();
		if (m_pDispatcher)
		{
			m_RootScope = m_pDispatcher->CreateScope(Parent);
			if (!m_RootScope.IsValid())
			{
				throw FException("Game save request parent Scope unavailable");
			}
		}
	}
	void RequireOwner() const
	{
		if (m_bPolling || FThread::CurrentThreadId() != m_Owner || (m_pDispatcher && !m_pDispatcher->CanSynchronize()))
		{
			throw FException("Game save request requires owner update outside Task or Commit");
		}
	}
	bool Stop(EGameSaveRequestState Target) noexcept
	{
		if (!m_Current)
		{
			return true;
		}
		FScopedLock Lock(m_Current->Mutex);
		// 公開後、公開結果不明、Scene切替後の取消しは成功と扱わない。
		const EGameSaveRequestState Status = static_cast<EGameSaveRequestState>(m_Current->Status.Load());
		if (!Pending(Status))
		{
			return Status == EGameSaveRequestState::Canceled || Status == EGameSaveRequestState::Superseded;
		}
		m_Current->Status.Store(static_cast<uint32>(Target));
		// 未公開結果の破棄は確保せず、tempをロード対象にしない。
		m_Current->Transaction.Reset();
		if (m_pDispatcher && m_CurrentScope.IsValid())
		{
			m_pDispatcher->Cancel(m_CurrentScope);
		}
		return true;
	}
	FGameSaveTicket Begin(FString Slot, FSaveDocument Document, bool Save, FSavePayloadMigration Migration = {})
	{
		RequireOwner();
		if (m_bRetired || m_Sequence == TNumericLimits<uint64>::Max())
		{
			throw FException("Save request retired or sequence exhausted");
		}
		if (m_Current && ((m_Current->Save && Pending(static_cast<EGameSaveRequestState>(m_Current->Status.Load()))) || m_Current->Status.Load() == static_cast<uint32>(EGameSaveRequestState::PublicationUnknown) || m_Current->Status.Load() == static_cast<uint32>(EGameSaveRequestState::TransitionRequested)))
		{
			throw FFileError(EFileOperation::Busy, 0);
		}
		// 入力とTask確保失敗では、旧要求をまだ変更しない。
		const FSaveStore Copy = m_Store;
		const auto State = MakeShared<SavePrivate::FRequestState>();
		State->Sequence = m_Sequence + 1;
		State->Save = Save;
		FTaskRequest Task;
		Task.Prepare = [Copy, Slot, Document, State, Migration]()
		{
			return Prepare(Copy, Slot, Document, Migration, State);
		};
		Task.Commit = [State]()
		{
			return Deliver(State);
		};
		FTaskScope Scope;
		if (m_pDispatcher)
		{
			Scope = m_pDispatcher->CreateScope(m_RootScope);
			if (!Scope.IsValid())
			{
				throw FException("Save request Scope unavailable");
			}
			Task.Scope = Scope;
		}
		Stop(EGameSaveRequestState::Superseded);
		if (m_pDispatcher && m_CurrentScope.IsValid())
		{
			m_pDispatcher->DestroyScope(m_CurrentScope);
		}
		m_Current = State;
		m_CurrentScope = Scope;
		m_Sequence = State->Sequence;
		if (!m_pDispatcher)
		{
			Task.Prepare();
			Task.Commit();
		}
		else
		{
			try
			{
				if (!m_pDispatcher->Submit(Move(Task)))
				{
					FScopedLock Lock(State->Mutex);
					State->Status.Store(static_cast<uint32>(EGameSaveRequestState::Failed));
					SavePrivate::SaveReason(*State, "TaskDispatcher rejected save preparation");
					m_pDispatcher->DestroyScope(Scope);
				}
			}
			catch (...)
			{
				FScopedLock Lock(State->Mutex);
				State->Status.Store(static_cast<uint32>(EGameSaveRequestState::Failed));
				SavePrivate::SaveReason(*State, "Save task submission failed");
				m_pDispatcher->DestroyScope(Scope);
				throw;
			}
		}
		return FGameSaveTicket(State);
	}
};
FGameSaveRequest::FGameSaveRequest(FSaveStore Store, FTaskDispatcher* Dispatcher, FTaskScope Parent) : m_pImpl(MakeUnique<FImpl>(Move(Store), Dispatcher, Parent))
{
}
FGameSaveRequest::~FGameSaveRequest()
{
	Retire();
}
FGameSaveTicket FGameSaveRequest::RequestSave(FString Slot, FSaveDocument Captured)
{
	return m_pImpl->Begin(Move(Slot), Move(Captured), true);
}
FGameSaveTicket FGameSaveRequest::RequestLoad(FString Slot, FSaveDocument Expected, FSavePayloadMigration Migration)
{
	return m_pImpl->Begin(Move(Slot), Move(Expected), false, Move(Migration));
}
bool FGameSaveRequest::Cancel()
{
	m_pImpl->RequireOwner();
	return m_pImpl->Stop(EGameSaveRequestState::Canceled);
}
bool FGameSaveRequest::RetryPublication()
{
	m_pImpl->RequireOwner();
	const auto State = m_pImpl->m_Current;
	if (!State || m_pImpl->m_bRetired || (m_pImpl->m_pDispatcher && m_pImpl->m_pDispatcher->IsCanceled(m_pImpl->m_CurrentScope)))
	{
		return false;
	}
	FScopedLock Lock(State->Mutex);
	if (!State->Save || !State->Transaction || State->Status.Load() != static_cast<uint32>(EGameSaveRequestState::PublicationUnknown))
	{
		return false;
	}
	State->Status.Store(static_cast<uint32>(EGameSaveRequestState::Prepared));
	return true;
}
bool FGameSaveRequest::Retire() noexcept
{
	// 終了は所有側だけが行い、兄弟Scopeは待たない。
	auto& I = *m_pImpl;
	if (I.m_bPolling || FThread::CurrentThreadId() != I.m_Owner || (I.m_pDispatcher && !I.m_pDispatcher->CanSynchronize()))
	{
		return false;
	}
	I.Stop(EGameSaveRequestState::Canceled);
	if (I.m_pDispatcher && !I.m_pDispatcher->RetireScope(I.m_RootScope))
	{
		return false;
	}
	if (I.m_Current)
	{
		FScopedLock Lock(I.m_Current->Mutex);
		I.m_Current->Transaction.Reset();
		if (I.m_Current->Status.Load() == static_cast<uint32>(EGameSaveRequestState::TransitionRequested))
		{
			SavePrivate::SaveReason(*I.m_Current, "Resume owner retired before required Ready");
			I.m_Current->Status.Store(static_cast<uint32>(EGameSaveRequestState::Failed));
		}
	}
	I.m_bRetired = true;
	return true;
}
void FGameSaveRequest::Poll()
{
	auto& I = *m_pImpl;
	I.RequireOwner();
	TGuardValue Guard(I.m_bPolling, true);
	const auto State = I.m_Current;
	if (!State || I.m_bRetired)
	{
		return;
	}
	if (I.m_pDispatcher && I.m_pDispatcher->IsCanceled(I.m_CurrentScope))
	{
		I.Stop(EGameSaveRequestState::Canceled);
		return;
	}
	FScopedLock Lock(State->Mutex);
	if (State->Sequence != I.m_Sequence || State->Status.Load() != static_cast<uint32>(EGameSaveRequestState::Prepared) || !State->Delivered)
	{
		return;
	}
	if (State->Failed)
	{
		State->Status.Store(static_cast<uint32>(EGameSaveRequestState::Failed));
		return;
	}
	try
	{
		if (State->Save)
		{
			State->Status.Store(static_cast<uint32>(EGameSaveRequestState::Publishing));
			State->Transaction->Publish();
			State->Status.Store(static_cast<uint32>(EGameSaveRequestState::Committed));
			State->Transaction.Reset();
		}
		else if (I.m_Accepted != State->Sequence)
		{
			I.m_LastLoaded = Move(State->Loaded);
			I.m_Accepted = State->Sequence;
		}
	}
	catch (const FSaveError& Error)
	{
		SavePrivate::SaveReason(*State, Error.What());
		State->Status.Store(static_cast<uint32>(Error.GetCode() == ESaveError::PublicationUnknown ? EGameSaveRequestState::PublicationUnknown : EGameSaveRequestState::Failed));
	}
	catch (const FException& Error)
	{
		SavePrivate::SaveReason(*State, Error.What());
		State->Status.Store(static_cast<uint32>(EGameSaveRequestState::Failed));
	}
}
const FSaveLoadResult& FGameSaveRequest::GetLoaded() const
{
	m_pImpl->RequireOwner();
	if (!m_pImpl->m_LastLoaded)
	{
		throw FException("No accepted save load");
	}
	return *m_pImpl->m_LastLoaded;
}
uint64 FGameSaveRequest::GetAcceptedSequence() const
{
	m_pImpl->RequireOwner();
	return m_pImpl->m_Accepted;
}
void FGameSaveRequest::MarkTransitionRequested(uint64 Sequence)
{
	m_pImpl->RequireOwner();
	// Scene切替要求を値採用や描画から自動的に再入しない。
	const auto State = m_pImpl->m_Current;
	if (!State || State->Save || Sequence != m_pImpl->m_Accepted || Sequence != State->Sequence || State->Status.Load() != static_cast<uint32>(EGameSaveRequestState::Prepared))
	{
		throw FException("Save resume transition sequence mismatch");
	}
	State->Status.Store(static_cast<uint32>(EGameSaveRequestState::TransitionRequested));
}
void FGameSaveRequest::MarkResumeCompleted(uint64 Sequence)
{
	m_pImpl->RequireOwner();
	// この入口はゲーム再開adapterが必須Readyを確認した後だけ使う。
	const auto State = m_pImpl->m_Current;
	if (!State || State->Save || Sequence != m_pImpl->m_Accepted || Sequence != State->Sequence || State->Status.Load() != static_cast<uint32>(EGameSaveRequestState::TransitionRequested))
	{
		throw FException("Save resume completion sequence mismatch");
	}
	State->Status.Store(static_cast<uint32>(EGameSaveRequestState::Completed));
}
void FGameSaveRequest::MarkResumeFailed(uint64 Sequence, const char* Reason)
{
	m_pImpl->RequireOwner();
	const auto State = m_pImpl->m_Current;
	if (!State || State->Save || Sequence != m_pImpl->m_Accepted || Sequence != State->Sequence)
	{
		throw FException("Save resume failure sequence mismatch");
	}
	FScopedLock Lock(State->Mutex);
	const auto Status = static_cast<EGameSaveRequestState>(State->Status.Load());
	if (Status != EGameSaveRequestState::Prepared && Status != EGameSaveRequestState::TransitionRequested)
	{
		throw FException("Save resume failure state mismatch");
	}
	SavePrivate::SaveReason(*State, Reason ? Reason : "Checkpoint resume failed");
	State->Status.Store(static_cast<uint32>(EGameSaveRequestState::Failed));
}
} // namespace Dxf
