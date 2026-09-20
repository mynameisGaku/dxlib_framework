// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/ContinuousCollision.h"
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
} // namespace Toolbox
