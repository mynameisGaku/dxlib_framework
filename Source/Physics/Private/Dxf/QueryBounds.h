// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_PHYSICS_QUERY_BOUNDS_H
#define DXF_PRIVATE_PHYSICS_QUERY_BOUNDS_H
#include "Toolbox/Utility.h"
namespace Dxf::PhysicsPrivate
{
/**
 * 問い合わせ索引の軸平行境界（f64）。Min <= Max。
 */
template <Toolbox::int32 Dimension> struct TQueryBounds
{
	/**
	 * 各軸の最小座標。
	 */
	Toolbox::f64 Min[Dimension]{};
	/**
	 * 各軸の最大座標。
	 */
	Toolbox::f64 Max[Dimension]{};
};

/**
 * 形状の境界を求めた結果。索引に入れられない形状（現在の姿勢のWorld形状が無効）はbIndexable=false。
 */
template <Toolbox::int32 Dimension> struct TQueryShapeBounds
{
	/**
	 * 索引に入れられるか。
	 */
	bool bIndexable = false;
	/**
	 * World形状を覆う境界（丸めの余白を含む）。
	 */
	TQueryBounds<Dimension> Tight;
};

/**
 * 境界の二つの軸平行箱が重なるか（接する場合を含む）を返す。
 * @param A 一つ目。
 * @param B 二つ目。
 */
template <Toolbox::int32 Dimension>
FORCEINLINE bool Overlaps_Internal(const TQueryBounds<Dimension>& A, const TQueryBounds<Dimension>& B) noexcept
{
	for (Toolbox::int32 Axis = 0; Axis < Dimension; ++Axis)
	{
		if (A.Max[Axis] < B.Min[Axis] || B.Max[Axis] < A.Min[Axis])
		{
			return false;
		}
	}
	return true;
}
/**
 * AがBを含むか（境界上を含む）を返す。
 * @param A 外側。
 * @param B 内側。
 */
template <Toolbox::int32 Dimension>
FORCEINLINE bool Contains_Internal(const TQueryBounds<Dimension>& A, const TQueryBounds<Dimension>& B) noexcept
{
	for (Toolbox::int32 Axis = 0; Axis < Dimension; ++Axis)
	{
		if (B.Min[Axis] < A.Min[Axis] || B.Max[Axis] > A.Max[Axis])
		{
			return false;
		}
	}
	return true;
}
/**
 * 二つの境界を合わせた境界を返す。
 * @param A 一つ目。
 * @param B 二つ目。
 */
template <Toolbox::int32 Dimension>
FORCEINLINE TQueryBounds<Dimension> Union_Internal(const TQueryBounds<Dimension>& A,
                                                   const TQueryBounds<Dimension>& B) noexcept
{
	TQueryBounds<Dimension> Result;
	for (Toolbox::int32 Axis = 0; Axis < Dimension; ++Axis)
	{
		Result.Min[Axis] = A.Min[Axis] < B.Min[Axis] ? A.Min[Axis] : B.Min[Axis];
		Result.Max[Axis] = A.Max[Axis] > B.Max[Axis] ? A.Max[Axis] : B.Max[Axis];
	}
	return Result;
}
/**
 * 境界を各軸の両側へ広げる。
 * @param Bounds 元の境界。
 * @param Amount 広げる距離（非負）。
 */
template <Toolbox::int32 Dimension>
FORCEINLINE TQueryBounds<Dimension> Expanded_Internal(const TQueryBounds<Dimension>& Bounds,
                                                      Toolbox::f64 Amount) noexcept
{
	TQueryBounds<Dimension> Result = Bounds;
	for (Toolbox::int32 Axis = 0; Axis < Dimension; ++Axis)
	{
		Result.Min[Axis] -= Amount;
		Result.Max[Axis] += Amount;
	}
	return Result;
}
/**
 * 挿入先を選ぶ費用（2Dは周長、3Dは表面積）。
 * @param Bounds 対象の境界。
 */
template <Toolbox::int32 Dimension>
FORCEINLINE Toolbox::f64 Cost_Internal(const TQueryBounds<Dimension>& Bounds) noexcept
{
	if constexpr (Dimension == 2)
	{
		return 2 * ((Bounds.Max[0] - Bounds.Min[0]) + (Bounds.Max[1] - Bounds.Min[1]));
	}
	else
	{
		const Toolbox::f64 X = Bounds.Max[0] - Bounds.Min[0];
		const Toolbox::f64 Y = Bounds.Max[1] - Bounds.Min[1];
		const Toolbox::f64 Z = Bounds.Max[2] - Bounds.Min[2];
		return 2 * (X * Y + Y * Z + Z * X);
	}
}
/**
 * 境界の座標の絶対値の最大を返す。
 * @param Bounds 対象の境界。
 */
template <Toolbox::int32 Dimension>
FORCEINLINE Toolbox::f64 MaxAbs_Internal(const TQueryBounds<Dimension>& Bounds) noexcept
{
	Toolbox::f64 Result = 0;
	for (Toolbox::int32 Axis = 0; Axis < Dimension; ++Axis)
	{
		Result = Toolbox::Max(Result, Toolbox::Max(Toolbox::Abs(Bounds.Min[Axis]), Toolbox::Abs(Bounds.Max[Axis])));
	}
	return Result;
}
/**
 * 形状の境界を、丸めで内側へ縮まない余白（座標の規模の2^-40と2^-60の和）だけ広げる。
 * @param Bounds f64で求めた形状の境界。
 */
template <Toolbox::int32 Dimension>
FORCEINLINE TQueryBounds<Dimension> Padded_Internal(const TQueryBounds<Dimension>& Bounds) noexcept
{
	const Toolbox::f64 Scale = MaxAbs_Internal(Bounds);
	return Expanded_Internal(Bounds, Scale * 9.094947017729282e-13 + 8.673617379884035e-19);
}
/**
 * 線分（半径で広げたノードの境界との判定用）。始点と変位をf64で持つ。
 */
template <Toolbox::int32 Dimension> struct TQuerySegment
{
	/**
	 * 始点。
	 */
	Toolbox::f64 Start[Dimension]{};
	/**
	 * 変位（終点－始点）。
	 */
	Toolbox::f64 Delta[Dimension]{};
	/**
	 * ノードの境界を広げる距離（移動する円／球の半径、線分は0）。
	 */
	Toolbox::f64 Radius = 0;
	/**
	 * 線分全体（半径を含む）を覆う境界。
	 */
	TQueryBounds<Dimension> Bounds;
};
/**
 * 半径で広げた境界と線分（割合0～1）が交わるか（接する場合を含む）を返す。保守的な判定で、偽の陽性は許す。
 * @param Bounds ノードの境界。
 * @param Segment 線分。
 */
template <Toolbox::int32 Dimension>
FORCEINLINE bool SegmentOverlaps_Internal(const TQueryBounds<Dimension>& Bounds,
                                          const TQuerySegment<Dimension>& Segment) noexcept
{
	if (!Overlaps_Internal(Bounds, Segment.Bounds))
	{
		return false;
	}
	Toolbox::f64 Low = 0;
	Toolbox::f64 High = 1;
	for (Toolbox::int32 Axis = 0; Axis < Dimension; ++Axis)
	{
		const Toolbox::f64 Min = Bounds.Min[Axis] - Segment.Radius;
		const Toolbox::f64 Max = Bounds.Max[Axis] + Segment.Radius;
		const Toolbox::f64 Origin = Segment.Start[Axis];
		const Toolbox::f64 Delta = Segment.Delta[Axis];
		if (Delta == 0)
		{
			if (Origin < Min || Origin > Max)
			{
				return false;
			}
			continue;
		}
		Toolbox::f64 First = (Min - Origin) / Delta;
		Toolbox::f64 Last = (Max - Origin) / Delta;
		if (First > Last)
		{
			const Toolbox::f64 Swapped = First;
			First = Last;
			Last = Swapped;
		}
		// 割合の丸めで接する場合を落とさないよう、区間を僅かに広げる。
		const Toolbox::f64 Slack = (Toolbox::Abs(First) + Toolbox::Abs(Last)) * 1e-12 + 1e-12;
		Low = Toolbox::Max(Low, First - Slack);
		High = Toolbox::Min(High, Last + Slack);
		if (Low > High)
		{
			return false;
		}
	}
	return true;
}
} // namespace Dxf::PhysicsPrivate
#endif
