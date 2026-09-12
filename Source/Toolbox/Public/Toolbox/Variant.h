// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_VARIANT_H
#define TOOLBOX_VARIANT_H
#include "Toolbox/Optional.h"
namespace Toolbox
{
namespace Detail
{
/**
 * 型リスト内の指定番号に対応する型を求める。
 */
template <size_t I, typename T, typename... Rest> struct TVariantType : TVariantType<I - 1, Rest...>
{
};
/**
 * 型リスト内の指定番号に対応する型を求める。
 */
template <typename T, typename... Rest> struct TVariantType<0, T, Rest...>
{
	using Type = T;
};
/**
 * 型に一致する最初の選択番号を求める。重複型の選択には番号タグを使う。
 */
template <typename T, typename First, typename... Rest> constexpr size_t VariantIndex()
{
	if constexpr (IsSame<T, First>)
	{
		return 0;
	}
	else
	{
		static_assert(sizeof...(Rest) > 0, "Type is not a variant alternative");
		return 1 + VariantIndex<T, Rest...>();
	}
}
/**
 * 全選択肢に必要な最大のサイズまたはアラインメントを求める。
 */
template <size_t A, size_t... Rest> constexpr size_t Maximum()
{
	if constexpr (sizeof...(Rest) == 0)
	{
		return A;
	}
	else
	{
		// 残りの選択肢に必要な最大値。
		constexpr size_t B = Maximum<Rest...>();
		return A > B ? A : B;
	}
}
} // namespace Detail
/**
 * 指定した型のうち一つを直接保持する。型が違うGetは例外で通知する。
 */
template <typename... Types> class TVariant
{
	/**
	 * 選択番号から対応する保持型を取り出す。
	 */
	template <size_t I> using TAt = typename Detail::TVariantType<I, Types...>::Type;

public:
	/**
	 * 指定した値を使って初期状態を構築する。
	 */
	TVariant()
	{
		Construct_Internal<0>();
	}
	/**
	 * 番号で指定した選択肢を構築する。同じ型が複数存在しても区別できる。
	 * @param Values 選択した型の構築引数。
	 */
	template <size_t I, typename... Args> explicit TVariant(TInPlaceIndex<I>, Args&&... Values)
	{
		Construct_Internal<I>(Forward<Args>(Values)...);
	}
	/**
	 * 元の値から所有領域または状態を移す。
	 * @param Value 処理または保持する値。
	 */
	template <typename T>
	    requires(!IsSame<TDecay<T>, TVariant>)
	TVariant(T&& Value)
	{
		Construct_Internal<Detail::VariantIndex<TDecay<T>, Types...>()>(Forward<T>(Value));
	}
	/**
	 * 保持する値をコピーする。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	TVariant(const TVariant& Other)
	    requires(IsConstructible<Types, const Types&> && ...)
	{
		CopyFrom_Internal<0>(Other);
	}
	/**
	 * 元の値から所有領域または状態を移す。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	TVariant(TVariant&& Other) noexcept((__is_nothrow_constructible(Types, Types&&) && ...))
	{
		MoveFrom_Internal<0>(Other);
	}
	/**
	 * 保持している値と所有領域を破棄する。
	 */
	~TVariant()
	{
		Destroy_Internal();
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TVariant& operator=(TVariant&& Other) noexcept((__is_nothrow_constructible(Types, Types&&) && ...))
	{
		if (this != &Other)
		{
			Destroy_Internal();
			MoveFrom_Internal<0>(Other);
		}
		return *this;
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TVariant& operator=(const TVariant& Other)
	    requires(IsConstructible<Types, const Types&> && ...)
	{
		if (this != &Other)
		{
			// 元の値を保ったまま変更を準備する複製。
			TVariant Copy(Other);
			*this = Move(Copy);
		}
		return *this;
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	template <typename T>
	    requires(!IsSame<TDecay<T>, TVariant>)
	TVariant& operator=(T&& Value)
	{
		// 交換または代入が終わるまで元の値を保持する一時領域。
		TVariant Temporary(Forward<T>(Value));
		*this = Move(Temporary);
		return *this;
	}
	/**
	 * 現在有効な型の番号を返す。
	 */
	FORCEINLINE size_t Index() const noexcept
	{
		return m_Index;
	}
	/**
	 * 保持している対象を返す。値型の選択が違う場合は例外で通知する。
	 */
	template <size_t I> TAt<I>& Get()
	{
		if (m_Index != I)
		{
			throw FBadAccess();
		}
		return *reinterpret_cast<TAt<I>*>(m_Storage);
	}
	/**
	 * 保持している対象を返す。値型の選択が違う場合は例外で通知する。
	 */
	template <size_t I> const TAt<I>& Get() const
	{
		if (m_Index != I)
		{
			throw FBadAccess();
		}
		return *reinterpret_cast<const TAt<I>*>(m_Storage);
	}
	/**
	 * 有効な選択肢をコールバックへ渡す。
	 * @param Function 対象の値を受け取るコールバック。
	 */
	template <typename F> decltype(auto) Visit(F&& Function)
	{
		return Visit_Internal<0>(*this, Function);
	}
	/**
	 * 有効な選択肢をコールバックへ渡す。
	 * @param Function 対象の値を受け取るコールバック。
	 */
	template <typename F> decltype(auto) Visit(F&& Function) const
	{
		return Visit_Internal<0>(*this, Function);
	}

private:
	/**
	 * 選択番号を保ってコピーする。同じ型が複数あっても別の選択肢として扱う。
	 * @param Other
	 * コピー元。値を持たない場合は何も構築しない。
	 */
	template <size_t I> void CopyFrom_Internal(const TVariant& Other)
	{
		if constexpr (I < sizeof...(Types))
		{
			if (Other.m_Index == I)
			{
				Construct_Internal<I>(Other.template Get<I>());
			}
			else
			{
				CopyFrom_Internal<I + 1>(Other);
			}
		}
	}
	/**
	 * 選択番号を保って所有値を移す。型の一致だけで番号を選び直さない。
	 * @param Other
	 * 移動元。値を持たない場合は何も構築しない。
	 */
	template <size_t I> void MoveFrom_Internal(TVariant& Other)
	{
		if constexpr (I < sizeof...(Types))
		{
			if (Other.m_Index == I)
			{
				Construct_Internal<I>(Move(Other.template Get<I>()));
			}
			else
			{
				MoveFrom_Internal<I + 1>(Other);
			}
		}
	}
	/**
	 * ValuesからI番目の型を構築し、有効な型番号を記録する。
	 * @param Values
	 * 対象型のコンストラクターへ転送する引数。
	 */
	template <size_t I, typename... Args> void Construct_Internal(Args&&... Values)
	{
		new (m_Storage) TAt<I>(Forward<Args>(Values)...);
		m_Index = I;
	}
	/**
	 * 有効な型のデストラクターを一度だけ呼ぶ。
	 */
	void Destroy_Internal() noexcept
	{
		if (m_Index < sizeof...(Types))
		{
			Visit(
			    [](auto& Value)
			    {
				    using T = TDecay<decltype(Value)>;
				    Value.~T();
			    });
			m_Index = sizeof...(Types);
		}
	}
	/**
	 * 有効な型の番号に対応する処理へ分岐する。
	 * @param Value 処理または保持する値。
	 * @param Function 対象の値を受け取るコールバック。
	 */
	template <size_t I, typename Self, typename F> static decltype(auto) Visit_Internal(Self& Value, F& Function)
	{
		if constexpr (I + 1 == sizeof...(Types))
		{
			return Function(Value.template Get<I>());
		}
		else
		{
			if (Value.m_Index == I)
			{
				return Function(Value.template Get<I>());
			}
			return Visit_Internal<I + 1>(Value, Function);
		}
	}
	/**
	 * 対象型のアラインメントを満たす未初期化の所有領域。
	 */
	alignas(Detail::Maximum<alignof(Types)...>()) unsigned char m_Storage[Detail::Maximum<sizeof(Types)...>()];
	/**
	 * 内部領域で現在有効な型の番号。
	 */
	size_t m_Index = sizeof...(Types);
};
/**
 * I番目の型として値を取り出す。異なる型ならFBadAccessを送出する。
 * @param Value 読み出す対象の選択値。

 */
template <size_t I, typename... T> FORCEINLINE decltype(auto) Get(TVariant<T...>& Value)
{
	return Value.template Get<I>();
}
/**
 * I番目の型として変更できない参照を取り出す。
 * @param Value 読み出す対象の選択値。

 */
template <size_t I, typename... T> FORCEINLINE decltype(auto) Get(const TVariant<T...>& Value)
{
	return Value.template Get<I>();
}
/**
 * I番目の型の値を移動可能な参照として取り出す。
 * @param Value 所有値を取り出す選択値。

 */
template <size_t I, typename... T> FORCEINLINE decltype(auto) Get(TVariant<T...>&& Value)
{
	return Move(Value.template Get<I>());
}
/**
 * 型Uを指定して参照を取り出す。異なる型ならFBadAccessを送出する。
 * @param Value 読み出す対象の選択値。

 */
template <typename U, typename... T> FORCEINLINE const U& Get(const TVariant<T...>& Value)
{
	return Value.template Get<Detail::VariantIndex<U, T...>()>();
}
/**
 * 型Uが現在の選択肢であるか調べる。
 * @param Value 型を確認する選択値。
 */
template <typename U, typename... T> bool HoldsAlternative(const TVariant<T...>& Value)
{
	return Value.Index() == Detail::VariantIndex<U, T...>();
}
/**
 * 有効な選択肢をコールバックへ渡す。
 * @param Function 対象の値を受け取るコールバック。
 * @param Value 処理または保持する値。
 */
template <typename F, typename V> decltype(auto) Visit(F&& Function, V&& Value)
{
	return Forward<V>(Value).Visit(Forward<F>(Function));
}
} // namespace Toolbox
#endif
