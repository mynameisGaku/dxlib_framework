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
 * 64ビットの原子カウンター。参照カウントと一意IDの発行に使用する。
 */
class FAtomicCounter
{
public:
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Value 処理または保持する値。
	 */
	explicit FAtomicCounter(uint64 Value = 0) noexcept : m_Value(static_cast<long long>(Value))
	{
	}
	/**
	 * 意図しない所有権や状態の複製を禁止する。
	 */
	FAtomicCounter(const FAtomicCounter&) = delete;
	/**
	 * 意図しない所有権や状態の複製を禁止する。
	 */
	FAtomicCounter& operator=(const FAtomicCounter&) = delete;
	/**
	 * 加算前の値を返し、加算を他のスレッドから見て不可分に行う。
	 * @param Amount カウンターへ加算する量。
	 */
	uint64 FetchAdd(uint64 Amount) noexcept
	{
#if defined(_MSC_VER)
		return static_cast<uint64>(_InterlockedExchangeAdd64(&m_Value, static_cast<long long>(Amount)));
#else
		return static_cast<uint64>(__atomic_fetch_add(&m_Value, static_cast<long long>(Amount), __ATOMIC_ACQ_REL));
#endif
	}
	/**
	 * 現在のカウンター値を原子的に読み取る。
	 */
	uint64 Load() const noexcept
	{
#if defined(_MSC_VER)
		return static_cast<uint64>(_InterlockedCompareExchange64(&m_Value, 0, 0));
#else
		return static_cast<uint64>(__atomic_load_n(&m_Value, __ATOMIC_ACQUIRE));
#endif
	}
	/**
	 * 期待値と一致した場合だけ更新する。不一致なら現在値をExpectedへ返す。
	 * @param Expected 比較する現在値。不一致なら実際の値に置き換わる。
	 * @param Desired 比較一致時に設定する値。
	 */
	bool CompareExchange(uint64& Expected, uint64 Desired) noexcept
	{
#if defined(_MSC_VER)
		// 比較交換を実行した時点でのカウンター値。
		const auto Old = static_cast<uint64>(
		    _InterlockedCompareExchange64(&m_Value, static_cast<long long>(Desired), static_cast<long long>(Expected)));
		if (Old == Expected)
		{
			return true;
		}
		Expected = Old;
		return false;
#else
		// 比較の期待値。不一致なら実際の値を受け取る。
		long long Current = static_cast<long long>(Expected);
		// 比較交換が成功したかどうか。
		const bool Success = __atomic_compare_exchange_n(&m_Value, &Current, static_cast<long long>(Desired), false,
		                                                 __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
		Expected = static_cast<uint64>(Current);
		return Success;
#endif
	}

private:
	/**
	 * OSの原子命令で操作する、8バイト境界に整列したカウンター。
	 */
	alignas(8) mutable volatile long long m_Value;
};
} // namespace Toolbox
#endif
