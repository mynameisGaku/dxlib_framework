// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Contact2D.h"
namespace Toolbox
{
bool IsValid(const FOrientedBox2D& Box) noexcept
{
	return Box.Center.IsValid() && Box.HalfExtents.IsValid() && Box.HalfExtents.X >= 0 &&
	       Box.HalfExtents.Y >= 0 && IsFinite(Box.Angle);
}
FContactPoint2D FindContact(const FCircle2D& A, const FCircle2D& B)
{
	if (!IsValid(A) || !IsValid(B))
	{
		throw FException("Invalid 2D contact circles");
	}
	// 中心間の差を倍精度で求める。
	const f64 DeltaX = f64(B.Center.X) - A.Center.X;
	const f64 DeltaY = f64(B.Center.Y) - A.Center.Y;
	const f64 Distance = Sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
	const f64 Radii = f64(A.Radius) + B.Radius;
	FContactPoint2D Hit;
	Hit.FeatureId = 0;
	if (Distance > 1e-12)
	{
		// 二つ目から一つ目へ向く単位法線。
		const f64 NormalX = -DeltaX / Distance;
		const f64 NormalY = -DeltaY / Distance;
		Hit.Normal = {static_cast<f32>(NormalX), static_cast<f32>(NormalY)};
		Hit.Separation = static_cast<f32>(Distance - Radii);
		// 両表面の中点。
		const f64 MiddleX = (f64(A.Center.X) + f64(B.Center.X)) * 0.5;
		const f64 MiddleY = (f64(A.Center.Y) + f64(B.Center.Y)) * 0.5;
		Hit.Position = {static_cast<f32>(MiddleX), static_cast<f32>(MiddleY)};
		return Hit;
	}
	// 同一点では代表方向を返し、貫通は半径の和になる。
	Hit.Normal = {1, 0};
	Hit.Separation = static_cast<f32>(-Radii);
	Hit.Position = A.Center;
	return Hit;
}
// 箱座標の軸番号と符号から面IDを作る。-x=0、+x=1、-y=2、+y=3。
static uint32 FaceId_Internal(int32 Axis, f64 Coordinate) noexcept
{
	return static_cast<uint32>(Axis * 2 + (Coordinate > 0 ? 1 : 0));
}
FContactPoint2D FindContact(const FCircle2D& Circle, const FOrientedBox2D& Box)
{
	if (!IsValid(Circle) || !IsValid(Box))
	{
		throw FException("Invalid 2D circle or box contact");
	}
	// 回転角の余弦と正弦。
	const f64 Cosine = Cos(f64(Box.Angle));
	const f64 Sine = Sin(f64(Box.Angle));
	// 中心差を箱座標へ回転する。
	const f64 OffsetX = f64(Circle.Center.X) - Box.Center.X;
	const f64 OffsetY = f64(Circle.Center.Y) - Box.Center.Y;
	const f64 LocalX = Cosine * OffsetX + Sine * OffsetY;
	const f64 LocalY = -Sine * OffsetX + Cosine * OffsetY;
	// 半辺長。
	const f64 HalfX = Box.HalfExtents.X;
	const f64 HalfY = Box.HalfExtents.Y;
	// 各面までの侵入余裕。
	const f64 MarginX = HalfX - Abs(LocalX);
	const f64 MarginY = HalfY - Abs(LocalY);
	FContactPoint2D Hit;
	if (MarginX < 0 || MarginY < 0)
	{
		// 箱の範囲へ寄せた最近点。
		const f64 ClampedX = Clamp(LocalX, -HalfX, HalfX);
		const f64 ClampedY = Clamp(LocalY, -HalfY, HalfY);
		const f64 DeltaX = LocalX - ClampedX;
		const f64 DeltaY = LocalY - ClampedY;
		const f64 Distance = Sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
		// 箱座標の法線。
		f64 NormalX = 0;
		f64 NormalY = 0;
		if (Distance > 1e-12)
		{
			NormalX = DeltaX / Distance;
			NormalY = DeltaY / Distance;
		}
		else
		{
			// 面上に中心がある場合は最小余裕の面を向く。
			if (MarginX > MarginY)
			{
				NormalX = 0;
				NormalY = LocalY > 0 ? 1 : -1;
			}
			else
			{
				NormalX = LocalX > 0 ? 1 : -1;
				NormalY = 0;
			}
		}
		// 特徴の判定。
		const bool InsideX = Abs(LocalX) <= HalfX;
		const bool InsideY = Abs(LocalY) <= HalfY;
		if (!InsideX && InsideY)
		{
			Hit.FeatureId = FaceId_Internal(0, LocalX);
		}
		else if (InsideX && !InsideY)
		{
			Hit.FeatureId = FaceId_Internal(1, LocalY);
		}
		else if (!InsideX && !InsideY)
		{
			// 頂点の符号組み合わせ。
			Hit.FeatureId = static_cast<uint32>(4 + (LocalX > 0 ? 1 : 0) + (LocalY > 0 ? 2 : 0));
		}
		else
		{
			Hit.FeatureId = MarginX > MarginY ? FaceId_Internal(1, LocalY) : FaceId_Internal(0, LocalX);
		}
		// 法線をワールド座標へ戻す。
		const f64 WorldX = Cosine * NormalX - Sine * NormalY;
		const f64 WorldY = Sine * NormalX + Cosine * NormalY;
		Hit.Normal = {static_cast<f32>(WorldX), static_cast<f32>(WorldY)};
		Hit.Separation = static_cast<f32>(Distance - Circle.Radius);
		// 最近点と円表面の中点。
		const f64 NearX = f64(Box.Center.X) + Cosine * ClampedX - Sine * ClampedY;
		const f64 NearY = f64(Box.Center.Y) + Sine * ClampedX + Cosine * ClampedY;
		const f64 SurfaceX = f64(Circle.Center.X) - WorldX * Circle.Radius;
		const f64 SurfaceY = f64(Circle.Center.Y) - WorldY * Circle.Radius;
		Hit.Position = {static_cast<f32>((NearX + SurfaceX) * 0.5), static_cast<f32>((NearY + SurfaceY) * 0.5)};
		return Hit;
	}
	// 中心が内部にある場合は最小余裕の面へ押し出す。
	f64 NormalX = 0;
	f64 NormalY = 0;
	uint32 Feature = 0;
	f64 Depth = 0;
	if (MarginX < MarginY)
	{
		NormalX = LocalX > 0 ? 1 : -1;
		NormalY = 0;
		Feature = FaceId_Internal(0, LocalX);
		Depth = MarginX;
	}
	else
	{
		NormalX = 0;
		NormalY = LocalY > 0 ? 1 : -1;
		Feature = FaceId_Internal(1, LocalY);
		Depth = MarginY;
	}
	Hit.FeatureId = Feature;
	// 法線をワールド座標へ戻す。
	const f64 WorldX = Cosine * NormalX - Sine * NormalY;
	const f64 WorldY = Sine * NormalX + Cosine * NormalY;
	Hit.Normal = {static_cast<f32>(WorldX), static_cast<f32>(WorldY)};
	Hit.Separation = static_cast<f32>(-(Depth + Circle.Radius));
	// 円表面と対応する箱表面の中点。
	const f64 FaceX = LocalX + NormalX * Depth;
	const f64 FaceY = LocalY + NormalY * Depth;
	const f64 NearX = f64(Box.Center.X) + Cosine * FaceX - Sine * FaceY;
	const f64 NearY = f64(Box.Center.Y) + Sine * FaceX + Cosine * FaceY;
	const f64 SurfaceX = f64(Circle.Center.X) - WorldX * Circle.Radius;
	const f64 SurfaceY = f64(Circle.Center.Y) - WorldY * Circle.Radius;
	Hit.Position = {static_cast<f32>((NearX + SurfaceX) * 0.5), static_cast<f32>((NearY + SurfaceY) * 0.5)};
	return Hit;
}
} // namespace Toolbox
