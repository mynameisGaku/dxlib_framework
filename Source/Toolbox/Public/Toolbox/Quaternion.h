// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_QUATERNION_H
#define TOOLBOX_QUATERNION_H
#include "Toolbox/Matrix4.h"
namespace Toolbox
{
/**
 * 回転を表す四元数。X・Y・Zが虚部、Wが実部で、初期値は無回転。
 */
struct alignas(16) FQuaternion
{
	/**
	 * X軸に対応する虚部。
	 */
	f32 X = 0;
	/**
	 * Y軸に対応する虚部。
	 */
	f32 Y = 0;
	/**
	 * Z軸に対応する虚部。
	 */
	f32 Z = 0;
	/**
	 * 実数成分。
	 */
	f32 W = 1;
	/**
	 * 任意の軸とラジアン角から単位四元数を作る。
	 * @param Axis 回転軸または参照する軸番号。
	 * @param Radians ラジアン単位の回転角。
	 */
	static FQuaternion FromAxisAngle(FVector3 Axis, f32 Radians)
	{
		Axis = Normalize(Axis);
		if (LengthSquared(Axis) == 0 || !IsFinite(Radians))
		{
			throw FException("Invalid quaternion axis or angle");
		}
		const f32 S = static_cast<f32>(Sin(Radians * 0.5f));
		return {Axis.X * S, Axis.Y * S, Axis.Z * S, static_cast<f32>(Cos(Radians * 0.5f))};
	}
	/**
	 * 四成分の長さの二乗を返す。
	 */
	f32 NormSquared() const noexcept
	{
		return X * X + Y * Y + Z * Z + W * W;
	}
	/**
	 * 単位四元数を返す。ゼロ長・非有限値は例外で通知する。
	 */
	FQuaternion Normalized() const
	{
		/**
		 * 全成分の二乗を倍精度で蓄積し、微小値と巨大値の正規化を保つ。
		 */
		const f64 Norm = Sqrt(f64(X) * X + f64(Y) * Y + f64(Z) * Z + f64(W) * W);
		if (!IsFinite(Norm) || Norm <= 0)
		{
			throw FException("Invalid quaternion norm");
		}
		return {static_cast<f32>(X / Norm), static_cast<f32>(Y / Norm), static_cast<f32>(Z / Norm),
		        static_cast<f32>(W / Norm)};
	}
	/**
	 * 虚部の符号を反転する。単位四元数では逆回転になる。
	 */
	FQuaternion Conjugate() const noexcept
	{
		return {-X, -Y, -Z, W};
	}
	/**
	 * 非単位の四元数にも対応する逆元を返す。ゼロ長は例外で通知する。
	 */
	FQuaternion Inverse() const
	{
		/**
		 * 逆元が表現可能な非単位四元数も、二乗の丸めで失わないための倍精度ノルム。
		 */
		const f64 Norm = f64(X) * X + f64(Y) * Y + f64(Z) * Z + f64(W) * W;
		if (!IsFinite(Norm) || Norm <= 0)
		{
			throw FException("Invalid quaternion inverse");
		}
		return {static_cast<f32>(-X / Norm), static_cast<f32>(-Y / Norm), static_cast<f32>(-Z / Norm),
		        static_cast<f32>(W / Norm)};
	}
	/**
	 * 回転を合成する。A*BはBを先に適用する。
	 * @param Other 演算または比較の相手。
	 */
	FQuaternion operator*(const FQuaternion& Other) const noexcept
	{
		/**
		 * 合成後の四元数の虚部。
		 */
		const FVector3 Imaginary = FVector3{Other.X, Other.Y, Other.Z} * W + FVector3{X, Y, Z} * Other.W +
		                           Cross({X, Y, Z}, {Other.X, Other.Y, Other.Z});
		return {Imaginary.X, Imaginary.Y, Imaginary.Z, W * Other.W - Dot({X, Y, Z}, {Other.X, Other.Y, Other.Z})};
	}
	/**
	 * 四元数を正規化し、方向または原点基準の位置を回転する。
	 * @param Value 処理対象の値。
	 */
	FVector3 Rotate(FVector3 Value) const
	{
		/**
		 * 計算中の単位四元数。
		 */
		const auto Q = Normalized();
		/**
		 * 回転を表す単位軸。
		 */
		const FVector3 Axis{Q.X, Q.Y, Q.Z};
		/**
		 * 虚部と入力方向の外積の二倍。
		 * @param Axis 回転軸または参照する軸番号。
		 * @param Value 処理対象の値。
		 */
		const FVector3 TwiceCross = Cross(Axis, Value) * 2;
		return Value + TwiceCross * Q.W + Cross(Axis, TwiceCross);
	}
	/**
	 * 同じ回転を表す行優先4×4行列を返す。
	 */
	FMatrix4 ToMatrix() const
	{
		/**
		 * 計算中の単位四元数。
		 */
		const auto Q = Normalized();
		/**
		 * 計算または検索の結果。
		 */
		FMatrix4 Result;
		Result.Values = {1 - 2 * (Q.Y * Q.Y + Q.Z * Q.Z),
		                 2 * (Q.X * Q.Y - Q.Z * Q.W),
		                 2 * (Q.X * Q.Z + Q.Y * Q.W),
		                 0,
		                 2 * (Q.X * Q.Y + Q.Z * Q.W),
		                 1 - 2 * (Q.X * Q.X + Q.Z * Q.Z),
		                 2 * (Q.Y * Q.Z - Q.X * Q.W),
		                 0,
		                 2 * (Q.X * Q.Z - Q.Y * Q.W),
		                 2 * (Q.Y * Q.Z + Q.X * Q.W),
		                 1 - 2 * (Q.X * Q.X + Q.Y * Q.Y),
		                 0,
		                 0,
		                 0,
		                 0,
		                 1};
		return Result;
	}
	/**
	 * 回転行列から四元数を取り出す。拡大縮小・反射は受け付けない。
	 * @param Matrix 変換元の行列。
	 */
	static FQuaternion FromMatrix(const FMatrix4& Matrix);
	/**
	 * 二つの回転を最短経路で球面補間する。割合は0〜1へ制限する。
	 * @param A 左側の入力値。
	 * @param B 右側の入力値。
	 * @param Alpha 始点から終点への補間割合。
	 */
	static FQuaternion Slerp(FQuaternion A, FQuaternion B, f32 Alpha)
	{
		if (!IsFinite(Alpha))
		{
			throw FException("Invalid interpolation fraction");
		}
		A = A.Normalized();
		B = B.Normalized();
		Alpha = Clamp(Alpha, 0.0f, 1.0f);
		/**
		 * 二つの単位四元数の内積。
		 */
		f32 Cosine = A.X * B.X + A.Y * B.Y + A.Z * B.Z + A.W * B.W;
		if (Cosine < 0)
		{
			B = {-B.X, -B.Y, -B.Z, -B.W};
			Cosine = -Cosine;
		}
		/**
		 * 左側の回転に掛ける補間係数。
		 */
		f32 Left = 1 - Alpha;
		/**
		 * 右側の回転に掛ける補間係数。
		 */
		f32 Right = Alpha;
		if (Cosine < 0.9995f)
		{
			/**
			 * 補間する回転間の角度。
			 */
			const f32 Angle = static_cast<f32>(acos(Clamp(Cosine, -1.0f, 1.0f)));
			/**
			 * 球面補間の正規化に使う正弦。
			 */
			const f32 Denominator = static_cast<f32>(Sin(Angle));
			Left = static_cast<f32>(Sin((1 - Alpha) * Angle)) / Denominator;
			Right = static_cast<f32>(Sin(Alpha * Angle)) / Denominator;
		}
		return FQuaternion{A.X * Left + B.X * Right, A.Y * Left + B.Y * Right, A.Z * Left + B.Z * Right,
		                   A.W * Left + B.W * Right}
		    .Normalized();
	}
};
} // namespace Toolbox
#endif
