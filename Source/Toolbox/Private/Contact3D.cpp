// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Contact3D.h"
namespace Toolbox
{
// 箱軸の長さが判定に使えるか調べる。
static bool ValidAxis_Internal(FVector3 Axis) noexcept
{
	if (!Axis.IsValid())
	{
		return false;
	}
	// 長さの二乗。
	const f32 Squared = LengthSquared(Axis);
	return Squared > 1e-24f && IsFinite(Squared);
}
FContactPoint3D FindContact(const FSphere& A, const FSphere& B)
{
	if (!A.Center.IsValid() || !B.Center.IsValid())
	{
		throw FException("Invalid 3D contact sphere centers");
	}
	if (!IsFinite(A.Radius) || A.Radius < 0 || !IsFinite(B.Radius) || B.Radius < 0)
	{
		throw FException("Invalid 3D contact sphere radii");
	}
	// 中心間の差を倍精度で求める。
	const f64 DeltaX = f64(B.Center.X) - A.Center.X;
	const f64 DeltaY = f64(B.Center.Y) - A.Center.Y;
	const f64 DeltaZ = f64(B.Center.Z) - A.Center.Z;
	const f64 Distance = Sqrt(DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ);
	const f64 Radii = f64(A.Radius) + B.Radius;
	FContactPoint3D Hit;
	Hit.FeatureId = 0;
	if (Distance > 1e-12)
	{
		// 二つ目から一つ目へ向く単位法線。
		Hit.Normal = {static_cast<f32>(-DeltaX / Distance), static_cast<f32>(-DeltaY / Distance),
		              static_cast<f32>(-DeltaZ / Distance)};
		Hit.Separation = static_cast<f32>(Distance - Radii);
		// 両中心の中点。
		Hit.Position = {static_cast<f32>((f64(A.Center.X) + f64(B.Center.X)) * 0.5),
		                static_cast<f32>((f64(A.Center.Y) + f64(B.Center.Y)) * 0.5),
		                static_cast<f32>((f64(A.Center.Z) + f64(B.Center.Z)) * 0.5)};
		return Hit;
	}
	// 同一点では代表方向を返し、貫通は半径の和になる。
	Hit.Normal = {1, 0, 0};
	Hit.Separation = static_cast<f32>(-Radii);
	Hit.Position = A.Center;
	return Hit;
}
// 箱座標の軸番号と符号から面IDを作る。軸*2に正方向で1を足す。
static uint32 FaceId_Internal(int32 Axis, f64 Coordinate) noexcept
{
	return static_cast<uint32>(Axis * 2 + (Coordinate > 0 ? 1 : 0));
}
FContactPoint3D FindContact(const FSphere& Sphere, const FOBB& Box)
{
	if (!Sphere.Center.IsValid() || !IsFinite(Sphere.Radius) || Sphere.Radius < 0)
	{
		throw FException("Invalid 3D sphere contact");
	}
	if (!Box.Center.IsValid() || !Box.HalfExtents.IsValid())
	{
		throw FException("Invalid 3D box contact");
	}
	if (Box.HalfExtents.X < 0 || Box.HalfExtents.Y < 0 || Box.HalfExtents.Z < 0)
	{
		throw FException("Invalid 3D box contact");
	}
	if (!ValidAxis_Internal(Box.Axes[0]) || !ValidAxis_Internal(Box.Axes[1]) || !ValidAxis_Internal(Box.Axes[2]))
	{
		throw FException("Invalid 3D box contact");
	}
	// 軸を単位化して箱座標を作る。平行六面体の歪みは方向だけに使う。
	FVector3 Unit[3];
	f64 Half[3];
	f64 Local[3];
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const f64 Length = Sqrt(f64(Box.Axes[Axis].X) * Box.Axes[Axis].X + f64(Box.Axes[Axis].Y) * Box.Axes[Axis].Y +
		                        f64(Box.Axes[Axis].Z) * Box.Axes[Axis].Z);
		Unit[Axis] = {static_cast<f32>(f64(Box.Axes[Axis].X) / Length),
		              static_cast<f32>(f64(Box.Axes[Axis].Y) / Length),
		              static_cast<f32>(f64(Box.Axes[Axis].Z) / Length)};
		Half[Axis] = Box.HalfExtents.Component(Axis);
		// 中心差の軸成分。
		const f64 OffsetX = f64(Sphere.Center.X) - Box.Center.X;
		const f64 OffsetY = f64(Sphere.Center.Y) - Box.Center.Y;
		const f64 OffsetZ = f64(Sphere.Center.Z) - Box.Center.Z;
		Local[Axis] = f64(Unit[Axis].X) * OffsetX + f64(Unit[Axis].Y) * OffsetY + f64(Unit[Axis].Z) * OffsetZ;
	}
	// 各面までの侵入余裕。
	f64 Margin[3];
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Margin[Axis] = Half[Axis] - Abs(Local[Axis]);
	}
	FContactPoint3D Hit;
	// 箱座標の法線。
	f64 NormalLocal[3] = {0, 0, 0};
	// 箱座標の最近点。
	f64 NearLocal[3] = {0, 0, 0};
	if (Margin[0] < 0 || Margin[1] < 0 || Margin[2] < 0)
	{
		// 箱の範囲へ寄せた最近点。
		f64 Clamped[3];
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Clamped[Axis] = Clamp(Local[Axis], -Half[Axis], Half[Axis]);
		}
		const f64 DeltaX = Local[0] - Clamped[0];
		const f64 DeltaY = Local[1] - Clamped[1];
		const f64 DeltaZ = Local[2] - Clamped[2];
		const f64 Distance = Sqrt(DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ);
		if (Distance > 1e-12)
		{
			NormalLocal[0] = DeltaX / Distance;
			NormalLocal[1] = DeltaY / Distance;
			NormalLocal[2] = DeltaZ / Distance;
		}
		else
		{
			// 面上に中心がある場合は最小余裕の面を向く。
			int32 Face = 0;
			for (int32 Axis = 1; Axis < 3; ++Axis)
			{
				if (Margin[Axis] < Margin[Face])
				{
					Face = Axis;
				}
			}
			NormalLocal[Face] = Local[Face] > 0 ? 1 : -1;
		}
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			NearLocal[Axis] = Clamped[Axis];
		}
		// 中心が箱範囲の内側にある軸の数を数える。
		int32 InsideCount = 0;
		int32 OutsideAxis = -1;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (Abs(Local[Axis]) <= Half[Axis])
			{
				++InsideCount;
			}
			else
			{
				OutsideAxis = Axis;
			}
		}
		if (InsideCount == 2)
		{
			Hit.FeatureId = FaceId_Internal(OutsideAxis, Local[OutsideAxis]);
		}
		else if (InsideCount == 1)
		{
			// 二軸が範囲外の辺。軸番号の小さい順に12辺を番号付けする。
			int32 First = -1;
			int32 Second = -1;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				if (Abs(Local[Axis]) > Half[Axis])
				{
					if (First < 0)
					{
						First = Axis;
					}
					else
					{
						Second = Axis;
					}
				}
			}
			// 辺の基準番号。
			int32 Edge = 0;
			if (First == 0 && Second == 1)
			{
				Edge = 0;
			}
			else if (First == 0 && Second == 2)
			{
				Edge = 4;
			}
			else
			{
				Edge = 8;
			}
			// 辺の四つの平行候補を符号で区別する。
			const int32 Low = First < Second ? First : Second;
			const int32 High = First < Second ? Second : First;
			Edge += (Local[Low] > 0 ? 1 : 0) + (Local[High] > 0 ? 2 : 0);
			Hit.FeatureId = static_cast<uint32>(6 + Edge);
		}
		else
		{
			// 三軸が範囲外の頂点。符号ビットで八つを区別する。
			uint32 Vertex = 0;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				if (Local[Axis] > 0)
				{
					Vertex |= static_cast<uint32>(1 << Axis);
				}
			}
			Hit.FeatureId = 18 + Vertex;
		}
		Hit.Separation = static_cast<f32>(Distance - Sphere.Radius);
	}
	else
	{
		// 中心が内部にある場合は最小余裕の面へ押し出す。
		int32 Face = 0;
		for (int32 Axis = 1; Axis < 3; ++Axis)
		{
			if (Margin[Axis] < Margin[Face])
			{
				Face = Axis;
			}
		}
		NormalLocal[Face] = Local[Face] > 0 ? 1 : -1;
		Hit.FeatureId = FaceId_Internal(Face, Local[Face]);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			NearLocal[Axis] = Local[Axis];
		}
		NearLocal[Face] += NormalLocal[Face] * Margin[Face];
		Hit.Separation = static_cast<f32>(-(Margin[Face] + Sphere.Radius));
	}
	// 法線をワールド座標へ戻す。
	const f64 WorldX = f64(Unit[0].X) * NormalLocal[0] + f64(Unit[1].X) * NormalLocal[1] + f64(Unit[2].X) * NormalLocal[2];
	const f64 WorldY = f64(Unit[0].Y) * NormalLocal[0] + f64(Unit[1].Y) * NormalLocal[1] + f64(Unit[2].Y) * NormalLocal[2];
	const f64 WorldZ = f64(Unit[0].Z) * NormalLocal[0] + f64(Unit[1].Z) * NormalLocal[1] + f64(Unit[2].Z) * NormalLocal[2];
	Hit.Normal = {static_cast<f32>(WorldX), static_cast<f32>(WorldY), static_cast<f32>(WorldZ)};
	// 箱表面の最近点。
	const f64 NearX = f64(Box.Center.X) + f64(Unit[0].X) * NearLocal[0] + f64(Unit[1].X) * NearLocal[1] +
	                  f64(Unit[2].X) * NearLocal[2];
	const f64 NearY = f64(Box.Center.Y) + f64(Unit[0].Y) * NearLocal[0] + f64(Unit[1].Y) * NearLocal[1] +
	                  f64(Unit[2].Y) * NearLocal[2];
	const f64 NearZ = f64(Box.Center.Z) + f64(Unit[0].Z) * NearLocal[0] + f64(Unit[1].Z) * NearLocal[1] +
	                  f64(Unit[2].Z) * NearLocal[2];
	// 球表面と箱表面の中点。
	const f64 SurfaceX = f64(Sphere.Center.X) - WorldX * Sphere.Radius;
	const f64 SurfaceY = f64(Sphere.Center.Y) - WorldY * Sphere.Radius;
	const f64 SurfaceZ = f64(Sphere.Center.Z) - WorldZ * Sphere.Radius;
	Hit.Position = {static_cast<f32>((NearX + SurfaceX) * 0.5), static_cast<f32>((NearY + SurfaceY) * 0.5),
	                static_cast<f32>((NearZ + SurfaceZ) * 0.5)};
	return Hit;
}
} // namespace Toolbox
