// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/SegmentIntersection.h"
#include "Toolbox/SegmentIntersection2D.h"
#include "Toolbox/ContinuousCollision.h"
#include "Toolbox/Matrix4.h"
namespace Toolbox
{
TOptional<f64> IntersectSegment(FVector3 Start, FVector3 End, const FSphere& Sphere)
{
	if (!Start.IsValid() || !End.IsValid())
	{
		throw FException("Invalid segment endpoints");
	}
	// 半径0の移動球として扱い、接線・初期接触・有限区間の既存判定を再利用する。
	const FSweepHit3D Hit = Sweep(FSphere{Start, 0}, End - Start, Sphere, {});
	return Hit.bHit ? TOptional<f64>(Hit.Time) : TOptional<f64>();
}
TOptional<f64> IntersectSegment(FVector3 Start, FVector3 End, const FOBB& Box)
{
	if (!Start.IsValid() || !End.IsValid() || !IsValid(FCollisionShape{Box}))
	{
		throw FException("Invalid segment or box");
	}
	// 軸の小さな丸め誤差も含めて逆変換する。転置が逆行列だとは仮定しない。
	FMatrix4 World;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (int32 Row = 0; Row < 3; ++Row)
		{
			World.Values[static_cast<size_t>(Row * 4 + Axis)] = Box.Axes[static_cast<size_t>(Axis)].Component(Row);
		}
	}
	FMatrix4 Inverse;
	if (!World.TryInverse(Inverse))
	{
		throw FException("Box basis cannot be inverted");
	}
	// 先に中心を引き、逆行列の大きな平行移動との相殺を避ける。
	const FVector3 A = Inverse.TransformPoint(Start - Box.Center);
	const FVector3 B = Inverse.TransformPoint(End - Box.Center);
	if (!A.IsValid() || !B.IsValid())
	{
		throw FException("Local segment is not finite");
	}
	// 各軸の内側にいる割合区間を交差させる。
	f64 Entry = 0;
	f64 Exit = 1;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const f64 Origin = A.Component(Axis);
		const f64 Delta = static_cast<f64>(B.Component(Axis)) - Origin;
		const f64 Extent = Box.HalfExtents.Component(Axis);
		if (Delta == 0)
		{
			if (Origin < -Extent || Origin > Extent)
			{
				return {};
			}
			continue;
		}
		const f64 First = (-Extent - Origin) / Delta;
		const f64 Last = (Extent - Origin) / Delta;
		Entry = Max(Entry, Min(First, Last));
		Exit = Min(Exit, Max(First, Last));
		if (Entry > Exit)
		{
			return {};
		}
	}
	return Entry;
}
TOptional<f64> IntersectSegment(FVector2 Start, FVector2 End, const FCircle2D& Circle)
{
	if (!Start.IsValid() || !End.IsValid() || !IsValid(Circle))
	{
		throw FException("Invalid 2D segment or circle");
	}
	// 半径0の移動円として扱い、接線・初期接触・有限区間の既存判定を再利用する。変位の表現不能はSweepが拒否する。
	const FSweepHit2D Hit = Sweep(FCircle2D{Start, 0}, End - Start, Circle, {}, 0);
	return Hit.bHit ? TOptional<f64>(Hit.Time) : TOptional<f64>();
}
TOptional<f64> IntersectSegment(FVector2 Start, FVector2 End, const FOrientedBox2D& Box)
{
	if (!Start.IsValid() || !End.IsValid() || !IsValid(Box))
	{
		throw FException("Invalid 2D segment or box");
	}
	// Contact2Dと同じ規約で、中心差を先に取ってから逆回転し局所座標へ移す。
	const f64 Cosine = Cos(f64(Box.Angle));
	const f64 Sine = Sin(f64(Box.Angle));
	const f64 StartX = f64(Start.X) - Box.Center.X;
	const f64 StartY = f64(Start.Y) - Box.Center.Y;
	const f64 EndX = f64(End.X) - Box.Center.X;
	const f64 EndY = f64(End.Y) - Box.Center.Y;
	const f64 Origin[2] = {Cosine * StartX + Sine * StartY, -Sine * StartX + Cosine * StartY};
	const f64 Target[2] = {Cosine * EndX + Sine * EndY, -Sine * EndX + Cosine * EndY};
	const f64 Extents[2] = {Box.HalfExtents.X, Box.HalfExtents.Y};
	// 各軸の内側にいる割合区間を交差させる。
	f64 Entry = 0;
	f64 Exit = 1;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const f64 Delta = Target[Axis] - Origin[Axis];
		if (!IsFinite(Origin[Axis]) || !IsFinite(Delta))
		{
			throw FException("Local 2D segment is not finite");
		}
		if (Delta == 0)
		{
			if (Origin[Axis] < -Extents[Axis] || Origin[Axis] > Extents[Axis])
			{
				return {};
			}
			continue;
		}
		const f64 First = (-Extents[Axis] - Origin[Axis]) / Delta;
		const f64 Last = (Extents[Axis] - Origin[Axis]) / Delta;
		Entry = Max(Entry, Min(First, Last));
		Exit = Min(Exit, Max(First, Last));
		if (Entry > Exit)
		{
			return {};
		}
	}
	return Entry;
}
} // namespace Toolbox
