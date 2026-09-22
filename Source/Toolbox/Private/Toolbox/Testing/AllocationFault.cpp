// SPDX-License-Identifier: NOASSERTION
#include "AllocationFault.h"
#include "Toolbox/Atomic.h"
#include <stdlib.h>
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace
{
thread_local Toolbox::int64 Countdown = -1;
thread_local bool bInjected = false;
Toolbox::FAtomicCounter Outstanding;
// 一回だけ失敗し、エラー結果の構築は通常の確保に戻す。
void BeforeAllocate_Internal()
{
	if (Countdown == 0)
	{
		Countdown = -1;
		bInjected = true;
		throw Toolbox::FException("expected render plan allocation failure");
	}
	if (Countdown > 0)
	{
		--Countdown;
	}
}
void* Allocate_Internal(Toolbox::size_t Size)
{
	BeforeAllocate_Internal();
	void* Memory = malloc(Size == 0 ? 1 : Size);
	if (Memory == nullptr)
	{
		throw Toolbox::FException("test allocator exhausted");
	}
	Outstanding.FetchAdd(1);
	return Memory;
}
void* AllocateAligned_Internal(Toolbox::size_t Size, Toolbox::size_t Alignment)
{
	BeforeAllocate_Internal();
	// posix_memalignの要求に合わせ、小さい言語上のアラインメントも扱う。
	Alignment = Alignment < sizeof(void*) ? sizeof(void*) : Alignment;
	void* Memory = nullptr;
#if defined(_WIN32)
	Memory = _aligned_malloc(Size == 0 ? 1 : Size, Alignment);
#else
	if (posix_memalign(&Memory, Alignment, Size == 0 ? 1 : Size) != 0)
	{
		Memory = nullptr;
	}
#endif
	if (Memory == nullptr)
	{
		throw Toolbox::FException("test aligned allocator exhausted");
	}
	Outstanding.FetchAdd(1);
	return Memory;
}
void Free_Internal(void* Memory) noexcept
{
	if (Memory != nullptr)
	{
		Outstanding.FetchSub(1);
		free(Memory);
	}
}
void FreeAligned_Internal(void* Memory) noexcept
{
	if (Memory != nullptr)
	{
		Outstanding.FetchSub(1);
#if defined(_WIN32)
		_aligned_free(Memory);
#else
		free(Memory);
#endif
	}
}
}
void* operator new(Toolbox::size_t Size)
{
	return Allocate_Internal(Size);
}
void* operator new[](Toolbox::size_t Size)
{
	return Allocate_Internal(Size);
}
void operator delete(void* Memory) noexcept
{
	Free_Internal(Memory);
}
void operator delete[](void* Memory) noexcept
{
	Free_Internal(Memory);
}
void operator delete(void* Memory, Toolbox::size_t) noexcept
{
	Free_Internal(Memory);
}
void operator delete[](void* Memory, Toolbox::size_t) noexcept
{
	Free_Internal(Memory);
}
void* operator new(Toolbox::size_t Size, std::align_val_t Alignment)
{
	return AllocateAligned_Internal(Size, static_cast<Toolbox::size_t>(Alignment));
}
void* operator new[](Toolbox::size_t Size, std::align_val_t Alignment)
{
	return AllocateAligned_Internal(Size, static_cast<Toolbox::size_t>(Alignment));
}
void operator delete(void* Memory, std::align_val_t) noexcept
{
	FreeAligned_Internal(Memory);
}
void operator delete[](void* Memory, std::align_val_t) noexcept
{
	FreeAligned_Internal(Memory);
}
void operator delete(void* Memory, Toolbox::size_t, std::align_val_t) noexcept
{
	FreeAligned_Internal(Memory);
}
void operator delete[](void* Memory, Toolbox::size_t, std::align_val_t) noexcept
{
	FreeAligned_Internal(Memory);
}
namespace Toolbox::Testing
{
void SetAllocationFailureCountdown(int64 Value) noexcept
{
	Countdown = Value;
	bInjected = false;
}
bool WasAllocationFailureInjected() noexcept
{
	return bInjected;
}
uint64 GetOutstandingTestAllocations() noexcept
{
	return Outstanding.Load();
}
}
