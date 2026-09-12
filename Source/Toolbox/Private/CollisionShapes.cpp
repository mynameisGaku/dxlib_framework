// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/CollisionShapes.h"
namespace Toolbox
{
bool FAABB::IsValid() const noexcept
{
	return Min.IsValid() && Max.IsValid() && Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z;
}
bool FAABB::Contains(const FAABB& Other) const noexcept
{
	return Min.X <= Other.Min.X && Min.Y <= Other.Min.Y && Min.Z <= Other.Min.Z && Max.X >= Other.Max.X &&
	       Max.Y >= Other.Max.Y && Max.Z >= Other.Max.Z;
}
bool FAABB::Intersects(const FAABB& Other) const noexcept
{
#if TOOLBOX_SIMD_SSE2
	// 左側の箱の最小座標レーン。
	const __m128 AMin = _mm_set_ps(0, Min.Z, Min.Y, Min.X);
	// 左側の箱の最大座標レーン。
	const __m128 AMax = _mm_set_ps(0, Max.Z, Max.Y, Max.X);
	// 右側の箱の最小座標レーン。
	const __m128 BMin = _mm_set_ps(0, Other.Min.Z, Other.Min.Y, Other.Min.X);
	// 右側の箱の最大座標レーン。
	const __m128 BMax = _mm_set_ps(0, Other.Max.Z, Other.Max.Y, Other.Max.X);
	return (_mm_movemask_ps(_mm_and_ps(_mm_cmple_ps(AMin, BMax), _mm_cmple_ps(BMin, AMax))) & 7) == 7;
#else
	return Min.X <= Other.Max.X && Max.X >= Other.Min.X && Min.Y <= Other.Max.Y && Max.Y >= Other.Min.Y &&
	       Min.Z <= Other.Max.Z && Max.Z >= Other.Min.Z;
#endif
}
namespace
{
// 箱の回転軸が直交する単位ベクトルか調べる。
bool ValidAxes(const TArray<FVector3, 3>& Axes) noexcept
{
	for (const auto& Axis : Axes)
	{
		if (!Axis.IsValid() || Abs(LengthSquared(Axis) - 1) > 1e-4f)
		{
			return false;
		}
	}
	return Abs(Dot(Axes[0], Axes[1])) < 1e-4f && Abs(Dot(Axes[0], Axes[2])) < 1e-4f &&
	       Abs(Dot(Axes[1], Axes[2])) < 1e-4f;
}
// 点集合が空でなく、有限な座標だけを持つか調べる。
bool ValidVertices(const TVector<FVector3>& Vertices) noexcept
{
	if (Vertices.IsEmpty())
	{
		return false;
	}
	for (const auto& Point : Vertices)
	{
		if (!Point.IsValid())
		{
			return false;
		}
	}
	return true;
}
// 指定方向へ最も遠い頂点を選ぶ。
FVector3 SupportVertices(const TVector<FVector3>& Vertices, FVector3 Direction)
{
	// 現在の最適な支持点。
	FVector3 Best = Vertices[0];
	// 現在の支持点の方向への射影。
	f32 Score = Dot(Best, Direction);
	for (size_t I = 1; I < Vertices.Size(); ++I)
	{
		// 候補頂点の方向への射影。
		const f32 Candidate = Dot(Vertices[I], Direction);
		if (Candidate > Score)
		{
			Best = Vertices[I];
			Score = Candidate;
		}
	}
	return Best;
}
// OBBの指定方向側の角を求める。
FVector3 SupportBox(const FOBB& Box, FVector3 Direction)
{
	// 計算または検索の結果。
	FVector3 Result = Box.Center;
	for (int32 I = 0; I < 3; ++I)
	{
		Result += Box.Axes[static_cast<size_t>(I)] * (Dot(Box.Axes[static_cast<size_t>(I)], Direction) >= 0
		                                                  ? Box.HalfExtents.Component(I)
		                                                  : -Box.HalfExtents.Component(I));
	}
	return Result;
}
// 凸形状の支持点を求める。Meshは三角形へ分解してから呼ぶ。
FVector3 Support(const FCollisionShape& Shape, FVector3 Direction)
{
	return Visit(
	    [&](const auto& Value) -> FVector3
	    {
		    using T = TDecay<decltype(Value)>;
		    if constexpr (IsSame<T, FAABB>)
		    {
			    return {Direction.X >= 0 ? Value.Max.X : Value.Min.X, Direction.Y >= 0 ? Value.Max.Y : Value.Min.Y,
			            Direction.Z >= 0 ? Value.Max.Z : Value.Min.Z};
		    }
		    else if constexpr (IsSame<T, FSphere>)
		    {
			    return Value.Center + Normalize(Direction) * Value.Radius;
		    }
		    else if constexpr (IsSame<T, FOBB>)
		    {
			    return SupportBox(Value, Direction);
		    }
		    else if constexpr (IsSame<T, FCube>)
		    {
			    return SupportBox({Value.Center, {Value.HalfExtent, Value.HalfExtent, Value.HalfExtent}, Value.Axes},
			                      Direction);
		    }
		    else
		    {
			    if constexpr (IsSame<T, FMesh>)
			    {
				    return SupportVertices(Value.GetVertices(), Direction);
			    }
			    else
			    {
				    return SupportVertices(Value.Vertices, Direction);
			    }
		    }
	    },
	    Shape);
}
// 単体上で原点に最も近い点を求め、有効な頂点だけを残す。
FVector3 ReduceSimplex(TVector<FVector3>& Simplex)
{
	// 単体上で見つかった最小距離の二乗。
	f64 BestDistance = DBL_MAX;
	// 最も近い部分単体を示す頂点ビット。
	uint32 BestMask = 1;
	// 現在の最適な支持点。
	FVector3 Best = Simplex[0];
	// 探索する頂点部分集合の上限。
	const uint32 Limit = uint32(1) << static_cast<uint32>(Simplex.Size());
	for (uint32 Mask = 1; Mask < Limit; ++Mask)
	{
		// 部分単体を構成する最大四頂点。
		FVector3 Points[4];
		// 処理対象の要素数。
		int32 Count = 0;
		for (size_t I = 0; I < Simplex.Size(); ++I)
		{
			if (Mask & (uint32(1) << static_cast<uint32>(I)))
			{
				Points[Count++] = Simplex[I];
			}
		}
		// 原点への射影を表す重心座標。
		f64 Weights[4]{1, 0, 0, 0};
		// 現在の候補が数値条件を満たすか。
		bool Valid = true;
		if (Count > 1)
		{
			// 射影条件を解く拡大係数行列。
			f64 Matrix[3][4]{};
			// 単体の独立した辺の数。
			const int32 Dimension = Count - 1;
			for (int32 Row = 0; Row < Dimension; ++Row)
			{
				// 基準頂点から伸びる辺。
				const FVector3 Edge = Points[Row + 1] - Points[0];
				for (int32 Column = 0; Column < Dimension; ++Column)
				{
					// 比較対象となる値。
					const FVector3 Other = Points[Column + 1] - Points[0];
					Matrix[Row][Column] = f64(Edge.X) * Other.X + f64(Edge.Y) * Other.Y + f64(Edge.Z) * Other.Z;
				}
				Matrix[Row][Dimension] =
				    -(f64(Edge.X) * Points[0].X + f64(Edge.Y) * Points[0].Y + f64(Edge.Z) * Points[0].Z);
			}
			for (int32 Column = 0; Column < Dimension; ++Column)
			{
				// 絶対値が最大のピボット行。
				int32 Pivot = Column;
				for (int32 Row = Column + 1; Row < Dimension; ++Row)
				{
					if (Abs(Matrix[Row][Column]) > Abs(Matrix[Pivot][Column]))
					{
						Pivot = Row;
					}
				}
				if (Abs(Matrix[Pivot][Column]) < 1e-24)
				{
					Valid = false;
					break;
				}
				for (int32 K = 0; K <= Dimension; ++K)
				{
					Swap(Matrix[Pivot][K], Matrix[Column][K]);
				}
				// 行を正規化する除数。
				const f64 Divisor = Matrix[Column][Column];
				for (int32 K = Column; K <= Dimension; ++K)
				{
					Matrix[Column][K] /= Divisor;
				}
				for (int32 Row = 0; Row < Dimension; ++Row)
				{
					if (Row == Column)
					{
						continue;
					}
					// 他行から消去する成分の倍率。
					const f64 Factor = Matrix[Row][Column];
					for (int32 K = Column; K <= Dimension; ++K)
					{
						Matrix[Row][K] -= Factor * Matrix[Column][K];
					}
				}
			}
			if (!Valid)
			{
				continue;
			}
			for (int32 I = 1; I < Count; ++I)
			{
				Weights[I] = Matrix[I - 1][Dimension];
				Weights[0] -= Weights[I];
			}
		}
		for (int32 I = 0; I < Count; ++I)
		{
			if (Weights[I] < -1e-10)
			{
				Valid = false;
			}
		}
		if (!Valid)
		{
			continue;
		}
		// 重心座標で合成するX座標。
		f64 X = 0;
		// 重心座標で合成するY座標。
		f64 Y = 0;
		// 重心座標で合成するZ座標。
		f64 Z = 0;
		for (int32 I = 0; I < Count; ++I)
		{
			X += Points[I].X * Weights[I];
			Y += Points[I].Y * Weights[I];
			Z += Points[I].Z * Weights[I];
		}
		// 原点への距離。
		const f64 Distance = X * X + Y * Y + Z * Z;
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestMask = Mask;
			Best = {static_cast<f32>(X), static_cast<f32>(Y), static_cast<f32>(Z)};
		}
	}
	// 不要な頂点を除いた単体。
	TVector<FVector3> Reduced;
	Reduced.Reserve(4);
	for (size_t I = 0; I < Simplex.Size(); ++I)
	{
		if (BestMask & (uint32(1) << static_cast<uint32>(I)))
		{
			Reduced.PushBack(Simplex[I]);
		}
	}
	Simplex = Move(Reduced);
	return Best;
}
// 支持点を用いたGJK距離判定。縮退した線分・三角形も単体の部分集合で扱う。
bool ConvexIntersects(const FCollisionShape& A, const FCollisionShape& B, f32 Tolerance)
{
	// ミンコフスキー差の支持点を取得する処理。
	auto Point = [&](FVector3 Direction)
	{
		return Support(A, Direction) - Support(B, -Direction);
	};
	// 原点に近づける支持点の集合。
	TVector<FVector3> Simplex;
	Simplex.Reserve(4);
	Simplex.PushBack(Point({1, 0, 0}));
	// 現在の単体で原点に最も近い点。
	FVector3 Closest = Simplex[0];
	for (int32 Iteration = 0; Iteration < 96; ++Iteration)
	{
		// 原点への距離。
		const f32 Distance = Length(Closest);
		if (Distance <= Tolerance)
		{
			return true;
		}
		// 次に追加する支持点または子ノード番号。
		const FVector3 Next = Point(-Closest);
		if (Dot(Next, -Closest) < -Tolerance * Distance)
		{
			return false;
		}
		// 既存の支持点と重複しているか。
		bool Duplicate = false;
		for (FVector3 Existing : Simplex)
		{
			if (LengthSquared(Next - Existing) <= Tolerance * Tolerance * 0.01f)
			{
				Duplicate = true;
			}
		}
		if (Duplicate || Simplex.Size() == 4)
		{
			return Distance <= Tolerance;
		}
		Simplex.PushBack(Next);
		Closest = ReduceSimplex(Simplex);
	}
	return Length(Closest) <= Tolerance;
}
} // namespace
bool IsValid(const FCollisionShape& Shape) noexcept
{
	return Visit(
	    [](const auto& Value)
	    {
		    using T = TDecay<decltype(Value)>;
		    if constexpr (IsSame<T, FAABB>)
		    {
			    return Value.IsValid();
		    }
		    else if constexpr (IsSame<T, FSphere>)
		    {
			    return Value.Center.IsValid() && IsFinite(Value.Radius) && Value.Radius >= 0;
		    }
		    else if constexpr (IsSame<T, FOBB>)
		    {
			    return Value.Center.IsValid() && Value.HalfExtents.IsValid() && Value.HalfExtents.X >= 0 &&
			           Value.HalfExtents.Y >= 0 && Value.HalfExtents.Z >= 0 && ValidAxes(Value.Axes);
		    }
		    else if constexpr (IsSame<T, FCube>)
		    {
			    return Value.Center.IsValid() && IsFinite(Value.HalfExtent) && Value.HalfExtent >= 0 &&
			           ValidAxes(Value.Axes);
		    }
		    else if constexpr (IsSame<T, FConvex>)
		    {
			    return ValidVertices(Value.Vertices);
		    }
		    else
		    {
			    return Value.IsValid();
		    }
	    },
	    Shape);
}
FAABB Bounds(const FCollisionShape& Shape)
{
	if (!IsValid(Shape))
	{
		throw FException("Invalid collision shape");
	}
	if (HoldsAlternative<FMesh>(Shape))
	{
		return Get<FMesh>(Shape).GetBounds();
	}
	// 各軸の最小支持座標。
	const FVector3 Low{Support(Shape, {-1, 0, 0}).X, Support(Shape, {0, -1, 0}).Y, Support(Shape, {0, 0, -1}).Z};
	// 各軸の最大支持座標。
	const FVector3 High{Support(Shape, {1, 0, 0}).X, Support(Shape, {0, 1, 0}).Y, Support(Shape, {0, 0, 1}).Z};
	// 計算または検索の結果。
	const FAABB Result{Low, High};
	if (!Result.IsValid())
	{
		throw FException("Collision bounds overflow");
	}
	return Result;
}
bool Intersects(const FCollisionShape& A, const FCollisionShape& B, f32 Tolerance)
{
	if (!IsFinite(Tolerance) || Tolerance <= 0)
	{
		throw FException("Invalid collision tolerance");
	}
	// 形状またはノードを囲む境界箱。
	FAABB Box = Bounds(A);
	// 接触許容誤差を各軸へ広げた幅。
	const FVector3 Margin{Tolerance, Tolerance, Tolerance};
	Box.Min = Box.Min - Margin;
	Box.Max += Margin;
	if (!Box.Intersects(Bounds(B)))
	{
		return false;
	}
	if (HoldsAlternative<FMesh>(A))
	{
		// 三角形ごとに詳細判定するメッシュ。
		const auto& Mesh = Get<FMesh>(A);
		// 三角形候補を検索する境界箱。
		FAABB Search = Bounds(B);
		Search.Min = Search.Min - Margin;
		Search.Max += Margin;
		// 境界箱の比較を通過した候補。
		const auto Candidates = Mesh.QueryTriangles_Internal(Search);
		// 三角形を構成する頂点番号。
		const auto& Indices = Mesh.GetIndices();
		// メッシュが所有する頂点座標。
		const auto& Vertices = Mesh.GetVertices();
		for (size_t TriangleIndex : Candidates)
		{
			// 三角形の先頭頂点番号がある位置。
			const size_t I = TriangleIndex * 3;
			// 現在の三角形を表す判定データ。
			FCollisionShape Triangle =
			    FConvex{{Vertices[Indices[I]], Vertices[Indices[I + 1]], Vertices[Indices[I + 2]]}};
			if (Intersects(Triangle, B, Tolerance))
			{
				return true;
			}
		}
		return false;
	}
	if (HoldsAlternative<FMesh>(B))
	{
		return Intersects(B, A, Tolerance);
	}
	if (HoldsAlternative<FSphere>(A) && HoldsAlternative<FSphere>(B))
	{
		// 演算の左側に使用する値。
		const auto& Left = Get<FSphere>(A);
		// 演算の右側に使用する値。
		const auto& Right = Get<FSphere>(B);
		// 有限な単精度座標でも差や半径の和が上限を超えるため、差を取る前に倍精度へ広げる。
		const f64 DeltaX = static_cast<f64>(Left.Center.X) - Right.Center.X;
		const f64 DeltaY = static_cast<f64>(Left.Center.Y) - Right.Center.Y;
		const f64 DeltaZ = static_cast<f64>(Left.Center.Z) - Right.Center.Z;
		// 二球が接触と見なされる中心間距離の上限。
		const f64 RadiusSum = static_cast<f64>(Left.Radius) + Right.Radius + Tolerance;
		return DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ <= RadiusSum * RadiusSum;
	}
	return ConvexIntersects(A, B, Tolerance);
}
} // namespace Toolbox
