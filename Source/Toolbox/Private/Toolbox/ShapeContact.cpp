// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/ShapeContactQuery2D.h"
#include "Toolbox/ShapeContactQuery3D.h"
namespace Toolbox
{
namespace
{
// 方向を採用する距離の下限の、計算に使った値の規模に対する比（2^-30）。SweepToCenterの法線と同じ比。
constexpr f64 DirectionRatio_Internal = 1.0 / 1073741824.0;
// 2D／3D共通の接触結果。法線は単位方向（f64）。
struct FContactCore_Internal
{
	// 表面間の符号付き距離。
	f64 Separation = 0;
	// Normalが有効な方向か。
	bool bNormal = false;
	// 分離が最も速く増える単位方向。
	f64 Normal[3]{};
};
// 3成分の絶対値の最大。
f64 MaxAbs_Internal(const f64 (&Value)[3]) noexcept
{
	return Max(Abs(Value[0]), Max(Abs(Value[1]), Abs(Value[2])));
}
// 最大成分で割ってから求める長さ（平方の桁あふれ・消失を避ける）。
f64 Length_Internal(const f64 (&Value)[3]) noexcept
{
	const f64 Scale = MaxAbs_Internal(Value);
	if (Scale == 0)
	{
		return 0;
	}
	const f64 X = Value[0] / Scale;
	const f64 Y = Value[1] / Scale;
	const f64 Z = Value[2] / Scale;
	return Scale * Sqrt(X * X + Y * Y + Z * Z);
}
// 3成分の内積。
f64 Dot_Internal(const f64 (&A)[3], const f64 (&B)[3]) noexcept
{
	return A[0] * B[0] + A[1] * B[1] + A[2] * B[2];
}
// 長さLengthで割って単位方向にする。
void Normalize_Internal(const f64 (&Value)[3], f64 Length, f64 (&Out)[3]) noexcept
{
	Out[0] = Value[0] / Length;
	Out[1] = Value[1] / Length;
	Out[2] = Value[2] / Length;
}
// 円／球同士。Deltaは対象の中心から調べる形状の中心への差、RadiusSumは半径の和。
FContactCore_Internal BallContact_Internal(const f64 (&Delta)[3], f64 RadiusSum)
{
	FContactCore_Internal Core;
	const f64 Distance = Length_Internal(Delta);
	Core.Separation = Distance - RadiusSum;
	const f64 Scale = Max(MaxAbs_Internal(Delta), RadiusSum);
	if (Distance > Scale * DirectionRatio_Internal)
	{
		Normalize_Internal(Delta, Distance, Core.Normal);
		Core.bNormal = true;
	}
	return Core;
}
// 自由な軸の正規方程式 G q = A^T p を部分ピボットの消去法で解く。Gは実際の軸の内積。特異ならfalse。
bool SolveFree_Internal(const f64 (&Axes)[3][3], const int32 (&Free)[3], int32 FreeCount, const f64 (&Point)[3],
                        f64 (&Coordinates)[3]) noexcept
{
	f64 Matrix[3][4]{};
	for (int32 Row = 0; Row < FreeCount; ++Row)
	{
		const f64(&A)[3] = Axes[Free[Row]];
		for (int32 Column = 0; Column < FreeCount; ++Column)
		{
			Matrix[Row][Column] = Dot_Internal(A, Axes[Free[Column]]);
		}
		Matrix[Row][FreeCount] = Dot_Internal(A, Point);
	}
	for (int32 Column = 0; Column < FreeCount; ++Column)
	{
		int32 Pivot = Column;
		for (int32 Row = Column + 1; Row < FreeCount; ++Row)
		{
			if (Abs(Matrix[Row][Column]) > Abs(Matrix[Pivot][Column]))
			{
				Pivot = Row;
			}
		}
		if (!(Abs(Matrix[Pivot][Column]) > 0))
		{
			return false;
		}
		for (int32 K = 0; K <= FreeCount; ++K)
		{
			Swap(Matrix[Pivot][K], Matrix[Column][K]);
		}
		for (int32 Row = 0; Row < FreeCount; ++Row)
		{
			if (Row == Column)
			{
				continue;
			}
			const f64 Factor = Matrix[Row][Column] / Matrix[Column][Column];
			for (int32 K = Column; K <= FreeCount; ++K)
			{
				Matrix[Row][K] -= Factor * Matrix[Column][K];
			}
		}
	}
	for (int32 Row = 0; Row < FreeCount; ++Row)
	{
		Coordinates[Row] = Matrix[Row][FreeCount] / Matrix[Row][Row];
	}
	return true;
}
// 実際の軸を持つ箱。Deltaは箱の中心から調べる形状の中心への差、Dimensionは使う軸の数（2または3）。
FContactCore_Internal BoxContact_Internal(const f64 (&Delta)[3], const f64 (&Axes)[3][3], const f64 (&Half)[3],
                                          int32 Dimension, f64 Radius)
{
	// 各軸を「自由・下限・上限」に分けた全状態（内部を含む）で、範囲内の最近点を探す。
	int32 States = 1;
	for (int32 Axis = 0; Axis < Dimension; ++Axis)
	{
		States *= 3;
	}
	f64 BestSquared = -1;
	f64 BestResidual[3]{};
	for (int32 State = 0; State < States; ++State)
	{
		int32 Free[3]{};
		int32 FreeCount = 0;
		f64 Residual[3] = {Delta[0], Delta[1], Delta[2]};
		int32 Code = State;
		for (int32 Axis = 0; Axis < Dimension; ++Axis)
		{
			const int32 Mode = Code % 3;
			Code /= 3;
			if (Mode == 0)
			{
				Free[FreeCount++] = Axis;
				continue;
			}
			const f64 Value = Mode == 1 ? -Half[Axis] : Half[Axis];
			for (int32 Component = 0; Component < 3; ++Component)
			{
				Residual[Component] -= Value * Axes[Axis][Component];
			}
		}
		f64 Coordinates[3]{};
		if (FreeCount > 0 && !SolveFree_Internal(Axes, Free, FreeCount, Residual, Coordinates))
		{
			continue;
		}
		bool bInside = true;
		for (int32 Index = 0; Index < FreeCount; ++Index)
		{
			const f64 Extent = Half[Free[Index]];
			bInside = bInside && Coordinates[Index] >= -Extent && Coordinates[Index] <= Extent;
		}
		if (!bInside)
		{
			continue;
		}
		for (int32 Index = 0; Index < FreeCount; ++Index)
		{
			for (int32 Component = 0; Component < 3; ++Component)
			{
				Residual[Component] -= Coordinates[Index] * Axes[Free[Index]][Component];
			}
		}
		const f64 Squared = Dot_Internal(Residual, Residual);
		if (BestSquared < 0 || Squared < BestSquared)
		{
			BestSquared = Squared;
			BestResidual[0] = Residual[0];
			BestResidual[1] = Residual[1];
			BestResidual[2] = Residual[2];
		}
	}
	if (BestSquared < 0)
	{
		throw FException("Degenerate box axes in shape contact");
	}
	FContactCore_Internal Core;
	f64 Scale = Max(MaxAbs_Internal(Delta), Radius);
	for (int32 Axis = 0; Axis < Dimension; ++Axis)
	{
		Scale = Max(Scale, Half[Axis] * MaxAbs_Internal(Axes[Axis]));
	}
	const f64 Distance = Length_Internal(BestResidual);
	if (Distance > Scale * DirectionRatio_Internal)
	{
		// 中心は箱の外。最近点から中心への方向。
		Core.Separation = Distance - Radius;
		Normalize_Internal(BestResidual, Distance, Core.Normal);
		Core.bNormal = true;
		return Core;
	}
	// 中心は箱の内部（または丸め誤差の範囲で境界上）。各面の外向き法線と、そこまでの距離を比べる。
	const int32 AllFree[3] = {0, 1, 2};
	f64 Local[3]{};
	if (!SolveFree_Internal(Axes, AllFree, Dimension, Delta, Local))
	{
		throw FException("Degenerate box axes in shape contact");
	}
	f64 FaceNormals[3][3]{};
	if (Dimension == 2)
	{
		// 2Dの辺の法線は、もう一方の軸に直交する方向。
		const f64 Perp0[3] = {Axes[1][1], -Axes[1][0], 0};
		const f64 Perp1[3] = {-Axes[0][1], Axes[0][0], 0};
		for (int32 Component = 0; Component < 3; ++Component)
		{
			FaceNormals[0][Component] = Perp0[Component];
			FaceNormals[1][Component] = Perp1[Component];
		}
	}
	else
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const f64(&A)[3] = Axes[(Axis + 1) % 3];
			const f64(&B)[3] = Axes[(Axis + 2) % 3];
			FaceNormals[Axis][0] = A[1] * B[2] - A[2] * B[1];
			FaceNormals[Axis][1] = A[2] * B[0] - A[0] * B[2];
			FaceNormals[Axis][2] = A[0] * B[1] - A[1] * B[0];
		}
	}
	f64 BestExit = -1;
	f64 BestNormal[3]{};
	bool bTie = false;
	for (int32 Axis = 0; Axis < Dimension; ++Axis)
	{
		f64 Unit[3]{};
		const f64 Length = Length_Internal(FaceNormals[Axis]);
		if (!(Length > 0))
		{
			throw FException("Degenerate box axes in shape contact");
		}
		Normalize_Internal(FaceNormals[Axis], Length, Unit);
		f64 Width = Dot_Internal(Axes[Axis], Unit);
		if (Width < 0)
		{
			Width = -Width;
			for (int32 Component = 0; Component < 3; ++Component)
			{
				Unit[Component] = -Unit[Component];
			}
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			// 上限側は+Unit、下限側は-Unitが外向き。丸めで負になった距離は0とみなす。
			const f64 Exit = Max(0.0, (Half[Axis] + (Side == 0 ? -Local[Axis] : Local[Axis])) * Width);
			if (BestExit < 0 || Exit < BestExit)
			{
				BestExit = Exit;
				bTie = false;
				for (int32 Component = 0; Component < 3; ++Component)
				{
					BestNormal[Component] = Side == 0 ? Unit[Component] : -Unit[Component];
				}
			}
			else if (Exit == BestExit)
			{
				bTie = true;
			}
		}
	}
	Core.Separation = -(BestExit + Radius);
	if (!bTie)
	{
		Core.Normal[0] = BestNormal[0];
		Core.Normal[1] = BestNormal[1];
		Core.Normal[2] = BestNormal[2];
		Core.bNormal = true;
	}
	return Core;
}
// 共通の結果を2Dの公開型へ変える。
FShapeContact2D To2D_Internal(const FContactCore_Internal& Core)
{
	FShapeContact2D Result;
	Result.Separation = Core.Separation;
	if (Core.bNormal)
	{
		Result.Normal = FVector2{static_cast<f32>(Core.Normal[0]), static_cast<f32>(Core.Normal[1])};
	}
	return Result;
}
// 共通の結果を3Dの公開型へ変える。
FShapeContact3D To3D_Internal(const FContactCore_Internal& Core)
{
	FShapeContact3D Result;
	Result.Separation = Core.Separation;
	if (Core.bNormal)
	{
		Result.Normal = FVector3{static_cast<f32>(Core.Normal[0]), static_cast<f32>(Core.Normal[1]),
		                         static_cast<f32>(Core.Normal[2])};
	}
	return Result;
}
// 球の座標と半径を検査する。
void ValidateSphere_Internal(const FSphere& Sphere)
{
	if (!Sphere.Center.IsValid() || !IsFinite(Sphere.Radius) || Sphere.Radius < 0)
	{
		throw FException("Invalid shape contact sphere");
	}
}
} // namespace
FShapeContact2D FindShapeContact(const FCircle2D& Shape, const FCircle2D& Target)
{
	if (!IsValid(Shape) || !IsValid(Target))
	{
		throw FException("Invalid shape contact circle");
	}
	const f64 Delta[3] = {f64(Shape.Center.X) - Target.Center.X, f64(Shape.Center.Y) - Target.Center.Y, 0};
	return To2D_Internal(BallContact_Internal(Delta, f64(Shape.Radius) + f64(Target.Radius)));
}
FShapeContact2D FindShapeContact(const FCircle2D& Shape, const FOrientedBox2D& Target)
{
	if (!IsValid(Shape) || !IsValid(Target))
	{
		throw FException("Invalid shape contact rectangle");
	}
	// SweepToCenterと同じ反時計回りの軸。f64で作るため直交単位軸になる。
	const f64 Cosine = Cos(f64(Target.Angle));
	const f64 Sine = Sin(f64(Target.Angle));
	const f64 Axes[3][3] = {{Cosine, Sine, 0}, {-Sine, Cosine, 0}, {0, 0, 1}};
	const f64 Half[3] = {Target.HalfExtents.X, Target.HalfExtents.Y, 0};
	const f64 Delta[3] = {f64(Shape.Center.X) - Target.Center.X, f64(Shape.Center.Y) - Target.Center.Y, 0};
	return To2D_Internal(BoxContact_Internal(Delta, Axes, Half, 2, Shape.Radius));
}
FShapeContact3D FindShapeContact(const FSphere& Shape, const FSphere& Target)
{
	ValidateSphere_Internal(Shape);
	ValidateSphere_Internal(Target);
	const f64 Delta[3] = {f64(Shape.Center.X) - Target.Center.X, f64(Shape.Center.Y) - Target.Center.Y,
	                      f64(Shape.Center.Z) - Target.Center.Z};
	return To3D_Internal(BallContact_Internal(Delta, f64(Shape.Radius) + f64(Target.Radius)));
}
FShapeContact3D FindShapeContact(const FSphere& Shape, const FOBB& Target)
{
	ValidateSphere_Internal(Shape);
	// IntersectsSphereと同じOBBの条件（有限の中心、非負の半幅、有効な軸）。
	if (!IsValid(FCollisionShape{Target}))
	{
		throw FException("Invalid shape contact OBB");
	}
	f64 Axes[3][3]{};
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (int32 Component = 0; Component < 3; ++Component)
		{
			Axes[Axis][Component] = Target.Axes[static_cast<size_t>(Axis)].Component(Component);
		}
	}
	const f64 Half[3] = {Target.HalfExtents.X, Target.HalfExtents.Y, Target.HalfExtents.Z};
	const f64 Delta[3] = {f64(Shape.Center.X) - Target.Center.X, f64(Shape.Center.Y) - Target.Center.Y,
	                      f64(Shape.Center.Z) - Target.Center.Z};
	return To3D_Internal(BoxContact_Internal(Delta, Axes, Half, 3, Shape.Radius));
}
} // namespace Toolbox
