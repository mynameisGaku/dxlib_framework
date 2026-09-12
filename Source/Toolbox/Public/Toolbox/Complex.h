// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_COMPLEX_H
#define TOOLBOX_COMPLEX_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 実部と虚部を持つ複素数。f32とf64の両精度を使用できる。
 */
template <typename T> struct TComplex
{
	static_assert(IsSame<T, f32> || IsSame<T, f64>);
	/**
	 * 実数成分。
	 */
	T Real = 0;
	/**
	 * 虚数成分。
	 */
	T Imaginary = 0;
	/**
	 * 実部と虚部をそれぞれ加算する。
	 * @param Other 演算または比較の相手。
	 */
	FORCEINLINE TComplex operator+(TComplex Other) const noexcept
	{
		return {Real + Other.Real, Imaginary + Other.Imaginary};
	}
	/**
	 * 実部と虚部をそれぞれ減算する。
	 * @param Other 演算または比較の相手。
	 */
	FORCEINLINE TComplex operator-(TComplex Other) const noexcept
	{
		return {Real - Other.Real, Imaginary - Other.Imaginary};
	}
	/**
	 * iの二乗を-1として積を求める。
	 * @param Other 演算または比較の相手。
	 */
	FORCEINLINE TComplex operator*(TComplex Other) const noexcept
	{
		return {Real * Other.Real - Imaginary * Other.Imaginary, Real * Other.Imaginary + Imaginary * Other.Real};
	}
	/**
	 * スケーリングを使って除算する。ゼロ・非有限の除数は例外で通知する。
	 * @param Other 演算または比較の相手。
	 */
	TComplex operator/(TComplex Other) const
	{
		if (!IsFinite(Other.Real) || !IsFinite(Other.Imaginary) || (Other.Real == 0 && Other.Imaginary == 0))
		{
			throw FException("Invalid complex divisor");
		}
		if (Abs(Other.Real) >= Abs(Other.Imaginary))
		{
			// 複素除算の成分比。
			const T Ratio = Other.Imaginary / Other.Real;
			// 行を正規化する除数。
			const T Divisor = T(1) + Ratio * Ratio;
			return {(Real / Other.Real + (Imaginary / Other.Real) * Ratio) / Divisor,
			        (Imaginary / Other.Real - (Real / Other.Real) * Ratio) / Divisor};
		}
		// 複素除算の成分比。
		const T Ratio = Other.Real / Other.Imaginary;
		// 行を正規化する除数。
		const T Divisor = T(1) + Ratio * Ratio;
		return {((Real / Other.Imaginary) * Ratio + Imaginary / Other.Imaginary) / Divisor,
		        ((Imaginary / Other.Imaginary) * Ratio - Real / Other.Imaginary) / Divisor};
	}
	/**
	 * 虚部の符号を反転する。
	 */
	FORCEINLINE TComplex Conjugate() const noexcept
	{
		return {Real, -Imaginary};
	}
	/**
	 * 原点からの距離を返す。
	 */
	T Magnitude() const noexcept
	{
		return static_cast<T>(hypot(Real, Imaginary));
	}
	/**
	 * 偏角をラジアンで返す。
	 */
	T Phase() const noexcept
	{
		return static_cast<T>(atan2(Imaginary, Real));
	}
	/**
	 * 長さとラジアン角から複素数を作る。
	 * @param Magnitude 原点からの距離。
	 * @param Radians ラジアン単位の回転角。
	 */
	static TComplex FromPolar(T Magnitude, T Radians) noexcept
	{
		return {Magnitude * static_cast<T>(Cos(Radians)), Magnitude * static_cast<T>(Sin(Radians))};
	}
	/**
	 * 実部と虚部が完全に一致するか調べる。
	 */
	bool operator==(const TComplex&) const = default;
};
/**
 * 単精度の複素数。
 */
using FComplex32 = TComplex<f32>;
/**
 * 倍精度の複素数。
 */
using FComplex64 = TComplex<f64>;
/**
 * 単精度複素数の配列をSIMDで乗算する。出力は入力と同じ配列でもよい。
 * 部分的に重なる配列は使用しない。Countが0ならnullptrを渡せる。
 * @param A 左側の入力値。
 * @param B 右側の入力値。
 * @param Output 結果を書き込む先。
 * @param Count 処理する要素の個数。
 */
void MultiplyComplex(const FComplex32* A, const FComplex32* B, FComplex32* Output, size_t Count);
/**
 * 倍精度複素数の配列をSIMDで乗算する。出力は入力と同じ配列でもよい。
 * @param A 左側の入力値。
 * @param B 右側の入力値。
 * @param Output 結果を書き込む先。
 * @param Count 処理する要素の個数。
 */
void MultiplyComplex(const FComplex64* A, const FComplex64* B, FComplex64* Output, size_t Count);
} // namespace Toolbox
#endif
