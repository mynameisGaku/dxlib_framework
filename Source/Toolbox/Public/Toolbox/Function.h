// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_FUNCTION_H
#define TOOLBOX_FUNCTION_H
#include "Toolbox/UniquePtr.h"
namespace Toolbox
{
template <typename Signature> class TFunction;
/**
 * コピー可能なコールバックを所有する。空の呼び出しはFBadAccessを送出する。
 */
template <typename R, typename... Args> class TFunction<R(Args...)>
{
	/**
	 * 具体的な関数型を隠して呼び出しと複製を提供する。
	 */
	struct ICallable
	{
		/**
		 * 具体的な型の捕捉状態まで破棄する。
		 */
		virtual ~ICallable() = default;
		/**
		 * Valuesを保持するコールバックへ転送する。
		 */
		virtual R Invoke(Args... Values) = 0;
		/**
		 * 捕捉状態を独立して複製する。
		 */
		virtual ICallable* Clone() const = 0;
	};
	/**
	 * 一つの具体的なコールバックと捕捉状態を保持する。
	 */
	template <typename F> struct TCallable final : ICallable
	{
		/**
		 * Valueの捕捉状態を所有領域へ移す。
		 */
		explicit TCallable(F Value) : Function(Move(Value))
		{
		}
		/**
		 * 捕捉した状態を使ってValuesに対する処理を実行する。
		 */
		R Invoke(Args... Values) override
		{
			if constexpr (IsSame<R, void>)
			{
				Function(Forward<Args>(Values)...);
			}
			else
			{
				return Function(Forward<Args>(Values)...);
			}
		}
		/**
		 * 同じ捕捉値を持つ別のコールバックを作る。
		 */
		ICallable* Clone() const override
		{
			return new TCallable(Function);
		}
		/**
		 * 呼び出しに必要な捕捉状態を持つ関数オブジェクト。
		 */
		F Function;
	};

public:
	/**
	 * 指定した値を使って初期状態を構築する。
	 */
	TFunction() = default;
	/**
	 * 指定した値を使って初期状態を構築する。
	 */
	TFunction(decltype(nullptr))
	{
	}
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Value 処理または保持する値。
	 */
	template <typename F>
	    requires(!IsSame<TDecay<F>, TFunction>)
	TFunction(F Value)
	{
		if constexpr (requires { Value == nullptr; })
		{
			if (Value == nullptr)
			{
				return;
			}
		}
		m_pCallable.Reset(new TCallable<TDecay<F>>(Move(Value)));
	}
	/**
	 * 保持する値をコピーする。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	TFunction(const TFunction& Other) : m_pCallable(Other.m_pCallable ? Other.m_pCallable->Clone() : nullptr)
	{
	}
	/**
	 * 元の値から所有領域または状態を移す。
	 */
	TFunction(TFunction&&) noexcept = default;
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TFunction& operator=(const TFunction& Other)
	{
		if (this != &Other)
		{
			// 元の値を保ったまま変更を準備する複製。
			TFunction Copy(Other);
			m_pCallable = Move(Copy.m_pCallable);
		}
		return *this;
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TFunction& operator=(TFunction&&) noexcept = default;
	/**
	 * 有効な値または対象があるか調べる。
	 */
	FORCEINLINE explicit operator bool() const noexcept
	{
		return static_cast<bool>(m_pCallable);
	}
	/**
	 * 保持する処理を実行し、結果を返す。
	 */
	R operator()(Args... Values) const
	{
		if (!m_pCallable)
		{
			throw FBadAccess();
		}
		return m_pCallable->Invoke(Forward<Args>(Values)...);
	}

private:
	/**
	 * 捕捉した状態ごと所有するコールバック。
	 */
	TUniquePtr<ICallable> m_pCallable;
};
} // namespace Toolbox
#endif
