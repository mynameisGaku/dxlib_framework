// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_UTILITY_H
#define TOOLBOX_UTILITY_H
#include "Toolbox/Compiler.h"
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <new>
#include <initializer_list>

namespace Toolbox
{
/**
 * メモリ領域や配列の大きさを表すプラットフォーム幅の符号なし整数。
 */
using size_t = ::size_t;
/**
 * 符号付き8ビット整数。
 */
using int8 = ::int8_t;
/**
 * 符号付き16ビット整数。
 */
using int16 = ::int16_t;
/**
 * 符号付き32ビット整数。
 */
using int32 = ::int32_t;
/**
 * 符号付き64ビット整数。
 */
using int64 = ::int64_t;
/**
 * 符号なし16ビット整数。
 */
using uint16 = ::uint16_t;
/**
 * 32ビット単精度浮動小数点。
 */
using f32 = float;
/**
 * 64ビット倍精度浮動小数点。
 */
using f64 = double;
/**
 * 符号なし8ビット整数。
 */
using uint8 = uint8_t;
/**
 * 符号なし32ビット整数。
 */
using uint32 = uint32_t;
/**
 * 符号なし64ビット整数。
 */
using uint64 = uint64_t;
/**
 * コンパイラが生成する初期化リストを受け取る言語サポート型。
 */
template <typename T> using TInitializerList = std::initializer_list<T>;
/**
 * 参照修飾を除いた型を求める。
 */
template <typename T> struct TRemoveReference
{
	/**
	 * 型計算で選ばれた結果の型。
	 */
	using Type = T;
};
/**
 * 参照修飾を除いた型を求める。
 */
template <typename T> struct TRemoveReference<T&>
{
	/**
	 * 型計算で選ばれた結果の型。
	 */
	using Type = T;
};
/**
 * 参照修飾を除いた型を求める。
 */
template <typename T> struct TRemoveReference<T&&>
{
	/**
	 * 型計算で選ばれた結果の型。
	 */
	using Type = T;
};
/**
 * 最上位のconst修飾を除いた型を求める。
 */
template <typename T> struct TRemoveConst
{
	/**
	 * 型計算で選ばれた結果の型。
	 */
	using Type = T;
};
/**
 * 最上位のconst修飾を除いた型を求める。
 */
template <typename T> struct TRemoveConst<const T>
{
	/**
	 * 型計算で選ばれた結果の型。
	 */
	using Type = T;
};
/**
 * 参照と最上位constを除き、値を保持するための型を求める。
 */
template <typename T> using TDecay = typename TRemoveConst<typename TRemoveReference<T>::Type>::Type;
/**
 * 二つの型が異なる場合の判定値。
 */
template <typename A, typename B> inline constexpr bool IsSame = false;
/**
 * 二つの型が同じ場合の判定値。
 */
template <typename A> inline constexpr bool IsSame<A, A> = true;
/**
 * AがBの基底型であるかをコンパイラへ問い合わせる。
 */
template <typename A, typename B> inline constexpr bool IsBaseOf = __is_base_of(A, B);
/**
 * 指定した引数から型を構築できるかを調べる。
 * requires節では組み込み型特性を直接使わず、コンパイラ間で共通の名前を使う。
 */
template <typename T, typename... Args> inline constexpr bool IsConstructible = __is_constructible(T, Args...);

/**
 * コンパイル時の条件によって二つの型から選ぶ。
 */
template <bool Condition, typename A, typename B> struct TSelect
{
	/**
	 * 型計算で選ばれた結果の型。
	 */
	using Type = A;
};
/**
 * コンパイル時の条件によって二つの型から選ぶ。
 */
template <typename A, typename B> struct TSelect<false, A, B>
{
	/**
	 * 型計算で選ばれた結果の型。
	 */
	using Type = B;
};
/**
 * 値を移動可能な参照へ変換する。所有権の移動そのものは行わない。
 * @param Value 処理または保持する値。
 */
template <typename T> FORCEINLINE constexpr typename TRemoveReference<T>::Type&& Move(T&& Value) noexcept
{
	return static_cast<typename TRemoveReference<T>::Type&&>(Value);
}
/**
 * 呼び出し元の参照種別を保って引数を転送する。
 * @param Value 処理または保持する値。
 */
template <typename T> FORCEINLINE constexpr T&& Forward(typename TRemoveReference<T>::Type& Value) noexcept
{
	return static_cast<T&&>(Value);
}
/**
 * 呼び出し元の参照種別を保って引数を転送する。
 * @param Value 処理または保持する値。
 */
template <typename T> FORCEINLINE constexpr T&& Forward(typename TRemoveReference<T>::Type&& Value) noexcept
{
	return static_cast<T&&>(Value);
}
/**
 * 二つの値または所有領域を交換する。
 * @param A 一つ目の値。
 * @param B 二つ目の値。
 */
template <typename T>
void Swap(T& A, T& B) noexcept(__is_nothrow_constructible(T, T&&) && __is_nothrow_assignable(T&, T&&))
{
	// 交換または代入が終わるまで元の値を保持する一時領域。
	// @param A 一つ目の値。
	T Temporary(Move(A));
	A = Move(B);
	B = Move(Temporary);
}
/**
 * 新しい値を設定し、置き換える前の値を返す。
 * @param Value 処理または保持する値。
 * @param Replacement 現在値と置き換える新しい値。
 */
template <typename T, typename U> FORCEINLINE T Exchange(T& Value, U&& Replacement)
{
	// 変更前の値または要素数。
	T Previous = Move(Value);
	Value = Forward<U>(Replacement);
	return Previous;
}
/**
 * 比較して小さい方の値を返す。
 * @param A 一つ目の値。
 * @param B 二つ目の値。
 */
template <typename T> FORCEINLINE constexpr T Min(T A, T B)
{
	return B < A ? B : A;
}
/**
 * 比較して大きい方の値を返す。
 * @param A 一つ目の比較値。
 * @param B 二つ目の比較値。
 */
template <typename T> FORCEINLINE constexpr T Max(T A, T B)
{
	return A < B ? B : A;
}
/**
 * 値を下限と上限の間へ制限する。
 * @param Value 処理または保持する値。
 * @param Low 許容する最小値。
 * @param High 許容する最大値。
 */
template <typename T> FORCEINLINE constexpr T Clamp(T Value, T Low, T High)
{
	return Value < Low ? Low : (High < Value ? High : Value);
}
/**
 * 値の絶対値を返す。
 * @param Value 処理または保持する値。
 */
template <typename T> FORCEINLINE constexpr T Abs(T Value)
{
	return Value < T{} ? -Value : Value;
}
/**
 * NaNと無限大でないか調べる。
 * @param Value 処理または保持する値。
 */
FORCEINLINE bool IsFinite(f64 Value) noexcept
{
	return isfinite(Value) != 0;
}
/**
 * 非負の値の平方根を求める。負の値はNaNになる。
 * @param Value 処理または保持する値。
 */
FORCEINLINE f64 Sqrt(f64 Value)
{
	return sqrt(Value);
}
/**
 * 非負の値の平方根を求める。負の値はNaNになる。
 * @param Value 処理または保持する値。
 */
FORCEINLINE f32 Sqrt(f32 Value)
{
	return sqrtf(Value);
}
/**
 * ラジアン角の正弦を返す。
 * @param Value 処理または保持する値。
 */
FORCEINLINE f64 Sin(f64 Value)
{
	return sin(Value);
}
/**
 * ラジアン角の余弦を返す。
 * @param Value 処理または保持する値。
 */
FORCEINLINE f64 Cos(f64 Value)
{
	return cos(Value);
}
/**
 * 最も近い整数へ丸める。中間値はゼロから遠い方へ丸める。
 * @param Value 処理または保持する値。
 */
FORCEINLINE int64 RoundToLong(f64 Value)
{
	return static_cast<int64>(llround(Value));
}
/**
 * 値以下の最大の整数値（負の無限大方向への丸め）。
 * @param Value 処理または保持する値。
 */
FORCEINLINE f64 Floor(f64 Value)
{
	return floor(Value);
}
/**
 * 値以上の最小の整数値（正の無限大方向への丸め）。
 * @param Value 処理または保持する値。
 */
FORCEINLINE f64 Ceil(f64 Value)
{
	return ceil(Value);
}
/**
 * 浮動小数点の剰余（被除数と同じ符号）。
 * @param Value 被除数。
 * @param Divisor 除数。
 */
FORCEINLINE f64 Fmod(f64 Value, f64 Divisor)
{
	return fmod(Value, Divisor);
}
/**
 * eの冪（指数関数）。
 * @param Value 指数。
 */
FORCEINLINE f64 Exp(f64 Value)
{
	return exp(Value);
}
/**
 * 数値型で表現できる上限を提供する。
 */
template <typename T> struct TNumericLimits
{
	/**
	 * この型で表せる最大の有限値を返す。
	 */
	static constexpr T Max()
	{
		if constexpr (T(-1) < T(0))
		{
			return static_cast<T>((uint64(1) << (sizeof(T) * CHAR_BIT - 1)) - 1);
		}
		else
		{
			return static_cast<T>(~static_cast<T>(0));
		}
	}
};
/**
 * 数値型で表現できる上限を提供する。
 */
template <> struct TNumericLimits<int32>
{
	/**
	 * この型で表せる最大の有限値を返す。
	 */
	FORCEINLINE static constexpr int32 Max()
	{
		return INT_MAX;
	}
};
/**
 * 数値型で表現できる上限を提供する。
 */
template <> struct TNumericLimits<f32>
{
	/**
	 * この型で表せる最大の有限値を返す。
	 */
	FORCEINLINE static constexpr f32 Max()
	{
		return FLT_MAX;
	}
	/**
	 * 未定義の数値結果を表すNaNを返す。
	 */
	static f32 QuietNaN()
	{
		return nanf("");
	}
};
/**
 * 数値型で表現できる上限を提供する。
 */
template <> struct TNumericLimits<f64>
{
	/**
	 * この型で表せる最大の有限値を返す。
	 */
	FORCEINLINE static constexpr f64 Max()
	{
		return DBL_MAX;
	}
	/**
	 * 未定義の数値結果を表すNaNを返す。
	 */
	static f64 QuietNaN()
	{
		return nan("");
	}
};
/**
 * 診断メッセージを保持する例外。例外構築中にメモリを確保しない。
 */
class FException
{
public:
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Message 診断に使う説明文。
	 */
	explicit FException(const char* Message) noexcept
	{
		// 現在処理している要素の位置。
		size_t Index = 0;
		if (Message)
		{
			for (; Index + 1 < sizeof(m_Message) && Message[Index]; ++Index)
			{
				m_Message[Index] = Message[Index];
			}
		}
		m_Message[Index] = 0;
	}
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Message 診断に使う説明文。
	 */
	template <typename T> explicit FException(const T& Message) noexcept : FException(Message.CStr())
	{
	}
	/**
	 * 派生した例外を基底型経由でも安全に破棄する。
	 */
	virtual ~FException() = default;
	/**
	 * 保持している診断メッセージを返す。
	 */
	FORCEINLINE const char* What() const noexcept
	{
		return m_Message;
	}

private:
	/**
	 * 確保失敗時にも使える固定長の診断メッセージ。
	 */
	char m_Message[1024]{};
};
/**
 * 空の値や異なる型の選択を読み出したことを通知する例外。
 */
class FBadAccess : public FException
{
public:
	/**
	 * 無効な値アクセスを示す診断メッセージを設定する。
	 */
	FBadAccess() : FException("Toolbox: invalid value access")
	{
	}
};
/**
 * データを持たない成功状態などを表す値。
 */
struct FEmpty
{
};
/**
 * 構築する選択肢をコンパイル時の番号で指定するタグ型。
 */
template <size_t I> struct TInPlaceIndex
{
};
/**
 * 選択肢の番号を指定するためのタグ値。
 */
template <size_t I> inline constexpr TInPlaceIndex<I> InPlaceIndex{};
} // namespace Toolbox
#endif
