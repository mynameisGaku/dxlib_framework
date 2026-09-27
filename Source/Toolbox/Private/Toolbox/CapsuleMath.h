// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_PRIVATE_CAPSULE_MATH_H
#define TOOLBOX_PRIVATE_CAPSULE_MATH_H
#include "Toolbox/Optional.h"
#include "Toolbox/Utility.h"
namespace Toolbox::CapsulePrivate
{
/**
 * 倍精度の3成分（2Dは第3成分を0にする）。
 */
struct FPoint
{
	f64 X = 0;
	f64 Y = 0;
	f64 Z = 0;
};
FORCEINLINE FPoint Sub(const FPoint& A, const FPoint& B) noexcept
{
	return {A.X - B.X, A.Y - B.Y, A.Z - B.Z};
}
FORCEINLINE FPoint Add(const FPoint& A, const FPoint& B) noexcept
{
	return {A.X + B.X, A.Y + B.Y, A.Z + B.Z};
}
FORCEINLINE FPoint Scale(const FPoint& A, f64 S) noexcept
{
	return {A.X * S, A.Y * S, A.Z * S};
}
FORCEINLINE f64 Dot(const FPoint& A, const FPoint& B) noexcept
{
	return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}
/**
 * 線分A＋t(B−A)上で点Pに最も近いパラメーター（0〜1）。長さ0の線分は0。
 */
FORCEINLINE f64 ClosestParam(const FPoint& A, const FPoint& B, const FPoint& P) noexcept
{
	const FPoint D = Sub(B, A);
	const f64 Length = Dot(D, D);
	if (!(Length > 0))
	{
		return 0;
	}
	return Clamp(Dot(Sub(P, A), D) / Length, 0.0, 1.0);
}
/**
 * 線分同士の最も近い点のパラメーター（Ericson 5.1.9）。長さ0・平行を含む。平行では第一の線分のパラメーターを
 * 端に寄せた値を使う（決定的）。
 */
inline void ClosestParams(const FPoint& P1, const FPoint& Q1, const FPoint& P2, const FPoint& Q2, f64& S,
                          f64& T) noexcept
{
	const FPoint D1 = Sub(Q1, P1);
	const FPoint D2 = Sub(Q2, P2);
	const FPoint R = Sub(P1, P2);
	const f64 A = Dot(D1, D1);
	const f64 E = Dot(D2, D2);
	const f64 F = Dot(D2, R);
	const f64 Epsilon = 1e-18;
	if (A <= Epsilon && E <= Epsilon)
	{
		S = 0;
		T = 0;
		return;
	}
	if (A <= Epsilon)
	{
		S = 0;
		T = Clamp(F / E, 0.0, 1.0);
		return;
	}
	const f64 C = Dot(D1, R);
	if (E <= Epsilon)
	{
		T = 0;
		S = Clamp(-C / A, 0.0, 1.0);
		return;
	}
	const f64 B = Dot(D1, D2);
	const f64 Denominator = A * E - B * B;
	// 平行（分母が相対的に0）なら第一の線分は0から始める。
	S = Denominator > 1e-12 * A * E ? Clamp((B * F - C * E) / Denominator, 0.0, 1.0) : 0.0;
	T = (B * S + F) / E;
	if (T < 0)
	{
		T = 0;
		S = Clamp(-C / A, 0.0, 1.0);
	}
	else if (T > 1)
	{
		T = 1;
		S = Clamp((B - C) / A, 0.0, 1.0);
	}
}
/**
 * 凸な関数の0〜1での最小のパラメーター。両端と黄金分割探索（48回）の結果のうち、値が最小のもの（同じ値は小さい方）。
 * @param Value パラメーターから値を返す関数。
 */
template <typename F> f64 MinimizeConvex(F&& Value)
{
	constexpr f64 Ratio = 0.6180339887498949;
	f64 Low = 0;
	f64 High = 1;
	f64 X1 = High - Ratio * (High - Low);
	f64 X2 = Low + Ratio * (High - Low);
	f64 V1 = Value(X1);
	f64 V2 = Value(X2);
	for (int32 Iteration = 0; Iteration < 48; ++Iteration)
	{
		if (V1 <= V2)
		{
			High = X2;
			X2 = X1;
			V2 = V1;
			X1 = High - Ratio * (High - Low);
			V1 = Value(X1);
		}
		else
		{
			Low = X1;
			X1 = X2;
			V1 = V2;
			X2 = Low + Ratio * (High - Low);
			V2 = Value(X2);
		}
	}
	f64 Best = 0;
	f64 BestValue = Value(0.0);
	const f64 Middle = V1 <= V2 ? X1 : X2;
	const f64 MiddleValue = V1 <= V2 ? V1 : V2;
	if (MiddleValue < BestValue)
	{
		Best = Middle;
		BestValue = MiddleValue;
	}
	if (Value(1.0) < BestValue)
	{
		Best = 1;
	}
	return Best;
}
/**
 * 線分Start〜Endと、中心線A〜B・半径Rのカプセルの胴体（円柱面、両端の球を除く）の最初の交点の割合（Ericson 5.3.7）。
 * 始点が胴体の内部なら0。交わらなければ空。
 */
inline TOptional<f64> SegmentBody(const FPoint& Start, const FPoint& End, const FPoint& A, const FPoint& B,
                                  f64 R) noexcept
{
	const FPoint D = Sub(B, A);
	const FPoint M = Sub(Start, A);
	const FPoint N = Sub(End, Start);
	const f64 MD = Dot(M, D);
	const f64 ND = Dot(N, D);
	const f64 DD = Dot(D, D);
	if (!(DD > 0))
	{
		return {};
	}
	// 両端の外側に留まる線分は胴体に入らない（端の球で扱う）。
	if ((MD < 0 && MD + ND < 0) || (MD > DD && MD + ND > DD))
	{
		return {};
	}
	const f64 NN = Dot(N, N);
	const f64 MN = Dot(M, N);
	const f64 Av = DD * NN - ND * ND;
	const f64 K = Dot(M, M) - R * R;
	const f64 C = DD * K - MD * MD;
	const bool bInsideLength = MD >= 0 && MD <= DD;
	if (C <= 0 && bInsideLength)
	{
		return 0.0;
	}
	if (Abs(Av) <= 1e-18 * Max(1.0, DD * NN))
	{
		// 軸に平行：胴体の側面には入らない（端の球で扱う）。
		return {};
	}
	const f64 Bv = DD * MN - ND * MD;
	const f64 Discriminant = Bv * Bv - Av * C;
	if (Discriminant < 0)
	{
		return {};
	}
	const f64 Time = (-Bv - Sqrt(Discriminant)) / Av;
	if (Time < 0 || Time > 1)
	{
		return {};
	}
	const f64 Along = MD + Time * ND;
	if (Along < 0 || Along > DD)
	{
		return {};
	}
	return Time;
}
/**
 * 距離の下限を使う保守的な前進。Distance(t)は時刻tの表面間の符号付き距離、LengthはTime 0〜1の移動量。
 * 接触（距離≦Tolerance）の時刻を返し、Time 1まで接触しなければ空。開始時の接触は呼出し側で判定する。
 * 64回で収束しない場合は、到達した時刻（接触の前）を返す。
 */
template <typename F> TOptional<f64> AdvanceConservatively(F&& Distance, f64 Length, f64 Tolerance)
{
	if (!(Length > 0))
	{
		return {};
	}
	f64 Time = 0;
	for (int32 Iteration = 0; Iteration < 64; ++Iteration)
	{
		const f64 Gap = Distance(Time);
		if (Gap <= Tolerance)
		{
			return Time;
		}
		Time += Gap / Length;
		if (Time > 1)
		{
			// 終点でも接触していなければ当たらない。
			if (Distance(1.0) <= Tolerance)
			{
				return 1.0;
			}
			return {};
		}
	}
	return Time;
}
} // namespace Toolbox::CapsulePrivate
#endif
