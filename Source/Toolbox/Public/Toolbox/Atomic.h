// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_ATOMIC_H
#define TOOLBOX_ATOMIC_H
#include "Toolbox/Utility.h"
#if defined(_MSC_VER)
#include <intrin.h>
#endif
namespace Toolbox
{
/**
 * 32または64ビット整数を原子的に保持する。
 * 現段階ではint32・uint32・int64・uint64だけを対象とする。
 */
template <typename T> class TAtomic
{
	static_assert(IsSame<T, int32> || IsSame<T, uint32> || IsSame<T, int64> || IsSame<T, uint64>,
				  "TAtomic supports 32-bit and 64-bit integer types");

public:
	/**
	 * 指定した初期値を保持する。
	 * @param Value 初期値。
	 */
	explicit TAtomic(T Value = T{}) noexcept : m_Value(Value)
	{
	}
	/**
	 * 原子状態の意図しない複製を禁止する。
	 */
	TAtomic(const TAtomic&) = delete;
	/**
	 * 原子状態の意図しない複製を禁止する。
	 */
	TAtomic& operator=(const TAtomic&) = delete;
	/**
	 * 現在値をAcquireで読み取る。
	 */
	FORCEINLINE T Load() const noexcept
	{
#if defined(_MSC_VER)
		if constexpr (sizeof(T) == 8)
		{
			return static_cast<T>(_InterlockedCompareExchange64(
				reinterpret_cast<volatile long long*>(&m_Value), 0, 0));
		}
		else
		{
			return static_cast<T>(_InterlockedCompareExchange(
				reinterpret_cast<volatile long*>(&m_Value), 0, 0));
		}
#else
		return __atomic_load_n(&m_Value, __ATOMIC_ACQUIRE);
#endif
	}
	/**
	 * 値をReleaseで保存する。
	 * @param Value 新しい値。
	 */
	FORCEINLINE void Store(T Value) noexcept
	{
#if defined(_MSC_VER)
		if constexpr (sizeof(T) == 8)
		{
			_InterlockedExchange64(reinterpret_cast<volatile long long*>(&m_Value), static_cast<long long>(Value));
		}
		else
		{
			_InterlockedExchange(reinterpret_cast<volatile long*>(&m_Value), static_cast<long>(Value));
		}
#else
		__atomic_store_n(&m_Value, Value, __ATOMIC_RELEASE);
#endif
	}
	/**
	 * 値を不可分に置き換え、変更前の値を返す。
	 * @param Value 新しい値。
	 */
	FORCEINLINE T Exchange(T Value) noexcept
	{
#if defined(_MSC_VER)
		if constexpr (sizeof(T) == 8)
		{
			return static_cast<T>(_InterlockedExchange64(
				reinterpret_cast<volatile long long*>(&m_Value), static_cast<long long>(Value)));
		}
		else
		{
			return static_cast<T>(_InterlockedExchange(
				reinterpret_cast<volatile long*>(&m_Value), static_cast<long>(Value)));
		}
#else
		return __atomic_exchange_n(&m_Value, Value, __ATOMIC_ACQ_REL);
#endif
	}
	/**
	 * 期待値と一致した場合だけ更新する。不一致ならExpectedへ実値を返す。
	 * @param Expected 比較する期待値。
	 * @param Desired 一致時に設定する値。
	 */
	FORCEINLINE bool CompareExchange(T& Expected, T Desired) noexcept
	{
#if defined(_MSC_VER)
		T Old;
		if constexpr (sizeof(T) == 8)
		{
			Old = static_cast<T>(_InterlockedCompareExchange64(
				reinterpret_cast<volatile long long*>(&m_Value), static_cast<long long>(Desired),
				static_cast<long long>(Expected)));
		}
		else
		{
			Old = static_cast<T>(_InterlockedCompareExchange(
				reinterpret_cast<volatile long*>(&m_Value), static_cast<long>(Desired), static_cast<long>(Expected)));
		}
		if (Old == Expected)
		{
			return true;
		}
		Expected = Old;
		return false;
#else
		return __atomic_compare_exchange_n(&m_Value, &Expected, Desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
	}
	/**
	 * 値を加算し、加算前の値を返す。
	 * @param Amount 加算量。
	 */
	FORCEINLINE T FetchAdd(T Amount) noexcept
	{
#if defined(_MSC_VER)
		if constexpr (sizeof(T) == 8)
		{
			return static_cast<T>(_InterlockedExchangeAdd64(
				reinterpret_cast<volatile long long*>(&m_Value), static_cast<long long>(Amount)));
		}
		else
		{
			return static_cast<T>(_InterlockedExchangeAdd(
				reinterpret_cast<volatile long*>(&m_Value), static_cast<long>(Amount)));
		}
#else
		return __atomic_fetch_add(&m_Value, Amount, __ATOMIC_ACQ_REL);
#endif
	}
	/**
	 * 値を減算し、減算前の値を返す。
	 * @param Amount 減算量。
	 */
	FORCEINLINE T FetchSub(T Amount) noexcept
	{
#if defined(_MSC_VER)
		return FetchAdd(static_cast<T>(T{} - Amount));
#else
		return __atomic_fetch_sub(&m_Value, Amount, __ATOMIC_ACQ_REL);
#endif
	}

private:
	/**
	 * コンパイラまたはOSの原子命令から操作する整数。
	 */
	alignas(sizeof(T)) mutable volatile T m_Value;
};

/**
 * 64ビットの原子カウンター。既存APIとの互換性を保つ。
 */
class FAtomicCounter
{
public:
	/**
	 * 指定した値を初期状態として保持する。
	 * @param Value 初期値。
	 */
	explicit FAtomicCounter(uint64 Value = 0) noexcept : m_Value(Value)
	{
	}
	/**
	 * 原子状態の複製を禁止する。
	 */
	FAtomicCounter(const FAtomicCounter&) = delete;
	/**
	 * 原子状態のコピー代入を禁止する。
	 */
	FAtomicCounter& operator=(const FAtomicCounter&) = delete;
	/**
	 * 値を加算し、加算前の値を返す。
	 * @param Amount 加算量。
	 */
	FORCEINLINE uint64 FetchAdd(uint64 Amount) noexcept
	{
		return m_Value.FetchAdd(Amount);
	}
	/**
	 * 値を減算し、減算前の値を返す。
	 * @param Amount 減算量。
	 */
	FORCEINLINE uint64 FetchSub(uint64 Amount) noexcept
	{
		return m_Value.FetchSub(Amount);
	}
	/**
	 * 現在値をAcquireで読み取る。
	 */
	FORCEINLINE uint64 Load() const noexcept
	{
		return m_Value.Load();
	}
	/**
	 * 新しい値をReleaseで保存する。
	 * @param Value 新しい値。
	 */
	FORCEINLINE void Store(uint64 Value) noexcept
	{
		m_Value.Store(Value);
	}
	/**
	 * 期待値と一致した場合だけ更新する。不一致ならExpectedへ実値を返す。
	 * @param Expected 比較する期待値。
	 * @param Desired 一致時に設定する値。
	 */
	FORCEINLINE bool CompareExchange(uint64& Expected, uint64 Desired) noexcept
	{
		return m_Value.CompareExchange(Expected, Desired);
	}

private:
	/**
	 * 既存カウンターAPIの実体となる64ビット原子値。
	 */
	TAtomic<uint64> m_Value;
};
} // namespace Toolbox
#endif
