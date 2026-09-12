// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Quaternion.h"
namespace Toolbox
{
FQuaternion FQuaternion::FromMatrix(const FMatrix4& Matrix)
{
	/**
	 * 変換元行列の行優先成分。
	 */
	const auto& M = Matrix.Values;
	/**
	 * 行列から取り出したローカルX軸。
	 */
	const FVector3 X{M[0], M[4], M[8]};
	/**
	 * 行列から取り出したローカルY軸。
	 */
	const FVector3 Y{M[1], M[5], M[9]};
	/**
	 * 行列から取り出したローカルZ軸。
	 */
	const FVector3 Z{M[2], M[6], M[10]};
	if (!X.IsValid() || !Y.IsValid() || !Z.IsValid() || Abs(LengthSquared(X) - 1) > 1e-4f ||
	    Abs(LengthSquared(Y) - 1) > 1e-4f || Abs(LengthSquared(Z) - 1) > 1e-4f || Abs(Dot(X, Y)) > 1e-4f ||
	    Abs(Dot(X, Z)) > 1e-4f || Abs(Dot(Y, Z)) > 1e-4f || Dot(Cross(X, Y), Z) < 0.9999f)
	{
		throw FException("Matrix is not an orthonormal rotation");
	}
	/**
	 * 回転行列の対角和。
	 */
	const f32 Trace = M[0] + M[5] + M[10];
	/**
	 * 計算中の単位四元数。
	 */
	FQuaternion Q;
	if (Trace > 0)
	{
		/**
		 * 最も安定する四元数成分の四倍。
		 */
		const f32 S = Sqrt(Trace + 1) * 2;
		Q = {(M[9] - M[6]) / S, (M[2] - M[8]) / S, (M[4] - M[1]) / S, S * 0.25f};
	}
	else if (M[0] > M[5] && M[0] > M[10])
	{
		/**
		 * 最も安定する四元数成分の四倍。
		 */
		const f32 S = Sqrt(1 + M[0] - M[5] - M[10]) * 2;
		Q = {S * 0.25f, (M[1] + M[4]) / S, (M[2] + M[8]) / S, (M[9] - M[6]) / S};
	}
	else if (M[5] > M[10])
	{
		/**
		 * 最も安定する四元数成分の四倍。
		 */
		const f32 S = Sqrt(1 + M[5] - M[0] - M[10]) * 2;
		Q = {(M[1] + M[4]) / S, S * 0.25f, (M[6] + M[9]) / S, (M[2] - M[8]) / S};
	}
	else
	{
		/**
		 * 最も安定する四元数成分の四倍。
		 */
		const f32 S = Sqrt(1 + M[10] - M[0] - M[5]) * 2;
		Q = {(M[2] + M[8]) / S, (M[6] + M[9]) / S, S * 0.25f, (M[4] - M[1]) / S};
	}
	return Q.Normalized();
}
} // namespace Toolbox
