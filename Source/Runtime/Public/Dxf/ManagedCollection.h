#pragma once
#include "Dxf/ManagedLifecycle.h"
#include "Dxf/ManagedUpdater.h"
#include "Dxf/ManagedDrawDispatcher.h"
#include "Dxf/GuardValue.h"
namespace Dxf
{
/** Composition facade. It does not implement storage, update or lifecycle rules itself. */
template <typename T>
class TManagedCollection : public ILifecycleGroup
{
public:
	TManagedCollection() : m_Lifecycle(m_Storage), m_Updater(m_Storage), m_DrawDispatcher(m_Storage)
	{
	}
	~TManagedCollection() override
	{
		Shutdown_Internal();
	}
	TManagedCollection(const TManagedCollection&) = delete;
	TManagedCollection& operator=(const TManagedCollection&) = delete;
	template <typename U, typename... TArgs>
	TResult<TObjectHandle<U>> Spawn(TArgs&&... Args)
	{
		static_assert(std::is_base_of_v<T, U>);
		if (!m_bAccepting || m_bShutdownRequested)
		{
			return TResult<TObjectHandle<U>>::Failure(EErrorCode::InvalidState, "Collection stopped");
		}
		try
		{
			auto Object = std::make_unique<U>(std::forward<TArgs>(Args)...);
			Object->SetCreationOrder_Internal(m_NextCreationOrder++);
			PrepareObject_Internal(*Object);
			return TResult<TObjectHandle<U>>::Success(m_Storage.Insert(std::move(Object)).template Cast<U>());
		}
		catch (const std::exception& Error)
		{
			return TResult<TObjectHandle<U>>::Failure(EErrorCode::UserException, Error.what());
		}
		catch (...)
		{
			return TResult<TObjectHandle<U>>::Failure(EErrorCode::UserException, "Unknown constructor exception");
		}
	}
	template <typename U> bool Destroy(const TObjectHandle<U>& Handle) noexcept
	{
		T* Object = m_Storage.Find_Internal(Handle.GetId());
		if (!Object || Object->IsDestroyRequested())
		{
			return false;
		}
		Object->RequestDestroy_Internal();
		return true;
	}
	std::size_t Size() const noexcept
	{
		return m_Storage.Size();
	}
	void FreezeBoundary_Internal() override
	{
		if (!m_bBusy && m_bAccepting)
		{
			m_Lifecycle.FreezeBoundary_Internal();
		}
	}
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
		TResult<void> Result;
		{
			TGuardValue Guard(m_bBusy, true);
			Result = m_Lifecycle.CommitBoundary_Internal(Context);
		}
		FinishDispatch_Internal();
		return Result;
	}
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
		TResult<void> Result;
		{
			TGuardValue Guard(m_bBusy, true);
			Result = m_Updater.Tick_Internal(Context);
		}
		FinishDispatch_Internal();
		return Result;
	}
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
		TResult<void> Result;
		{
			TGuardValue Guard(m_bBusy, true);
			Result = m_DrawDispatcher.Draw_Internal(Context);
		}
		FinishDispatch_Internal();
		return Result;
	}
	void Shutdown_Internal() noexcept override
	{
		if (m_bBusy)
		{
			m_bShutdownRequested = true;
			// Suppress the remainder of the current dispatch without deleting its receiver.
			m_Storage.ForEach_Internal([](T& Object)
			{
				Object.RequestDestroy_Internal();
			});
			return;
		}
		if (!m_bAccepting)
		{
			return;
		}
		m_bAccepting = false;
		m_bShutdownRequested = false;
		TGuardValue Guard(m_bBusy, true);
		m_Lifecycle.Shutdown_Internal();
	}
protected:
	virtual void PrepareObject_Internal(T&)
	{
	}
private:
	static TResult<void> BusyError_Internal()
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Reentrant collection dispatch");
	}
	void FinishDispatch_Internal() noexcept
	{
		if (m_bShutdownRequested)
		{
			Shutdown_Internal();
		}
	}
	TSlotMap<T> m_Storage;
	TManagedLifecycle<T> m_Lifecycle;
	TManagedUpdater<T> m_Updater;
	TManagedDrawDispatcher<T> m_DrawDispatcher;
	std::uint64_t m_NextCreationOrder = 1;
	bool m_bAccepting = true;
	bool m_bBusy = false;
	bool m_bShutdownRequested = false;
};
}
