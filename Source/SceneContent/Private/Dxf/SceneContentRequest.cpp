// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SceneContentRequest.h"
#include "Dxf/ContentRequestState.h"
#include "Toolbox/Thread.h"
#include "Dxf/GuardValue.h"
namespace Dxf::ContentPrivate
{
// 最初の例外を確保なしで残す。診断作成の失敗で上書きしない。
void SaveReason(FContentRequestState& State, const char* Reason) noexcept
{
	Toolbox::size_t Index = 0;
	while (Reason[Index] != 0 && Index + 1 < sizeof(State.FailureReason))
	{
		State.FailureReason[Index] = Reason[Index];
		++Index;
	}
	State.FailureReason[Index] = 0;
}
ETaskPrepare Prepare(const FSceneContentSource& Source, const Toolbox::FString& Path, const Toolbox::TSharedPtr<FContentRequestState>& State)
{
	struct FDone
	{
		FContentRequestState& State;
		~FDone()
		{
			State.PrepareDone.Store(1);
		}
	} Done{*State};
	auto Expected = static_cast<Toolbox::uint32>(ESceneContentRequestState::Pending);
	if (!State->Status.CompareExchange(Expected, static_cast<Toolbox::uint32>(ESceneContentRequestState::Preparing)))
	{
		return ETaskPrepare::Canceled;
	}
	try
	{
		// 読解の途中で強制停止せず、有限上限のある読解の前後で取消しを確認する。
		if (State->Dimension == 2)
		{
			auto Definition = Source.LoadScene2D(Path);
			Toolbox::FScopedLock Lock(State->Mutex);
			if (State->Status.Load() == static_cast<Toolbox::uint32>(ESceneContentRequestState::Preparing))
			{
				State->Definition2D = Toolbox::Move(Definition);
			}
		}
		else
		{
			auto Definition = Source.LoadScene3D(Path);
			Toolbox::FScopedLock Lock(State->Mutex);
			if (State->Status.Load() == static_cast<Toolbox::uint32>(ESceneContentRequestState::Preparing))
			{
				State->Definition3D = Toolbox::Move(Definition);
			}
		}
	}
	catch (const FSceneContentError& Error)
	{
		Toolbox::FScopedLock Lock(State->Mutex);
		State->bFailed = true;
		SaveReason(*State, Error.What());
		try
		{
			State->Diagnostic = Error.GetDiagnostic();
		}
		catch (...)
		{
			// 確保なしの理由を保持する。採用側で失敗へ確定する。
		}
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::FScopedLock Lock(State->Mutex);
		State->bFailed = true;
		SaveReason(*State, Error.What());
	}
	catch (...)
	{
		Toolbox::FScopedLock Lock(State->Mutex);
		State->bFailed = true;
		SaveReason(*State, "Unexpected CPU preparation exception");
	}
	// 失敗診断もCPU結果として公開する。空定義をReadyへ変更するものではない。
	return State->Status.Load() == static_cast<Toolbox::uint32>(ESceneContentRequestState::Preparing)
	           ? ETaskPrepare::Success
	           : ETaskPrepare::Canceled;
}
bool Publish(const Toolbox::TSharedPtr<FContentRequestState>& State)
{
	Toolbox::FScopedLock Lock(State->Mutex);
	if (State->Status.Load() == static_cast<Toolbox::uint32>(ESceneContentRequestState::Preparing))
	{
		State->bCommitted = true;
	}
	return true;
}
} // namespace Dxf::ContentPrivate
namespace Dxf
{
struct FSceneContentRequest::FImpl
{
	FSceneContentSource m_Source;
	FTaskDispatcher* m_pDispatcher = nullptr;
	FTaskScope m_RootScope;
	FTaskScope m_CurrentScope;
	Toolbox::uint64 m_OwnerThread = Toolbox::FThread::CurrentThreadId();
	Toolbox::uint64 m_Sequence = 0;
	Toolbox::uint64 m_AcceptedSequence = 0;
	bool m_bRetired = false;
	bool m_bPolling = false;
	Toolbox::TSharedPtr<ContentPrivate::FContentRequestState> m_Current;
	FPreparedScene2D m_Last2D;
	FPreparedScene3D m_Last3D;
	FImpl(FSceneContentSource Source, FTaskDispatcher* Dispatcher, FTaskScope Parent)
	    : m_Source(Toolbox::Move(Source)), m_pDispatcher(Dispatcher)
	{
		RequireOwner();
		if (Dispatcher)
		{
			m_RootScope = Dispatcher->CreateScope(Parent);
			if (!m_RootScope.IsValid())
			{
				throw Toolbox::FException("Content request parent Scope is unavailable");
			}
		}
	}
	void RequireOwner() const
	{
		if (m_bPolling || Toolbox::FThread::CurrentThreadId() != m_OwnerThread || (m_pDispatcher && !m_pDispatcher->CanSynchronize()))
		{
			throw Toolbox::FException("Content request needs owner update outside Task or Commit");
		}
	}
	void StopCurrent(ESceneContentRequestState Target) noexcept
	{
		if (m_Current)
		{
			Toolbox::FScopedLock Lock(m_Current->Mutex);
			const auto State = static_cast<ESceneContentRequestState>(m_Current->Status.Load());
			if (State == ESceneContentRequestState::Pending || State == ESceneContentRequestState::Preparing)
			{
				m_Current->Status.Store(static_cast<Toolbox::uint32>(Target));
			}
		}
		if (m_pDispatcher && m_CurrentScope.IsValid())
		{
			m_pDispatcher->Cancel(m_CurrentScope);
		}
	}
	FSceneContentTicket Begin(Toolbox::FString Path, Toolbox::uint32 Dimension)
	{
		RequireOwner();
		if (m_bRetired || m_Sequence == Toolbox::uint64(-1))
		{
			throw Toolbox::FException("Content request is retired or sequence is exhausted");
		}
		// 入力コピーの失敗では現在の未完了要求を置換しない。
		const auto Source = m_Source;
		auto State = Toolbox::MakeShared<ContentPrivate::FContentRequestState>();
		State->Sequence = m_Sequence + 1;
		State->Dimension = Dimension;
		State->Diagnostic.Path = Path;
		FTaskScope Scope;
		if (m_pDispatcher)
		{
			Scope = m_pDispatcher->CreateScope(m_RootScope);
			if (!Scope.IsValid())
			{
				throw Toolbox::FException("Content request Scope is unavailable");
			}
		}
		StopCurrent(ESceneContentRequestState::Superseded);
		if (m_pDispatcher && m_CurrentScope.IsValid())
		{
			m_pDispatcher->DestroyScope(m_CurrentScope);
		}
		m_Current = State;
		m_CurrentScope = Scope;
		m_Sequence = State->Sequence;
		if (!m_pDispatcher)
		{
			ContentPrivate::Prepare(Source, Path, State);
			ContentPrivate::Publish(State);
		}
		else
		{
			try
			{
				const auto Accepted = m_pDispatcher->Submit({[Source, Path, State]()
				                                             {
					                                             return ContentPrivate::Prepare(Source, Path, State);
				                                             },
				                                             [State]()
				                                             {
					                                             return ContentPrivate::Publish(State);
				                                             },
				                                             Scope});
				if (!Accepted)
				{
					Toolbox::FScopedLock Lock(State->Mutex);
					State->Status.Store(static_cast<Toolbox::uint32>(ESceneContentRequestState::Failed));
					ContentPrivate::SaveReason(*State, "TaskDispatcher rejected content preparation");
					m_pDispatcher->DestroyScope(Scope);
				}
			}
			catch (...)
			{
				Toolbox::FScopedLock Lock(State->Mutex);
				State->Status.Store(static_cast<Toolbox::uint32>(ESceneContentRequestState::Failed));
				ContentPrivate::SaveReason(*State, "Content task submission failed");
				m_pDispatcher->DestroyScope(Scope);
				throw;
			}
		}
		return FSceneContentTicket(State);
	}
};
FSceneContentRequest::FSceneContentRequest(FSceneContentSource Source, FTaskDispatcher* Dispatcher, FTaskScope Parent)
    : m_pImpl(Toolbox::MakeUnique<FImpl>(Toolbox::Move(Source), Dispatcher, Parent))
{
}
FSceneContentRequest::~FSceneContentRequest()
{
	Retire();
}
FSceneContentTicket FSceneContentRequest::LoadScene2D(Toolbox::FString Path)
{
	return m_pImpl->Begin(Toolbox::Move(Path), 2);
}
FSceneContentTicket FSceneContentRequest::LoadScene3D(Toolbox::FString Path)
{
	return m_pImpl->Begin(Toolbox::Move(Path), 3);
}
void FSceneContentRequest::Cancel()
{
	m_pImpl->RequireOwner();
	m_pImpl->StopCurrent(ESceneContentRequestState::Canceled);
}
bool FSceneContentRequest::Retire() noexcept
{
	auto& I = *m_pImpl;
	if (I.m_bPolling || Toolbox::FThread::CurrentThreadId() != I.m_OwnerThread || (I.m_pDispatcher && !I.m_pDispatcher->CanSynchronize()))
	{
		return false;
	}
	I.StopCurrent(ESceneContentRequestState::Canceled);
	if (I.m_pDispatcher && !I.m_pDispatcher->RetireScope(I.m_RootScope))
	{
		return false;
	}
	I.m_bRetired = true;
	return true;
}
void FSceneContentRequest::Poll(FAssetService& Assets)
{
	auto& I = *m_pImpl;
	I.RequireOwner();
	TGuardValue Polling(I.m_bPolling, true);
	const auto State = I.m_Current;
	if (!State || I.m_bRetired)
	{
		return;
	}
	if (I.m_pDispatcher && I.m_pDispatcher->IsCanceled(I.m_CurrentScope))
	{
		I.StopCurrent(ESceneContentRequestState::Canceled);
		return;
	}
	{
		Toolbox::FScopedLock Lock(State->Mutex);
		if (State->Status.Load() != static_cast<Toolbox::uint32>(ESceneContentRequestState::Preparing) || !State->bCommitted)
		{
			return;
		}
		if (State->bFailed)
		{
			State->Status.Store(static_cast<Toolbox::uint32>(ESceneContentRequestState::Failed));
			return;
		}
	}
	try
	{
		// Commit外の所有側だけがNativeを取り込む。失敗なら最後の成功値は触らない。
		if (State->Dimension == 2)
		{
			auto Prepared = PrepareScene(Toolbox::Move(State->Definition2D), Assets);
			if (I.m_pDispatcher && I.m_pDispatcher->IsCanceled(I.m_CurrentScope))
			{
				I.StopCurrent(ESceneContentRequestState::Canceled);
				return;
			}
			I.m_Last2D = Toolbox::Move(Prepared);
		}
		else
		{
			auto Prepared = PrepareScene(Toolbox::Move(State->Definition3D), Assets);
			if (I.m_pDispatcher && I.m_pDispatcher->IsCanceled(I.m_CurrentScope))
			{
				I.StopCurrent(ESceneContentRequestState::Canceled);
				return;
			}
			I.m_Last3D = Toolbox::Move(Prepared);
		}
		I.m_AcceptedSequence = State->Sequence;
		State->Status.Store(static_cast<Toolbox::uint32>(ESceneContentRequestState::Ready));
	}
	catch (const FSceneContentError& Error)
	{
		Toolbox::FScopedLock Lock(State->Mutex);
		ContentPrivate::SaveReason(*State, Error.What());
		State->Status.Store(static_cast<Toolbox::uint32>(ESceneContentRequestState::Failed));
		try
		{
			State->Diagnostic = Error.GetDiagnostic();
		}
		catch (...)
		{
			// 失敗した状態と確保なしの最初の理由は既に確定している。
		}
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::FScopedLock Lock(State->Mutex);
		ContentPrivate::SaveReason(*State, Error.What());
		State->Status.Store(static_cast<Toolbox::uint32>(ESceneContentRequestState::Failed));
	}
	catch (...)
	{
		Toolbox::FScopedLock Lock(State->Mutex);
		ContentPrivate::SaveReason(*State, "Unexpected asset preparation exception");
		State->Status.Store(static_cast<Toolbox::uint32>(ESceneContentRequestState::Failed));
	}
}
const FPreparedScene2D& FSceneContentRequest::GetPrepared2D() const
{
	m_pImpl->RequireOwner();
	if (!m_pImpl->m_Last2D.Definition)
	{
		throw Toolbox::FException("No accepted 2D content preparation");
	}
	return m_pImpl->m_Last2D;
}
const FPreparedScene3D& FSceneContentRequest::GetPrepared3D() const
{
	m_pImpl->RequireOwner();
	if (!m_pImpl->m_Last3D.Definition)
	{
		throw Toolbox::FException("No accepted 3D content preparation");
	}
	return m_pImpl->m_Last3D;
}
Toolbox::uint64 FSceneContentRequest::GetAcceptedSequence() const
{
	m_pImpl->RequireOwner();
	return m_pImpl->m_AcceptedSequence;
}
} // namespace Dxf
