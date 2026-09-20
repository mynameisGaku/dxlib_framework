// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Thread.h"
#include "Toolbox/ConditionVariable.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifdef Yield
#undef Yield
#endif
#else
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <string.h>
#endif
namespace Toolbox
{
struct FThread::FImpl
{
#if defined(_WIN32)
	HANDLE Handle = nullptr;
	static DWORD WINAPI EntryPoint_Internal(void* Context)
	{
		FImpl* Impl = static_cast<FImpl*>(Context);
		try
		{
			Impl->Entry(Impl->Context);
		}
		catch (...)
		{
			// OSスレッド境界からC++例外を外へ伝播させない。
		}
		return 0;
	}
#else
	pthread_t Handle{};
	static void* EntryPoint_Internal(void* Context)
	{
		FImpl* Impl = static_cast<FImpl*>(Context);
		try
		{
			Impl->Entry(Impl->Context);
		}
		catch (...)
		{
			// OSスレッド境界からC++例外を外へ伝播させない。
		}
		return nullptr;
	}
#endif
	FThreadEntry Entry = nullptr;
	void* Context = nullptr;
	bool bJoinable = false;
};
struct FMutex::FImpl
{
#if defined(_WIN32)
	SRWLOCK Mutex = SRWLOCK_INIT;
#else
	pthread_mutex_t Mutex{};
#endif
};
struct FConditionVariable::FImpl
{
#if defined(_WIN32)
	CONDITION_VARIABLE Condition = CONDITION_VARIABLE_INIT;
#else
	pthread_cond_t Condition{};
#endif
};
FThread::~FThread()
{
	Join();
	delete m_pImpl;
	m_pImpl = nullptr;
}
FThread::FThread(FThread&& Other) noexcept : m_pImpl(Other.m_pImpl)
{
	Other.m_pImpl = nullptr;
}
FThread& FThread::operator=(FThread&& Other) noexcept
{
	if (this != &Other)
	{
		Join();
		delete m_pImpl;
		m_pImpl = Other.m_pImpl;
		Other.m_pImpl = nullptr;
	}
	return *this;
}
bool FThread::Start(FThreadEntry Entry, void* Context)
{
	if (Entry == nullptr || IsJoinable())
	{
		return false;
	}
	if (m_pImpl == nullptr)
	{
		m_pImpl = new FImpl();
	}
	m_pImpl->Entry = Entry;
	m_pImpl->Context = Context;
#if defined(_WIN32)
	m_pImpl->Handle = CreateThread(nullptr, 0, &FImpl::EntryPoint_Internal, m_pImpl, 0, nullptr);
	if (m_pImpl->Handle == nullptr)
	{
		m_pImpl->Entry = nullptr;
		m_pImpl->Context = nullptr;
		return false;
	}
#else
	if (pthread_create(&m_pImpl->Handle, nullptr, &FImpl::EntryPoint_Internal, m_pImpl) != 0)
	{
		m_pImpl->Entry = nullptr;
		m_pImpl->Context = nullptr;
		return false;
	}
#endif
	m_pImpl->bJoinable = true;
	return true;
}
bool FThread::IsJoinable() const noexcept
{
	return m_pImpl != nullptr && m_pImpl->bJoinable;
}
void FThread::Join() noexcept
{
	if (!IsJoinable())
	{
		return;
	}
#if defined(_WIN32)
	WaitForSingleObject(m_pImpl->Handle, INFINITE);
	CloseHandle(m_pImpl->Handle);
	m_pImpl->Handle = nullptr;
#else
	pthread_join(m_pImpl->Handle, nullptr);
#endif
	m_pImpl->bJoinable = false;
	m_pImpl->Entry = nullptr;
	m_pImpl->Context = nullptr;
}
uint64 FThread::CurrentThreadId() noexcept
{
#if defined(_WIN32)
	return static_cast<uint64>(GetCurrentThreadId());
#else
	static_assert(sizeof(pthread_t) <= sizeof(uint64), "pthread_t does not fit in uint64");
	pthread_t Current = pthread_self();
	uint64 Result = 0;
	memcpy(&Result, &Current, sizeof(Current));
	return Result;
#endif
}
void FThread::Yield() noexcept
{
#if defined(_WIN32)
	SwitchToThread();
#else
	sched_yield();
#endif
}
uint32 FThread::HardwareThreadCount() noexcept
{
#if defined(_WIN32)
	const DWORD Count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
	return Count > 0 ? static_cast<uint32>(Count) : 1;
#else
	const long Count = sysconf(_SC_NPROCESSORS_ONLN);
	return Count > 0 ? static_cast<uint32>(Count) : 1;
#endif
}
FMutex::FMutex() : m_pImpl(new FImpl())
{
#if !defined(_WIN32)
	if (pthread_mutex_init(&m_pImpl->Mutex, nullptr) != 0)
	{
		delete m_pImpl;
		m_pImpl = nullptr;
		throw FException("pthread_mutex_init failed");
	}
#endif
}
FMutex::~FMutex()
{
	if (m_pImpl == nullptr)
	{
		return;
	}
#if !defined(_WIN32)
	pthread_mutex_destroy(&m_pImpl->Mutex);
#endif
	delete m_pImpl;
	m_pImpl = nullptr;
}
void FMutex::Lock() noexcept
{
#if defined(_WIN32)
	AcquireSRWLockExclusive(&m_pImpl->Mutex);
#else
	pthread_mutex_lock(&m_pImpl->Mutex);
#endif
}
bool FMutex::TryLock() noexcept
{
#if defined(_WIN32)
	return TryAcquireSRWLockExclusive(&m_pImpl->Mutex) != FALSE;
#else
	return pthread_mutex_trylock(&m_pImpl->Mutex) == 0;
#endif
}
void FMutex::Unlock() noexcept
{
#if defined(_WIN32)
	ReleaseSRWLockExclusive(&m_pImpl->Mutex);
#else
	pthread_mutex_unlock(&m_pImpl->Mutex);
#endif
}
FConditionVariable::FConditionVariable() : m_pImpl(new FImpl())
{
#if !defined(_WIN32)
	if (pthread_cond_init(&m_pImpl->Condition, nullptr) != 0)
	{
		delete m_pImpl;
		m_pImpl = nullptr;
		throw FException("pthread_cond_init failed");
	}
#endif
}
FConditionVariable::~FConditionVariable()
{
	if (m_pImpl == nullptr)
	{
		return;
	}
#if !defined(_WIN32)
	pthread_cond_destroy(&m_pImpl->Condition);
#endif
	delete m_pImpl;
	m_pImpl = nullptr;
}
void FConditionVariable::Wait(FMutex& Mutex) noexcept
{
#if defined(_WIN32)
	SleepConditionVariableSRW(&m_pImpl->Condition, &Mutex.m_pImpl->Mutex, INFINITE, 0);
#else
	pthread_cond_wait(&m_pImpl->Condition, &Mutex.m_pImpl->Mutex);
#endif
}
void FConditionVariable::NotifyOne() noexcept
{
#if defined(_WIN32)
	WakeConditionVariable(&m_pImpl->Condition);
#else
	pthread_cond_signal(&m_pImpl->Condition);
#endif
}
void FConditionVariable::NotifyAll() noexcept
{
#if defined(_WIN32)
	WakeAllConditionVariable(&m_pImpl->Condition);
#else
	pthread_cond_broadcast(&m_pImpl->Condition);
#endif
}
} // namespace Toolbox
