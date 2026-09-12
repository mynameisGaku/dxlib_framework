// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Matrix4.h"
namespace Toolbox
{
FMatrix4 FMatrix4::Translation(FVector3 Offset)
{
	/**
	 * 計算または検索の結果。
	 */
	FMatrix4 Result;
	Result.Values[3] = Offset.X;
	Result.Values[7] = Offset.Y;
	Result.Values[11] = Offset.Z;
	return Result;
}
FMatrix4 FMatrix4::Scale(FVector3 Factors)
{
	/**
	 * 計算または検索の結果。
	 */
	FMatrix4 Result;
	Result.Values[0] = Factors.X;
	Result.Values[5] = Factors.Y;
	Result.Values[10] = Factors.Z;
	return Result;
}
FMatrix4 FMatrix4::Rotation(FVector3 Axis, f32 Radians)
{
	Axis = Normalize(Axis);
	if (LengthSquared(Axis) == 0 || !IsFinite(Radians))
	{
		throw FException("Invalid rotation axis or angle");
	}
	/**
	 * 回転角の余弦。
	 */
	const f32 C = static_cast<f32>(Cos(Radians));
	/**
	 * 回転角の正弦。
	 */
	const f32 S = static_cast<f32>(Sin(Radians));
	/**
	 * ロドリゲスの公式で使う1と余弦の差。
	 */
	const f32 T = 1 - C;
	/**
	 * 計算または検索の結果。
	 */
	FMatrix4 Result;
	Result.Values = {T * Axis.X * Axis.X + C,
	                 T * Axis.X * Axis.Y - S * Axis.Z,
	                 T * Axis.X * Axis.Z + S * Axis.Y,
	                 0,
	                 T * Axis.X * Axis.Y + S * Axis.Z,
	                 T * Axis.Y * Axis.Y + C,
	                 T * Axis.Y * Axis.Z - S * Axis.X,
	                 0,
	                 T * Axis.X * Axis.Z - S * Axis.Y,
	                 T * Axis.Y * Axis.Z + S * Axis.X,
	                 T * Axis.Z * Axis.Z + C,
	                 0,
	                 0,
	                 0,
	                 0,
	                 1};
	return Result;
}
FMatrix4 FMatrix4::operator*(const FMatrix4& Other) const noexcept
{
	/**
	 * 計算または検索の結果。
	 */
	FMatrix4 Result;
	for (int32 Row = 0; Row < 4; ++Row)
	{
#if TOOLBOX_SIMD_SSE2
		/**
		 * 積和を蓄積する値。
		 */
		__m128 Sum = _mm_setzero_ps();
		for (int32 K = 0; K < 4; ++K)
		{
			Sum = _mm_add_ps(Sum, _mm_mul_ps(_mm_set1_ps(Values[static_cast<size_t>(Row * 4 + K)]),
			                                 _mm_load_ps(Other.Values.Data() + K * 4)));
		}
		_mm_store_ps(Result.Values.Data() + Row * 4, Sum);
#else
		for (int32 Column = 0; Column < 4; ++Column)
		{
			/**
			 * 積和を蓄積する値。
			 */
			f32 Sum = 0;
			for (int32 K = 0; K < 4; ++K)
			{
				Sum += Values[static_cast<size_t>(Row * 4 + K)] * Other.Values[static_cast<size_t>(K * 4 + Column)];
			}
			Result.Values[static_cast<size_t>(Row * 4 + Column)] = Sum;
		}
#endif
	}
	return Result;
}
FVector3 FMatrix4::TransformPoint(FVector3 Point) const
{
	/**
	 * 射影変換後の同次座標。
	 */
	const f32 W = Values[12] * Point.X + Values[13] * Point.Y + Values[14] * Point.Z + Values[15];
	if (!IsFinite(W) || W == 0)
	{
		throw FException("Point transformed to infinity");
	}
	return (TransformDirection(Point) + FVector3{Values[3], Values[7], Values[11]}) / W;
}
FVector3 FMatrix4::TransformDirection(FVector3 Direction) const noexcept
{
	return {Dot({Values[0], Values[1], Values[2]}, Direction), Dot({Values[4], Values[5], Values[6]}, Direction),
	        Dot({Values[8], Values[9], Values[10]}, Direction)};
}
FMatrix4 FMatrix4::Transposed() const noexcept
{
	/**
	 * 計算または検索の結果。
	 */
	FMatrix4 Result;
	for (size_t Row = 0; Row < 4; ++Row)
	{
		for (size_t Column = 0; Column < 4; ++Column)
		{
			Result.Values[Row * 4 + Column] = Values[Column * 4 + Row];
		}
	}
	return Result;
}
bool FMatrix4::TryInverse(FMatrix4& Output, f32 Tolerance) const noexcept
{
	if (!IsFinite(Tolerance) || Tolerance <= 0)
	{
		return false;
	}
	/**
	 * 行消去の作業用行列。
	 */
	FMatrix4 Work = *this;
	/**
	 * 単位行列から構築する逆行列。
	 */
	FMatrix4 Inverse;
	for (f32 Value : Values)
	{
		if (!IsFinite(Value))
		{
			return false;
		}
	}
	for (size_t Column = 0; Column < 4; ++Column)
	{
		/**
		 * 絶対値が最大のピボット行。
		 */
		size_t Pivot = Column;
		for (size_t Row = Column + 1; Row < 4; ++Row)
		{
			if (Abs(Work.Values[Row * 4 + Column]) > Abs(Work.Values[Pivot * 4 + Column]))
			{
				Pivot = Row;
			}
		}
		if (Abs(Work.Values[Pivot * 4 + Column]) <= Tolerance)
		{
			return false;
		}
		for (size_t K = 0; K < 4; ++K)
		{
			Swap(Work.Values[Column * 4 + K], Work.Values[Pivot * 4 + K]);
			Swap(Inverse.Values[Column * 4 + K], Inverse.Values[Pivot * 4 + K]);
		}
		/**
		 * 行を正規化する除数。
		 */
		const f32 Divisor = Work.Values[Column * 4 + Column];
#if TOOLBOX_SIMD_SSE2
		_mm_store_ps(Work.Values.Data() + Column * 4,
		             _mm_div_ps(_mm_load_ps(Work.Values.Data() + Column * 4), _mm_set1_ps(Divisor)));
		_mm_store_ps(Inverse.Values.Data() + Column * 4,
		             _mm_div_ps(_mm_load_ps(Inverse.Values.Data() + Column * 4), _mm_set1_ps(Divisor)));
#else
		for (size_t K = 0; K < 4; ++K)
		{
			Work.Values[Column * 4 + K] /= Divisor;
			Inverse.Values[Column * 4 + K] /= Divisor;
		}
#endif
		for (size_t Row = 0; Row < 4; ++Row)
		{
			if (Row == Column)
			{
				continue;
			}
			/**
			 * 他行から消去する成分の倍率。
			 */
			const f32 Factor = Work.Values[Row * 4 + Column];
#if TOOLBOX_SIMD_SSE2
			_mm_store_ps(Work.Values.Data() + Row * 4,
			             _mm_sub_ps(_mm_load_ps(Work.Values.Data() + Row * 4),
			                        _mm_mul_ps(_mm_set1_ps(Factor), _mm_load_ps(Work.Values.Data() + Column * 4))));
			_mm_store_ps(Inverse.Values.Data() + Row * 4,
			             _mm_sub_ps(_mm_load_ps(Inverse.Values.Data() + Row * 4),
			                        _mm_mul_ps(_mm_set1_ps(Factor), _mm_load_ps(Inverse.Values.Data() + Column * 4))));
#else
			for (size_t K = 0; K < 4; ++K)
			{
				Work.Values[Row * 4 + K] -= Factor * Work.Values[Column * 4 + K];
				Inverse.Values[Row * 4 + K] -= Factor * Inverse.Values[Column * 4 + K];
			}
#endif
		}
	}
	for (f32 Value : Inverse.Values)
	{
		if (!IsFinite(Value))
		{
			return false;
		}
	}
	Output = Inverse;
	return true;
}
} // namespace Toolbox
