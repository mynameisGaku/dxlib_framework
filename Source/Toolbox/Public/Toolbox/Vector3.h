// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_VECTOR3_H
#define TOOLBOX_VECTOR3_H
#include "Toolbox/Utility.h"
#if defined(_M_X64) || defined(__SSE2__)
#define TOOLBOX_SIMD_SSE2 1
#include <emmintrin.h>
#else
#define TOOLBOX_SIMD_SSE2 0
#endif
namespace Toolbox
{
/**
 * 3D座標と方向。SSE2対応CPUでは四成分をまとめて演算する。
 */
struct alignas(16) FVector3
{
	/**
	 * X軸の成分。
	 */
	f32 X = 0;
	/**
	 * Y軸の成分。
	 */
	f32 Y = 0;
	/**
	 * Z軸の成分。
	 */
	f32 Z = 0;
	/**
	 * SIMDロード用の未使用成分。
	 */
	f32 Padding = 0;
	/**
	 * 三軸の値からベクトルを作る。
	 * @param InX X軸の初期値。
	 * @param InY Y軸の初期値。
	 * @param InZ Z軸の初期値。
	 */
	constexpr FVector3(f32 InX = 0, f32 InY = 0, f32 InZ = 0) : X(InX), Y(InY), Z(InZ)
	{
	}
	/**
	 * 同じ軸の成分を加算する。
	 * @param Other 演算または比較の相手。
	 */
	FVector3 operator+(FVector3 Other) const noexcept;
	/**
	 * 同じ軸の成分を減算する。
	 * @param Other 演算または比較の相手。
	 */
	FVector3 operator-(FVector3 Other) const noexcept;
	/**
	 * 方向を反転する。
	 */
	FVector3 operator-() const noexcept
	{
		return {-X, -Y, -Z};
	}
	/**
	 * 各成分に倍率を掛ける。
	 * @param Scale 各成分に適用する倍率または除数。
	 */
	FVector3 operator*(f32 Scale) const noexcept;
	/**
	 * 各成分を除算する。ゼロまたは非有限の除数は例外で通知する。
	 * @param Scale 各成分に適用する倍率または除数。
	 */
	FVector3 operator/(f32 Scale) const
	{
		if (!IsFinite(Scale) || Scale == 0)
		{
			throw FException("Invalid vector divisor");
		}
		return {X / Scale, Y / Scale, Z / Scale};
	}
	/**
	 * 値を累積する。
	 * @param Other 演算または比較の相手。
	 */
	FVector3& operator+=(FVector3 Other) noexcept
	{
		return *this = *this + Other;
	}
	/**
	 * 三軸が完全に一致するか調べる。
	 * @param Other 演算または比較の相手。
	 */
	bool operator==(const FVector3& Other) const noexcept
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}
	/**
	 * 軸番号0〜2の成分を返す。
	 * @param Axis 回転軸または参照する軸番号。
	 */
	f32 Component(int32 Axis) const noexcept
	{
		return Axis == 0 ? X : (Axis == 1 ? Y : Z);
	}
	/**
	 * 全成分が有限値か調べる。
	 */
	bool IsValid() const noexcept
	{
		return IsFinite(X) && IsFinite(Y) && IsFinite(Z);
	}
};
/**
 * 二つの方向の内積を返す。
 * @param A 左側の入力値。
 * @param B 右側の入力値。
 */
f32 Dot(FVector3 A, FVector3 B) noexcept;
/**
 * 二つの方向に直交する外積を返す。
 * @param A 左側の入力値。
 * @param B 右側の入力値。
 */
FVector3 Cross(FVector3 A, FVector3 B) noexcept;
/**
 * 長さの二乗を返し、不要な平方根を避ける。
 * @param Value 処理対象の値。
 */
inline f32 LengthSquared(FVector3 Value) noexcept
{
	return Dot(Value, Value);
}
/**
 * ベクトルの長さを返す。
 * @param Value 処理対象の値。
 */
inline f32 Length(FVector3 Value) noexcept
{
	/**
	 * 二乗を倍精度で計算し、有限な長さのオーバーフローと微小値の消失を防ぐ。
	 */
	const f64 Squared = f64(Value.X) * Value.X + f64(Value.Y) * Value.Y + f64(Value.Z) * Value.Z;
	return static_cast<f32>(Sqrt(Squared));
}
/**
 * 単位ベクトルへ変換する。ゼロ長はゼロベクトルを返す。
 * @param Value 処理対象の値。
 */
inline FVector3 Normalize(FVector3 Value) noexcept
{
	/**
	 * 最大成分を基準に縮尺をそろえ、長さが単精度を超える方向にも対応する。
	 */
	const f32 Scale = Max(Abs(Value.X), Max(Abs(Value.Y), Abs(Value.Z)));
	if (!Value.IsValid() || Scale == 0)
	{
		return {};
	}
	/**
	 * 各成分の絶対値が1以下になる補助ベクトル。
	 */
	const FVector3 Scaled{Value.X / Scale, Value.Y / Scale, Value.Z / Scale};
	return Scaled * (1.0f / Sqrt(LengthSquared(Scaled)));
}
/**
 * 軸ごとに小さい値を選ぶ。
 * @param A 左側の入力値。
 * @param B 右側の入力値。
 */
inline FVector3 Min(FVector3 A, FVector3 B) noexcept
{
	return {Min(A.X, B.X), Min(A.Y, B.Y), Min(A.Z, B.Z)};
}
/**
 * 軸ごとに大きい値を選ぶ。
 * @param A 左側の入力値。
 * @param B 右側の入力値。
 */
inline FVector3 Max(FVector3 A, FVector3 B) noexcept
{
	return {-Min(-A.X, -B.X), -Min(-A.Y, -B.Y), -Min(-A.Z, -B.Z)};
}
} // namespace Toolbox
#endif
