// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/ContinuousCollision.h"
#include "Toolbox/ShapeSweep2D.h"
#include "Toolbox/ShapeSweep3D.h"
namespace Toolbox
{
namespace
{
// f64の内部演算から、範囲の狭い方向成分だけを公開型へ戻す。
FVector3 Normal_Internal(f64 X, f64 Y, f64 Z) noexcept
{
	const f64 Scale = Max(Abs(X), Max(Abs(Y), Abs(Z)));
	if (Scale == 0)
	{
		return {1, 0, 0};
	}
	X /= Scale;
	Y /= Scale;
	Z /= Scale;
	const f64 Length = Sqrt(X * X + Y * Y + Z * Z);
	return {static_cast<f32>(X / Length), static_cast<f32>(Y / Length), static_cast<f32>(Z / Length)};
}
// 相対座標にある点が原点中心の球へ入る時刻。入力成分はf32の和差の範囲に収まる。
FSweepHit3D PointBallEntry_Internal(const f64 (&P)[3], const f64 (&V)[3], f64 Radius)
{
	FSweepHit3D Result;
	const f64 StartSquared = P[0] * P[0] + P[1] * P[1] + P[2] * P[2];
	const f64 RadiusSquared = Radius * Radius;
	if (StartSquared <= RadiusSquared)
	{
		Result.bHit = true;
		Result.bInitialContact = true;
		Result.Time = 0;
		Result.Normal = StartSquared > 0 ? Normal_Internal(P[0], P[1], P[2]) : Normal_Internal(-V[0], -V[1], -V[2]);
		return Result;
	}
	const f64 SpeedSquared = V[0] * V[0] + V[1] * V[1] + V[2] * V[2];
	const f64 Projection = P[0] * V[0] + P[1] * V[1] + P[2] * V[2];
	if (SpeedSquared == 0 || Projection >= 0)
	{
		return Result;
	}
	// b*b-a*cでは長距離の移動で小さな半径が消えるため、外積による判別式を使う。
	const f64 CrossX = P[1] * V[2] - P[2] * V[1];
	const f64 CrossY = P[2] * V[0] - P[0] * V[2];
	const f64 CrossZ = P[0] * V[1] - P[1] * V[0];
	const f64 Discriminant = RadiusSquared * SpeedSquared - (CrossX * CrossX + CrossY * CrossY + CrossZ * CrossZ);
	if (Discriminant < 0)
	{
		return Result;
	}
	const f64 Root = Sqrt(Discriminant);
	// 小さい根の減算を避ける同値な式。区間の初期貫通は上で除外済み。
	const f64 Time = (StartSquared - RadiusSquared) / (-Projection + Root);
	if (Time < 0 || Time > 1)
	{
		return Result;
	}
	Result.bHit = true;
	Result.Time = Time;
	// 接触時刻だけから巨大な位置を引くと半径が消えるため、最近点と進入距離から法線を復元する。
	const f64 ClosestTime = -Projection / SpeedSquared;
	const f64 OffsetTime = Root / SpeedSquared;
	const f64 X = (P[0] + V[0] * ClosestTime) - V[0] * OffsetTime;
	const f64 Y = (P[1] + V[1] * ClosestTime) - V[1] * OffsetTime;
	const f64 Z = (P[2] + V[2] * ClosestTime) - V[2] * OffsetTime;
	Result.Normal = X == 0 && Y == 0 && Z == 0 ? Normal_Internal(-V[0], -V[1], -V[2]) : Normal_Internal(X, Y, Z);
	return Result;
}
// 球と変位に対する共通の事前条件。
void Validate_Internal(const FSphere& Sphere, FVector3 Move, f32 Tolerance)
{
	if (!Sphere.Center.IsValid() || !IsFinite(Sphere.Radius) || Sphere.Radius < 0 || !Move.IsValid() ||
	    !IsFinite(Tolerance) || Tolerance < 0)
	{
		throw FException("Invalid continuous collision input");
	}
}
// 処理済みの3D結果からXY成分を取り出す。
FORCEINLINE FSweepHit2D ToPlanar_Internal(const FSweepHit3D& Hit) noexcept
{
	return {Hit.bHit, Hit.bInitialContact, Hit.Time, {Hit.Normal.X, Hit.Normal.Y}};
}
} // namespace
FSweepHit3D Sweep(const FSphere& A, FVector3 DisplacementA, const FSphere& B, FVector3 DisplacementB, f32 Tolerance)
{
	Validate_Internal(A, DisplacementA, Tolerance);
	Validate_Internal(B, DisplacementB, Tolerance);
	const f64 P[3] = {f64(A.Center.X) - B.Center.X, f64(A.Center.Y) - B.Center.Y, f64(A.Center.Z) - B.Center.Z};
	const f64 V[3] = {f64(DisplacementA.X) - DisplacementB.X, f64(DisplacementA.Y) - DisplacementB.Y,
	                  f64(DisplacementA.Z) - DisplacementB.Z};
	return PointBallEntry_Internal(P, V, f64(A.Radius) + B.Radius + Tolerance);
}
FSweepHit3D Sweep(const FSphere& Sphere, FVector3 DisplacementSphere, const FAABB& Box, FVector3 DisplacementBox,
                  f32 Tolerance)
{
	Validate_Internal(Sphere, DisplacementSphere, Tolerance);
	if (!Box.IsValid() || !DisplacementBox.IsValid())
	{
		throw FException("Invalid swept box");
	}
	// 箱の最小点を原点にして、大きな共通オフセットを演算から外す。
	const f64 P[3] = {f64(Sphere.Center.X) - Box.Min.X, f64(Sphere.Center.Y) - Box.Min.Y,
	                  f64(Sphere.Center.Z) - Box.Min.Z};
	const f64 V[3] = {f64(DisplacementSphere.X) - DisplacementBox.X, f64(DisplacementSphere.Y) - DisplacementBox.Y,
	                  f64(DisplacementSphere.Z) - DisplacementBox.Z};
	const f64 Extents[3] = {f64(Box.Max.X) - Box.Min.X, f64(Box.Max.Y) - Box.Min.Y, f64(Box.Max.Z) - Box.Min.Z};
	const f64 Radius = f64(Sphere.Radius) + Tolerance;
	// 最近点の式が切り替わる時刻。両端と六つの面の通過時刻で最大八個。
	f64 Times[8] = {0, 1};
	int32 Count = 2;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (V[Axis] == 0)
		{
			continue;
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const f64 Time = ((Side == 0 ? 0 : Extents[Axis]) - P[Axis]) / V[Axis];
			if (Time > 0 && Time < 1)
			{
				Times[Count++] = Time;
			}
		}
	}
	for (int32 Index = 1; Index < Count; ++Index)
	{
		const f64 Value = Times[Index];
		int32 Position = Index;
		while (Position > 0 && Times[Position - 1] > Value)
		{
			Times[Position] = Times[Position - 1];
			--Position;
		}
		Times[Position] = Value;
	}
	for (int32 Index = 0; Index + 1 < Count; ++Index)
	{
		const f64 Begin = Times[Index];
		const f64 Duration = Times[Index + 1] - Begin;
		if (Duration == 0)
		{
			continue;
		}
		const f64 Middle = Begin + Duration * 0.5;
		f64 Distance[3]{};
		f64 Change[3]{};
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const f64 AtMiddle = P[Axis] + V[Axis] * Middle;
			if (AtMiddle < 0 || AtMiddle > Extents[Axis])
			{
				const f64 Boundary = AtMiddle < 0 ? 0 : Extents[Axis];
				Distance[Axis] = (P[Axis] - Boundary) + V[Axis] * Begin;
				Change[Axis] = V[Axis] * Duration;
			}
		}
		FSweepHit3D Hit = PointBallEntry_Internal(Distance, Change, Radius);
		if (Hit.bHit)
		{
			Hit.Time = Begin + Duration * Hit.Time;
			Hit.bInitialContact = Hit.Time == 0;
			return Hit;
		}
	}
	return {};
}
FSweepHit3D Sweep(const FAABB& Box, FVector3 DisplacementBox, const FSphere& Sphere, FVector3 DisplacementSphere,
                  f32 Tolerance)
{
	FSweepHit3D Hit = Sweep(Sphere, DisplacementSphere, Box, DisplacementBox, Tolerance);
	if (Hit.bHit)
	{
		Hit.Normal = -Hit.Normal;
	}
	return Hit;
}
FSweepHit2D Sweep(const FCircle2D& A, FVector2 DisplacementA, const FCircle2D& B, FVector2 DisplacementB, f32 Tolerance)
{
	return ToPlanar_Internal(
	    Sweep(FSphere{{A.Center.X, A.Center.Y, 0}, A.Radius}, {DisplacementA.X, DisplacementA.Y, 0},
	          FSphere{{B.Center.X, B.Center.Y, 0}, B.Radius}, {DisplacementB.X, DisplacementB.Y, 0}, Tolerance));
}
FSweepHit2D Sweep(const FCircle2D& Circle, FVector2 DisplacementCircle, const FAABB2D& Box, FVector2 DisplacementBox,
                  f32 Tolerance)
{
	return ToPlanar_Internal(Sweep(FSphere{{Circle.Center.X, Circle.Center.Y, 0}, Circle.Radius},
	                               {DisplacementCircle.X, DisplacementCircle.Y, 0},
	                               FAABB{{Box.Min.X, Box.Min.Y, 0}, {Box.Max.X, Box.Max.Y, 0}},
	                               {DisplacementBox.X, DisplacementBox.Y, 0}, Tolerance));
}
FSweepHit2D Sweep(const FAABB2D& Box, FVector2 DisplacementBox, const FCircle2D& Circle, FVector2 DisplacementCircle,
                  f32 Tolerance)
{
	return ToPlanar_Internal(Sweep(FAABB{{Box.Min.X, Box.Min.Y, 0}, {Box.Max.X, Box.Max.Y, 0}},
	                               {DisplacementBox.X, DisplacementBox.Y, 0},
	                               FSphere{{Circle.Center.X, Circle.Center.Y, 0}, Circle.Radius},
	                               {DisplacementCircle.X, DisplacementCircle.Y, 0}, Tolerance));
}
namespace
{
// 移動する円／球の中心・終点・半径と、f32で表現できる移動量かを検査し、開始位置と移動量をf64で返す。
void PrepareMove_Internal(const f64 (&Start)[3], const f64 (&End)[3], bool bValid, f32 Radius, f64 (&Move)[3])
{
	if (!bValid || !IsFinite(Radius) || Radius < 0)
	{
		throw FException("Invalid shape sweep input");
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Move[Axis] = End[Axis] - Start[Axis];
		if (!IsFinite(Move[Axis]) || Abs(Move[Axis]) > f64(TNumericLimits<f32>::Max()))
		{
			throw FException("Shape sweep movement is not representable");
		}
	}
}
// 実際の軸を持つ箱の面・辺・頂点（全軸が自由な内部以外の各状態）へ、点が半径以内に入る最初の割合。
// Positionは箱中心からの開始位置、Moveは移動量、Axes[i]は軸i、Dimensionは使う軸の数（2または3）。
// 各状態では、自由な軸が張る平面／直線への最短点の局所座標q(t)を求め、その有限範囲に収まる区間だけで判定する。
// 箱の点との距離なので偽の接触は生じず、外からの最初の接触は必ずいずれかの状態で最短点が範囲内になる。
TOptional<f64> BoxFeatureEntry_Internal(const f64 (&Position)[3], const f64 (&Move)[3], const f64 (&Axes)[3][3],
                                        const f64 (&Half)[3], int32 Dimension, f64 Radius)
{
	TOptional<f64> Best;
	int32 States = 1;
	for (int32 Axis = 0; Axis < Dimension; ++Axis)
	{
		States *= 3;
	}
	for (int32 State = 0; State < States; ++State)
	{
		// 各軸の状態（0: 自由、1: 下限、2: 上限）と、固定した軸の寄与を引いた位置。
		int32 Free[3]{};
		int32 FreeCount = 0;
		f64 Fixed[3] = {Position[0], Position[1], Position[2]};
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
				Fixed[Component] -= Value * Axes[Axis][Component];
			}
		}
		if (FreeCount == Dimension)
		{
			// 全軸が自由な状態は箱の内部。開始時の包含は呼出し側の初期接触判定が扱う。
			continue;
		}
		// 自由な軸の正規方程式 G q = A^T p。Gは実際の軸の内積（転置を逆行列の代わりにしない）。
		f64 Gram[2][2]{};
		f64 AtPosition[2]{};
		f64 AtMove[2]{};
		for (int32 Row = 0; Row < FreeCount; ++Row)
		{
			const f64(&A)[3] = Axes[Free[Row]];
			for (int32 Column = 0; Column < FreeCount; ++Column)
			{
				const f64(&B)[3] = Axes[Free[Column]];
				Gram[Row][Column] = A[0] * B[0] + A[1] * B[1] + A[2] * B[2];
			}
			AtPosition[Row] = A[0] * Fixed[0] + A[1] * Fixed[1] + A[2] * Fixed[2];
			AtMove[Row] = A[0] * Move[0] + A[1] * Move[1] + A[2] * Move[2];
		}
		f64 Q0[2]{};
		f64 Q1[2]{};
		if (FreeCount == 1)
		{
			if (!(Gram[0][0] > 0))
			{
				throw FException("Degenerate box axis in shape sweep");
			}
			Q0[0] = AtPosition[0] / Gram[0][0];
			Q1[0] = AtMove[0] / Gram[0][0];
		}
		else if (FreeCount == 2)
		{
			const f64 Determinant = Gram[0][0] * Gram[1][1] - Gram[0][1] * Gram[1][0];
			if (!(Determinant > 0))
			{
				throw FException("Degenerate box axes in shape sweep");
			}
			Q0[0] = (Gram[1][1] * AtPosition[0] - Gram[0][1] * AtPosition[1]) / Determinant;
			Q0[1] = (Gram[0][0] * AtPosition[1] - Gram[1][0] * AtPosition[0]) / Determinant;
			Q1[0] = (Gram[1][1] * AtMove[0] - Gram[0][1] * AtMove[1]) / Determinant;
			Q1[1] = (Gram[0][0] * AtMove[1] - Gram[1][0] * AtMove[0]) / Determinant;
		}
		// 自由座標が有限範囲[-h,+h]に収まる割合の閉区間を[0,1]と交差させる。
		f64 Low = 0;
		f64 High = 1;
		bool bFeasible = true;
		for (int32 Index = 0; Index < FreeCount && bFeasible; ++Index)
		{
			const f64 Extent = Half[Free[Index]];
			if (Q1[Index] == 0)
			{
				bFeasible = Q0[Index] >= -Extent && Q0[Index] <= Extent;
				continue;
			}
			const f64 First = (-Extent - Q0[Index]) / Q1[Index];
			const f64 Last = (Extent - Q0[Index]) / Q1[Index];
			Low = Max(Low, Min(First, Last));
			High = Min(High, Max(First, Last));
			bFeasible = Low <= High;
		}
		if (!bFeasible)
		{
			continue;
		}
		// 最短点からの残差 r(t)=r0+t r1。区間先頭へ移して、既存の点と球の進入計算を使う。
		f64 Residual0[3] = {Fixed[0], Fixed[1], Fixed[2]};
		f64 Residual1[3] = {Move[0], Move[1], Move[2]};
		for (int32 Index = 0; Index < FreeCount; ++Index)
		{
			const f64(&A)[3] = Axes[Free[Index]];
			for (int32 Component = 0; Component < 3; ++Component)
			{
				Residual0[Component] -= A[Component] * Q0[Index];
				Residual1[Component] -= A[Component] * Q1[Index];
			}
		}
		const f64 Span = High - Low;
		const f64 Start[3] = {Residual0[0] + Low * Residual1[0], Residual0[1] + Low * Residual1[1],
		                      Residual0[2] + Low * Residual1[2]};
		const f64 Change[3] = {Residual1[0] * Span, Residual1[1] * Span, Residual1[2] * Span};
		const FSweepHit3D Hit = PointBallEntry_Internal(Start, Change, Radius);
		if (!Hit.bHit)
		{
			continue;
		}
		const f64 Time = Low + Span * Hit.Time;
		if (!Best || Time < *Best)
		{
			Best = Time;
		}
	}
	return Best;
}
// 点と球の進入結果を公開用の結果へ変える。
TOptional<FShapeSweepHit> ToShapeHit_Internal(const FSweepHit3D& Hit)
{
	if (!Hit.bHit)
	{
		return {};
	}
	return FShapeSweepHit{Hit.Time, Hit.bInitialContact};
}
// 箱の特徴からの結果を公開用の結果へ変える。開始時の包含は呼出し側で判定済み。
TOptional<FShapeSweepHit> ToShapeHit_Internal(const TOptional<f64>& Time)
{
	if (!Time)
	{
		return {};
	}
	if (!IsFinite(*Time) || *Time < 0 || *Time > 1)
	{
		throw FException("Invalid shape sweep time");
	}
	return FShapeSweepHit{*Time, false};
}
} // namespace
TOptional<FShapeSweepHit> SweepToCenter(const FCircle2D& Moving, FVector2 EndCenter, const FCircle2D& Target)
{
	const f64 Start[3] = {Moving.Center.X, Moving.Center.Y, 0};
	const f64 End[3] = {EndCenter.X, EndCenter.Y, 0};
	f64 Move[3]{};
	PrepareMove_Internal(Start, End, Moving.Center.IsValid() && EndCenter.IsValid(), Moving.Radius, Move);
	if (!IsValid(Target))
	{
		throw FException("Invalid shape sweep target circle");
	}
	// 相対位置の点が半径の和の円へ入る時刻（XY平面の点と球の計算、Zは0）。
	const f64 Relative[3] = {Start[0] - Target.Center.X, Start[1] - Target.Center.Y, 0};
	return ToShapeHit_Internal(PointBallEntry_Internal(Relative, Move, f64(Moving.Radius) + f64(Target.Radius)));
}
TOptional<FShapeSweepHit> SweepToCenter(const FCircle2D& Moving, FVector2 EndCenter, const FOrientedBox2D& Target)
{
	const f64 Start[3] = {Moving.Center.X, Moving.Center.Y, 0};
	const f64 End[3] = {EndCenter.X, EndCenter.Y, 0};
	f64 Move[3]{};
	PrepareMove_Internal(Start, End, Moving.Center.IsValid() && EndCenter.IsValid(), Moving.Radius, Move);
	// 開始時の接触は許容距離0の既存判定で決める（矩形の検証も兼ねる）。
	if (Intersects(Moving, Target, 0.0f))
	{
		return FShapeSweepHit{0, true};
	}
	// Contact2Dと同じ規約の軸（反時計回り）。f64で作るため直交単位軸になる。
	const f64 Cosine = Cos(f64(Target.Angle));
	const f64 Sine = Sin(f64(Target.Angle));
	const f64 Axes[3][3] = {{Cosine, Sine, 0}, {-Sine, Cosine, 0}, {0, 0, 1}};
	const f64 Half[3] = {Target.HalfExtents.X, Target.HalfExtents.Y, 0};
	const f64 Position[3] = {Start[0] - Target.Center.X, Start[1] - Target.Center.Y, 0};
	return ToShapeHit_Internal(BoxFeatureEntry_Internal(Position, Move, Axes, Half, 2, Moving.Radius));
}
TOptional<FShapeSweepHit> SweepToCenter(const FSphere& Moving, FVector3 EndCenter, const FSphere& Target)
{
	const f64 Start[3] = {Moving.Center.X, Moving.Center.Y, Moving.Center.Z};
	const f64 End[3] = {EndCenter.X, EndCenter.Y, EndCenter.Z};
	f64 Move[3]{};
	PrepareMove_Internal(Start, End, Moving.Center.IsValid() && EndCenter.IsValid(), Moving.Radius, Move);
	if (!Target.Center.IsValid() || !IsFinite(Target.Radius) || Target.Radius < 0)
	{
		throw FException("Invalid shape sweep target sphere");
	}
	const f64 Relative[3] = {Start[0] - Target.Center.X, Start[1] - Target.Center.Y, Start[2] - Target.Center.Z};
	return ToShapeHit_Internal(PointBallEntry_Internal(Relative, Move, f64(Moving.Radius) + f64(Target.Radius)));
}
TOptional<FShapeSweepHit> SweepToCenter(const FSphere& Moving, FVector3 EndCenter, const FOBB& Target)
{
	const f64 Start[3] = {Moving.Center.X, Moving.Center.Y, Moving.Center.Z};
	const f64 End[3] = {EndCenter.X, EndCenter.Y, EndCenter.Z};
	f64 Move[3]{};
	PrepareMove_Internal(Start, End, Moving.Center.IsValid() && EndCenter.IsValid(), Moving.Radius, Move);
	// 開始時の接触は、実際の平行六面体への既存の距離計算（許容距離0）で決める（OBBの検証も兼ねる）。
	if (IntersectsSphere(Moving, Target, 0.0f))
	{
		return FShapeSweepHit{0, true};
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
	const f64 Position[3] = {Start[0] - Target.Center.X, Start[1] - Target.Center.Y, Start[2] - Target.Center.Z};
	return ToShapeHit_Internal(BoxFeatureEntry_Internal(Position, Move, Axes, Half, 3, Moving.Radius));
}
} // namespace Toolbox
