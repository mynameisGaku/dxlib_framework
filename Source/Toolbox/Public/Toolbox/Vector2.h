// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_VECTOR2_H
#define TOOLBOX_VECTOR2_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 2D衝突判定へ渡す座標またはフレーム全体の変位。
 */
struct FVector2
{
	/**
	 * X軸成分。
	 */
	f32 X = 0;
	/**
	 * Y軸成分。
	 */
	f32 Y = 0;
	/**
	 * 指定した二成分を保持する。
	 * @param InX X軸の値。
	 * @param InY Y軸の値。
	 */
	FORCEINLINE constexpr FVector2(f32 InX = 0, f32 InY = 0) noexcept : X(InX), Y(InY)
	{
	}
	/**
	 * 全成分が有限かを返す。
	 */
	FORCEINLINE bool IsValid() const noexcept
	{
		return IsFinite(X) && IsFinite(Y);
	}
	/**
	 * 同じ軸の成分を加算する。
	 * @param Other 演算または比較の相手。
	 */
	FORCEINLINE FVector2 operator+(FVector2 Other) const noexcept
	{
		return {X + Other.X, Y + Other.Y};
	}
	/**
	 * 同じ軸の成分を減算する。
	 * @param Other 演算または比較の相手。
	 */
	FORCEINLINE FVector2 operator-(FVector2 Other) const noexcept
	{
		return {X - Other.X, Y - Other.Y};
	}
	/**
	 * 方向を反転する。
	 */
	FORCEINLINE FVector2 operator-() const noexcept
	{
		return {-X, -Y};
	}
	/**
	 * 各成分に倍率を掛ける。
	 * @param Scale 各成分に適用する倍率。
	 */
	FORCEINLINE FVector2 operator*(f32 Scale) const noexcept
	{
		return {X * Scale, Y * Scale};
	}
	/**
	 * 値を累積する。
	 * @param Other 演算または比較の相手。
	 */
	FORCEINLINE FVector2& operator+=(FVector2 Other) noexcept
	{
		return *this = *this + Other;
	}
	/**
	 * 二軸が完全に一致するか調べる。
	 * @param Other 演算または比較の相手。
	 */
	FORCEINLINE bool operator==(const FVector2& Other) const noexcept
	{
		return X == Other.X && Y == Other.Y;
	}
};
/**
 * 二つの方向の内積を返す。
 * @param A 左側の入力値。
 * @param B 右側の入力値。
 */
FORCEINLINE f32 Dot(FVector2 A, FVector2 B) noexcept
{
	return A.X * B.X + A.Y * B.Y;
}
/**
 * 二つの方向が作る平行四辺形の符号付き面積を返す。反時計回りが正。
 * @param A 左側の入力値。
 * @param B 右側の入力値。
 */
FORCEINLINE f32 Cross(FVector2 A, FVector2 B) noexcept
{
	return A.X * B.Y - A.Y * B.X;
}
} // namespace Toolbox
#endif
