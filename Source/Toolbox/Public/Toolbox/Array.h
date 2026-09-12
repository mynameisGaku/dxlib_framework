// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_ARRAY_H
#define TOOLBOX_ARRAY_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 要素数がコンパイル時に決まる、動的確保しない配列。
 */
template <typename T, size_t N> struct TArray
{
	/**
	 * 保持する固定数の要素。
	 */
	T Values[N ? N : 1];
	/**
	 * 現在保持している要素数を返す。
	 */
	constexpr size_t Size() const noexcept
	{
		return N;
	}
	/**
	 * 固定配列の先頭ポインターを返す。配列自身の寿命中だけ有効。
	 */
	constexpr T* Data() noexcept
	{
		return Values;
	}
	/**
	 * 固定配列の先頭ポインターを返す。配列自身の寿命中だけ有効。
	 */
	constexpr const T* Data() const noexcept
	{
		return Values;
	}
	/**
	 * 指定位置の要素へアクセスする。Indexは要素数未満とする。
	 * @param Index 参照する要素の位置。
	 */
	constexpr T& operator[](size_t Index) noexcept
	{
		return Values[Index];
	}
	/**
	 * 指定位置の要素へアクセスする。Indexは要素数未満とする。
	 * @param Index 参照する要素の位置。
	 */
	constexpr const T& operator[](size_t Index) const noexcept
	{
		return Values[Index];
	}
	/**
	 * 先頭を指す反復子を返す。空の場合はEndと一致する。
	 */
	constexpr T* Begin() noexcept
	{
		return Values;
	}
	/**
	 * 先頭を指す反復子を返す。空の場合はEndと一致する。
	 */
	constexpr const T* Begin() const noexcept
	{
		return Values;
	}
	/**
	 * 最終要素の次を指す反復子を返す。この位置は参照しない。
	 */
	constexpr T* End() noexcept
	{
		return Values + N;
	}
	/**
	 * 最終要素の次を指す反復子を返す。この位置は参照しない。
	 */
	constexpr const T* End() const noexcept
	{
		return Values + N;
	}
	/**
	 * range-forが必要とする先頭反復子を返す。
	 */
	constexpr T* begin() noexcept
	{
		return Begin();
	}
	/**
	 * range-forが必要とする先頭反復子を返す。
	 */
	constexpr const T* begin() const noexcept
	{
		return Begin();
	}
	/**
	 * range-forが必要とする終端反復子を返す。
	 */
	constexpr T* end() noexcept
	{
		return End();
	}
	/**
	 * range-forが必要とする終端反復子を返す。
	 */
	constexpr const T* end() const noexcept
	{
		return End();
	}
	/**
	 * すべての要素へ同じ値を設定する。
	 * @param Value 処理または保持する値。
	 */
	constexpr void Fill(const T& Value)
	{
		/**
		 * 現在の要素位置を進めて範囲を走査する。
		 */
		for (size_t I = 0; I < N; ++I)
		{
			Values[I] = Value;
		}
	}
	/**
	 * 保持する値または対象が一致するか比較する。
	 */
	bool operator==(const TArray& Other) const
	{
		/**
		 * 現在の要素位置を進めて範囲を走査する。
		 */
		for (size_t I = 0; I < N; ++I)
		{
			if (!(Values[I] == Other.Values[I]))
			{
				return false;
			}
		}
		return true;
	}
};
/**
 * 初期値の数と先頭要素の型から固定配列の型を推論する。
 */
template <typename T, typename... U> TArray(T, U...) -> TArray<T, 1 + sizeof...(U)>;
} // namespace Toolbox
#endif
