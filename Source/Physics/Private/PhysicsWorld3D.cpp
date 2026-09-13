// SPDX-License-Identifier: NOASSERTION
#include "Dxf/RigidBody3D.h"
#include "Toolbox/ContinuousCollision.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
// 倍精度で姿勢変換を行う四元数。
struct FQuaternionD
{
	// 虚部のX成分。
	Toolbox::f64 X = 0;
	// 虚部のY成分。
	Toolbox::f64 Y = 0;
	// 虚部のZ成分。
	Toolbox::f64 Z = 0;
	// 実数成分。
	Toolbox::f64 W = 1;
};
// 倍精度の三成分ベクトル。
struct FVector3D
{
	// X軸成分。
	Toolbox::f64 X = 0;
	// Y軸成分。
	Toolbox::f64 Y = 0;
	// Z軸成分。
	Toolbox::f64 Z = 0;
};
// 単位四元数で方向を回転する。Quaternion::Rotateと同じ式の倍精度版。
static FVector3D Rotate_Internal(const FQuaternionD& Q, FVector3D Value) noexcept
{
	// 虚部と入力の外積の二倍。
	const FVector3D Cross = {Q.Y * Value.Z - Q.Z * Value.Y, Q.Z * Value.X - Q.X * Value.Z,
	                         Q.X * Value.Y - Q.Y * Value.X};
	const FVector3D Twice = {Cross.X * 2, Cross.Y * 2, Cross.Z * 2};
	// 外積と回転後の外積の合成。
	const FVector3D Outer = {Q.Y * Twice.Z - Q.Z * Twice.Y, Q.Z * Twice.X - Q.X * Twice.Z,
	                         Q.X * Twice.Y - Q.Y * Twice.X};
	FVector3D Result = {Value.X + Twice.X * Q.W + Outer.X, Value.Y + Twice.Y * Q.W + Outer.Y,
	                    Value.Z + Twice.Z * Q.W + Outer.Z};
	return Result;
}
// 対角慣性を姿勢で挟んでワールド量へ変換する。逆数と通常で共用する。
static FVector3D TransformDiagonal_Internal(const FQuaternionD& Q, FVector3D Diagonal, FVector3D Value) noexcept
{
	// 共役でボディ座標へ戻した成分。
	const FQuaternionD Conjugate = {-Q.X, -Q.Y, -Q.Z, Q.W};
	const FVector3D Local = Rotate_Internal(Conjugate, Value);
	// 対角成分を掛けてワールド座標へ戻す。
	const FVector3D Scaled = {Local.X * Diagonal.X, Local.Y * Diagonal.Y, Local.Z * Diagonal.Z};
	return Rotate_Internal(Q, Scaled);
}
// 姿勢を表す単精度四元数を倍精度へ写す。
static FQuaternionD ToDouble_Internal(Toolbox::FQuaternion Q) noexcept
{
	FQuaternionD Result;
	Result.X = Q.X;
	Result.Y = Q.Y;
	Result.Z = Q.Z;
	Result.W = Q.W;
	return Result;
}
// ワールド角速度の四元数微分で姿勢を進める。ノルム異常時は姿勢を保つ。
static void IntegrateOrientation_Internal(Toolbox::FQuaternion& Orientation, FVector3D Angular,
                                          Toolbox::f64 StepSeconds) noexcept
{
	const FQuaternionD Q = ToDouble_Internal(Orientation);
	FQuaternionD Next = {Q.X + 0.5 * StepSeconds * (Angular.X * Q.W + Angular.Y * Q.Z - Angular.Z * Q.Y),
	                     Q.Y + 0.5 * StepSeconds * (-Angular.X * Q.Z + Angular.Y * Q.W + Angular.Z * Q.X),
	                     Q.Z + 0.5 * StepSeconds * (Angular.X * Q.Y - Angular.Y * Q.X + Angular.Z * Q.W),
	                     Q.W + 0.5 * StepSeconds * (-Angular.X * Q.X - Angular.Y * Q.Y - Angular.Z * Q.Z)};
	// 姿勢ノルムの二乗。
	const Toolbox::f64 Norm = Next.X * Next.X + Next.Y * Next.Y + Next.Z * Next.Z + Next.W * Next.W;
	if (Toolbox::IsFinite(Norm) && Norm > 1e-24)
	{
		// 逆平方根で単位化する。
		const Toolbox::f64 Inverse = 1.0 / Toolbox::Sqrt(Norm);
		Orientation = {static_cast<Toolbox::f32>(Next.X * Inverse), static_cast<Toolbox::f32>(Next.Y * Inverse),
		               static_cast<Toolbox::f32>(Next.Z * Inverse), static_cast<Toolbox::f32>(Next.W * Inverse)};
	}
}
// 一つの立体剛体が持つ運動状態と蓄積した外力。
struct FBodyRecord3D
{
	// スロット再使用を見分ける世代。
	Toolbox::uint64 Generation = 0;
	// 有効な登録か。
	bool bAlive = false;
	// 運動区分。
	EBodyType Type = EBodyType::Dynamic;
	// 重心位置。メートル単位で右手系。
	Toolbox::FVector3 Position;
	// 姿勢。右手則の単位四元数。
	Toolbox::FQuaternion Orientation;
	// 重心速度。メートル毎秒単位。
	Toolbox::FVector3 Velocity;
	// 角速度。ワールド軸回りのラジアン毎秒。
	Toolbox::FVector3 AngularVelocity;
	// 直前の位置更新が終わった時点の速度。起床判定の相対運動に使う。
	Toolbox::FVector3 PrevVelocity;
	// 直前の位置更新が終わった時点の角速度。起床判定の相対運動に使う。
	Toolbox::FVector3 PrevAngularVelocity;
	// 質量の逆数。StaticとKinematicはゼロ。
	Toolbox::f32 InverseMass = 1;
	// ボディ座標の対角慣性の逆数。StaticとKinematicはゼロ。
	Toolbox::FVector3 InverseDiagonalInertia{1, 1, 1};
	// ボディ座標の対角慣性。角運動量の計算に使う。
	Toolbox::FVector3 DiagonalInertia{1, 1, 1};
	// 速度の減衰率。1毎秒単位。
	Toolbox::f32 LinearDamping = 0;
	// 角速度の減衰率。1毎秒単位。
	Toolbox::f32 AngularDamping = 0;
	// ワールド重力への追従倍率。
	Toolbox::f32 GravityScale = 1;
	// 移動区間の接触解決を行うか。
	bool bUseContinuous = false;
	// 速度低下による休止を許可するか。
	bool bAllowSleep = true;
	// 低速接触の継続秒数。
	Toolbox::f32 SleepTimer = 0;
	// 休止しているか。
	bool bSleeping = false;
	// 今回分割で接触したか。
	bool bTouched = false;
	// 次の更新で使う蓄積力。ニュートン単位。
	Toolbox::FVector3 Force;
	// 次の更新で使う蓄積トルク。ワールド軸回り。
	Toolbox::FVector3 Torque;
};
// 更新後の速度で位置を進める。
static void IntegratePosition_Internal(FBodyRecord3D& Record, Toolbox::f64 StepSeconds) noexcept;
// 剛体へ取り付けた形状と材質の登録。
struct FColliderRecord3D
{
	// スロット再使用を見分ける世代。
	Toolbox::uint64 Generation = 0;
	// 有効な登録か。
	bool bAlive = false;
	// 取り付け先の剛体。
	FBodyId3D Body;
	// 重心相対の形状。
	Toolbox::TVariant<Toolbox::FSphere, Toolbox::FOBB> Shape;
	// 摩擦係数。
	Toolbox::f32 Friction = 0.5f;
	// 反発係数。
	Toolbox::f32 Restitution = 0;
};
// 速度拘束の反復で使う単一接触点。
struct FSolvePoint3D
{
	// 接触位置。メートル単位。
	Toolbox::FVector3 Position;
	// 二つ目から一つ目へ向く単位法線。
	Toolbox::FVector3 Normal{1, 0, 0};
	// 表面間の符号付き距離。
	Toolbox::f32 Separation = 0;
	// 箱側の特徴を区別する安定ID。
	Toolbox::uint32 FeatureId = 0;
	// 混合済みの摩擦係数。
	Toolbox::f32 Friction = 0;
	// 混合済みの反発係数。
	Toolbox::f32 Restitution = 0;
	// 蓄積した法線Impulse。
	Toolbox::f32 NormalImpulse = 0;
	// 接平面で蓄積した摩擦Impulse。
	Toolbox::FVector3 FrictionImpulse;
	// 反復前の法線相対速度。反発目標の保存値。
	Toolbox::f64 ApproachSpeed = 0;
};
// 正準順序のコライダー組と接触点列。
struct FManifold3D
{
	// 正準順序の一つ目のコライダー。
	FColliderId3D ColliderA;
	// 正準順序の二つ目のコライダー。
	FColliderId3D ColliderB;
	// 一つ目の剛体。
	FBodyId3D BodyA;
	// 二つ目の剛体。
	FBodyId3D BodyB;
	// 解決する接触点列。
	Toolbox::TVector<FSolvePoint3D> Points;
};
// 前回Impulseの再利用記録。
struct FCachedImpulse3D
{
	// 正準順序の一つ目のコライダー。
	FColliderId3D ColliderA;
	// 正準順序の二つ目のコライダー。
	FColliderId3D ColliderB;
	// 一つ目の剛体の世代。
	Toolbox::uint64 BodyGenerationA = 0;
	// 二つ目の剛体の世代。
	Toolbox::uint64 BodyGenerationB = 0;
	// 接触点の特徴ID。
	Toolbox::uint32 FeatureId = 0;
	// 保存時の法線。
	Toolbox::FVector3 Normal{1, 0, 0};
	// 保存した法線Impulse。
	Toolbox::f32 NormalImpulse = 0;
	// 保存した摩擦Impulse。
	Toolbox::FVector3 FrictionImpulse;
};
// 移動区間を覆う軸平行境界。
struct FSweptBounds3D
{
	// 三軸の最小座標。
	Toolbox::FVector3 Min;
	// 三軸の最大座標。
	Toolbox::FVector3 Max;
};
// 球の移動区間を覆う境界を求める。
static FSweptBounds3D SweptSphere_Internal(Toolbox::FSphere Sphere, Toolbox::FVector3 Displacement) noexcept
{
	FSweptBounds3D Bounds;
	// 始点と終点の両端に半径を足す。
	const Toolbox::f64 StartX = Sphere.Center.X;
	const Toolbox::f64 StartY = Sphere.Center.Y;
	const Toolbox::f64 StartZ = Sphere.Center.Z;
	const Toolbox::f64 EndX = StartX + Displacement.X;
	const Toolbox::f64 EndY = StartY + Displacement.Y;
	const Toolbox::f64 EndZ = StartZ + Displacement.Z;
	Bounds.Min = {static_cast<Toolbox::f32>((StartX < EndX ? StartX : EndX) - Sphere.Radius),
	              static_cast<Toolbox::f32>((StartY < EndY ? StartY : EndY) - Sphere.Radius),
	              static_cast<Toolbox::f32>((StartZ < EndZ ? StartZ : EndZ) - Sphere.Radius)};
	Bounds.Max = {static_cast<Toolbox::f32>((StartX > EndX ? StartX : EndX) + Sphere.Radius),
	              static_cast<Toolbox::f32>((StartY > EndY ? StartY : EndY) + Sphere.Radius),
	              static_cast<Toolbox::f32>((StartZ > EndZ ? StartZ : EndZ) + Sphere.Radius)};
	return Bounds;
}
// 箱の移動区間を覆う境界を求める。
static FSweptBounds3D SweptBox_Internal(const Toolbox::FOBB& Box, Toolbox::FVector3 Displacement) noexcept
{
	FSweptBounds3D Bounds;
	// 八隅の始点と終点で範囲を広げる。
	bool bFirst = true;
	for (Toolbox::size_t Corner = 0; Corner < 8; ++Corner)
	{
		const Toolbox::f64 SignX = (Corner & 1) == 0 ? -1 : 1;
		const Toolbox::f64 SignY = (Corner & 2) == 0 ? -1 : 1;
		const Toolbox::f64 SignZ = (Corner & 4) == 0 ? -1 : 1;
		const Toolbox::f64 StartX = Toolbox::f64(Box.Center.X) + SignX * Box.HalfExtents.X * Box.Axes[0].X +
		                            SignY * Box.HalfExtents.Y * Box.Axes[1].X + SignZ * Box.HalfExtents.Z * Box.Axes[2].X;
		const Toolbox::f64 StartY = Toolbox::f64(Box.Center.Y) + SignX * Box.HalfExtents.X * Box.Axes[0].Y +
		                            SignY * Box.HalfExtents.Y * Box.Axes[1].Y + SignZ * Box.HalfExtents.Z * Box.Axes[2].Y;
		const Toolbox::f64 StartZ = Toolbox::f64(Box.Center.Z) + SignX * Box.HalfExtents.X * Box.Axes[0].Z +
		                            SignY * Box.HalfExtents.Y * Box.Axes[1].Z + SignZ * Box.HalfExtents.Z * Box.Axes[2].Z;
		const Toolbox::f64 PointsX[2] = {StartX, StartX + Displacement.X};
		const Toolbox::f64 PointsY[2] = {StartY, StartY + Displacement.Y};
		const Toolbox::f64 PointsZ[2] = {StartZ, StartZ + Displacement.Z};
		for (Toolbox::int32 Point = 0; Point < 2; ++Point)
		{
			if (bFirst)
			{
				Bounds.Min = {static_cast<Toolbox::f32>(PointsX[Point]), static_cast<Toolbox::f32>(PointsY[Point]),
				              static_cast<Toolbox::f32>(PointsZ[Point])};
				Bounds.Max = Bounds.Min;
				bFirst = false;
			}
			else
			{
				if (PointsX[Point] < Bounds.Min.X)
				{
					Bounds.Min.X = static_cast<Toolbox::f32>(PointsX[Point]);
				}
				if (PointsY[Point] < Bounds.Min.Y)
				{
					Bounds.Min.Y = static_cast<Toolbox::f32>(PointsY[Point]);
				}
				if (PointsZ[Point] < Bounds.Min.Z)
				{
					Bounds.Min.Z = static_cast<Toolbox::f32>(PointsZ[Point]);
				}
				if (PointsX[Point] > Bounds.Max.X)
				{
					Bounds.Max.X = static_cast<Toolbox::f32>(PointsX[Point]);
				}
				if (PointsY[Point] > Bounds.Max.Y)
				{
					Bounds.Max.Y = static_cast<Toolbox::f32>(PointsY[Point]);
				}
				if (PointsZ[Point] > Bounds.Max.Z)
				{
					Bounds.Max.Z = static_cast<Toolbox::f32>(PointsZ[Point]);
				}
			}
		}
	}
	return Bounds;
}
// 二つの移動境界が重なるかを調べる。
static bool SweptOverlaps_Internal(const FSweptBounds3D& A, const FSweptBounds3D& B) noexcept
{
	return A.Min.X <= B.Max.X && B.Min.X <= A.Max.X && A.Min.Y <= B.Max.Y && B.Min.Y <= A.Max.Y &&
	       A.Min.Z <= B.Max.Z && B.Min.Z <= A.Max.Z;
}
// 世界箱の軸が符号付き単位軸の置換かを調べる。
static bool IsAxisAlignedBox_Internal(const Toolbox::FOBB& Box) noexcept
{
	bool bUsed[3] = {false, false, false};
	for (Toolbox::int32 Axis = 0; Axis < 3; ++Axis)
	{
		// 軸の長さで正規化する。
		const Toolbox::f64 Length = Toolbox::Sqrt(Toolbox::f64(Box.Axes[Axis].X) * Box.Axes[Axis].X +
		                                          Toolbox::f64(Box.Axes[Axis].Y) * Box.Axes[Axis].Y +
		                                          Toolbox::f64(Box.Axes[Axis].Z) * Box.Axes[Axis].Z);
		if (!(Length > 1e-12))
		{
			return false;
		}
		const Toolbox::f64 X = Toolbox::Abs(Toolbox::f64(Box.Axes[Axis].X) / Length);
		const Toolbox::f64 Y = Toolbox::Abs(Toolbox::f64(Box.Axes[Axis].Y) / Length);
		const Toolbox::f64 Z = Toolbox::Abs(Toolbox::f64(Box.Axes[Axis].Z) / Length);
		Toolbox::int32 Dominant = -1;
		if (X > 1 - 1e-6 && Y < 1e-6 && Z < 1e-6)
		{
			Dominant = 0;
		}
		else if (Y > 1 - 1e-6 && X < 1e-6 && Z < 1e-6)
		{
			Dominant = 1;
		}
		else if (Z > 1 - 1e-6 && X < 1e-6 && Y < 1e-6)
		{
			Dominant = 2;
		}
		else
		{
			return false;
		}
		if (bUsed[Dominant])
		{
			return false;
		}
		bUsed[Dominant] = true;
	}
	return true;
}
// 箱軸を倍精度へ写す。
static FVector3D AxisD_Internal(Toolbox::FVector3 Axis) noexcept
{
	FVector3D Result;
	Result.X = Axis.X;
	Result.Y = Axis.Y;
	Result.Z = Axis.Z;
	return Result;
}
// 箱の支持点を求める。方向へ最も進んだ頂点。
static FVector3D Support_Internal(const Toolbox::FOBB& Box, FVector3D Direction) noexcept
{
	FVector3D Point = {Box.Center.X, Box.Center.Y, Box.Center.Z};
	const Toolbox::f64 Half[3] = {Box.HalfExtents.X, Box.HalfExtents.Y, Box.HalfExtents.Z};
	for (Toolbox::int32 Axis = 0; Axis < 3; ++Axis)
	{
		const FVector3D AxisVector = AxisD_Internal(Box.Axes[Axis]);
		const Toolbox::f64 Sign =
		    AxisVector.X * Direction.X + AxisVector.Y * Direction.Y + AxisVector.Z * Direction.Z >= 0 ? 1 : -1;
		Point.X += AxisVector.X * Sign * Half[Axis];
		Point.Y += AxisVector.Y * Sign * Half[Axis];
		Point.Z += AxisVector.Z * Sign * Half[Axis];
	}
	return Point;
}
// 箱の指定面の四隅を求める。面番号は軸*2に正方向で1を足す。
static void FaceVertices_Internal(const Toolbox::FOBB& Box, Toolbox::int32 Face, FVector3D Vertices[4]) noexcept
{
	const Toolbox::int32 Axis = Face / 2;
	const Toolbox::f64 Sign = (Face & 1) == 0 ? -1 : 1;
	const Toolbox::int32 Other[2] = {Axis == 0 ? 1 : 0, Axis == 2 ? 1 : 2};
	for (Toolbox::int32 Corner = 0; Corner < 4; ++Corner)
	{
		const Toolbox::f64 SignA = (Corner & 1) == 0 ? -1 : 1;
		const Toolbox::f64 SignB = (Corner & 2) == 0 ? -1 : 1;
		const FVector3D AxisN = AxisD_Internal(Box.Axes[Axis]);
		const FVector3D AxisA = AxisD_Internal(Box.Axes[Other[0]]);
		const FVector3D AxisB = AxisD_Internal(Box.Axes[Other[1]]);
		const Toolbox::f64 HalfN =
		    Axis == 0 ? Box.HalfExtents.X : (Axis == 1 ? Box.HalfExtents.Y : Box.HalfExtents.Z);
		const Toolbox::f64 HalfA =
		    Other[0] == 0 ? Box.HalfExtents.X : (Other[0] == 1 ? Box.HalfExtents.Y : Box.HalfExtents.Z);
		const Toolbox::f64 HalfB =
		    Other[1] == 0 ? Box.HalfExtents.X : (Other[1] == 1 ? Box.HalfExtents.Y : Box.HalfExtents.Z);
		Vertices[Corner].X = Box.Center.X + AxisN.X * Sign * HalfN + AxisA.X * SignA * HalfA + AxisB.X * SignB * HalfB;
		Vertices[Corner].Y = Box.Center.Y + AxisN.Y * Sign * HalfN + AxisA.Y * SignA * HalfA + AxisB.Y * SignB * HalfB;
		Vertices[Corner].Z = Box.Center.Z + AxisN.Z * Sign * HalfN + AxisA.Z * SignA * HalfA + AxisB.Z * SignB * HalfB;
	}
}
// 世界箱同士の接触を最大四点求める。分離時は空。法線はB→A。辺同士は近似一点。
static void FindBoxBoxContacts_Internal(const Toolbox::FOBB& A, const Toolbox::FOBB& B, Toolbox::f32 Slop,
                                        Toolbox::TVector<Toolbox::FContactPoint3D>& Out)
{
	// 中心差。
	const FVector3D Delta = {Toolbox::f64(B.Center.X) - A.Center.X, Toolbox::f64(B.Center.Y) - A.Center.Y,
	                         Toolbox::f64(B.Center.Z) - A.Center.Z};
	// 最大分離とその軸の情報。
	Toolbox::f64 BestSeparation = -1e30;
	FVector3D BestDirection = {0, 1, 0};
	bool bBestOnA = true;
	Toolbox::int32 BestFaceAxis = 0;
	bool bBestIsFace = true;
	for (Toolbox::int32 Owner = 0; Owner < 2; ++Owner)
	{
		const Toolbox::FOBB& Self = Owner == 0 ? A : B;
		for (Toolbox::int32 Axis = 0; Axis < 3; ++Axis)
		{
			// 軸の長さで正規化する。
			FVector3D Raw = AxisD_Internal(Self.Axes[Axis]);
			const Toolbox::f64 Length = Toolbox::Sqrt(Raw.X * Raw.X + Raw.Y * Raw.Y + Raw.Z * Raw.Z);
			if (!(Length > 1e-9))
			{
				continue;
			}
			Raw.X /= Length;
			Raw.Y /= Length;
			Raw.Z /= Length;
			// 軸方向の中心距離。
			const Toolbox::f64 Distance = Delta.X * Raw.X + Delta.Y * Raw.Y + Delta.Z * Raw.Z;
			// 両箱の軸への投影半径。
			Toolbox::f64 ProjectA = 0;
			Toolbox::f64 ProjectB = 0;
			for (Toolbox::int32 Other = 0; Other < 3; ++Other)
			{
				const FVector3D AxisA = AxisD_Internal(A.Axes[Other]);
				const FVector3D AxisB = AxisD_Internal(B.Axes[Other]);
				const Toolbox::f64 HalfA =
				    Other == 0 ? A.HalfExtents.X : (Other == 1 ? A.HalfExtents.Y : A.HalfExtents.Z);
				const Toolbox::f64 HalfB =
				    Other == 0 ? B.HalfExtents.X : (Other == 1 ? B.HalfExtents.Y : B.HalfExtents.Z);
				ProjectA += HalfA * Toolbox::Abs(AxisA.X * Raw.X + AxisA.Y * Raw.Y + AxisA.Z * Raw.Z);
				ProjectB += HalfB * Toolbox::Abs(AxisB.X * Raw.X + AxisB.Y * Raw.Y + AxisB.Z * Raw.Z);
			}
			const Toolbox::f64 Separation = Toolbox::Abs(Distance) - ProjectA - ProjectB;
			if (Separation > BestSeparation)
			{
				BestSeparation = Separation;
				// AからBへ向く軸方向。
				const Toolbox::f64 Sign = Distance >= 0 ? 1 : -1;
				BestDirection = {Raw.X * Sign, Raw.Y * Sign, Raw.Z * Sign};
				bBestOnA = Owner == 0;
				BestFaceAxis = Axis;
				bBestIsFace = true;
			}
		}
	}
	// 辺同士の軸。
	for (Toolbox::int32 AxisA = 0; AxisA < 3; ++AxisA)
	{
		for (Toolbox::int32 AxisB = 0; AxisB < 3; ++AxisB)
		{
			const FVector3D RawA = AxisD_Internal(A.Axes[AxisA]);
			const FVector3D RawB = AxisD_Internal(B.Axes[AxisB]);
			FVector3D Cross = {RawA.Y * RawB.Z - RawA.Z * RawB.Y, RawA.Z * RawB.X - RawA.X * RawB.Z,
			                   RawA.X * RawB.Y - RawA.Y * RawB.X};
			const Toolbox::f64 Length = Toolbox::Sqrt(Cross.X * Cross.X + Cross.Y * Cross.Y + Cross.Z * Cross.Z);
			if (!(Length > 1e-9))
			{
				continue;
			}
			Cross.X /= Length;
			Cross.Y /= Length;
			Cross.Z /= Length;
			// 軸方向の中心距離。
			const Toolbox::f64 Distance = Delta.X * Cross.X + Delta.Y * Cross.Y + Delta.Z * Cross.Z;
			// 両箱の軸への投影半径。
			Toolbox::f64 ProjectA = 0;
			Toolbox::f64 ProjectB = 0;
			for (Toolbox::int32 Other = 0; Other < 3; ++Other)
			{
				const FVector3D AxisA = AxisD_Internal(A.Axes[Other]);
				const FVector3D AxisB = AxisD_Internal(B.Axes[Other]);
				const Toolbox::f64 HalfA =
				    Other == 0 ? A.HalfExtents.X : (Other == 1 ? A.HalfExtents.Y : A.HalfExtents.Z);
				const Toolbox::f64 HalfB =
				    Other == 0 ? B.HalfExtents.X : (Other == 1 ? B.HalfExtents.Y : B.HalfExtents.Z);
				ProjectA += HalfA * Toolbox::Abs(AxisA.X * Cross.X + AxisA.Y * Cross.Y + AxisA.Z * Cross.Z);
				ProjectB += HalfB * Toolbox::Abs(AxisB.X * Cross.X + AxisB.Y * Cross.Y + AxisB.Z * Cross.Z);
			}
			const Toolbox::f64 Separation = Toolbox::Abs(Distance) - ProjectA - ProjectB;
			if (Separation > BestSeparation)
			{
				BestSeparation = Separation;
				// AからBへ向く軸方向。
				const Toolbox::f64 Sign = Distance >= 0 ? 1 : -1;
				BestDirection = {Cross.X * Sign, Cross.Y * Sign, Cross.Z * Sign};
				bBestIsFace = false;
			}
		}
	}
	if (BestSeparation > Slop)
	{
		return;
	}
	if (!bBestIsFace)
	{
		// 辺同士は両支持点の中点を近似接触にする。
		const FVector3D Negative = {-BestDirection.X, -BestDirection.Y, -BestDirection.Z};
		const FVector3D PointA = Support_Internal(A, BestDirection);
		const FVector3D PointB = Support_Internal(B, Negative);
		Toolbox::FContactPoint3D Hit;
		Hit.Position = {static_cast<Toolbox::f32>((PointA.X + PointB.X) * 0.5),
		                static_cast<Toolbox::f32>((PointA.Y + PointB.Y) * 0.5),
		                static_cast<Toolbox::f32>((PointA.Z + PointB.Z) * 0.5)};
		Hit.Normal = {static_cast<Toolbox::f32>(-BestDirection.X), static_cast<Toolbox::f32>(-BestDirection.Y),
		              static_cast<Toolbox::f32>(-BestDirection.Z)};
		Hit.Separation = static_cast<Toolbox::f32>(BestSeparation);
		Hit.FeatureId = 64;
		Out.PushBack(Hit);
		return;
	}
	// 基準面を持つ箱と入射面を持つ箱を決める。
	const Toolbox::FOBB& Reference = bBestOnA ? A : B;
	const Toolbox::FOBB& Incident = bBestOnA ? B : A;
	// 基準面の外向き法線はAからBへ向く。
	FVector3D FaceNormal = BestDirection;
	if (!bBestOnA)
	{
		FaceNormal = {-BestDirection.X, -BestDirection.Y, -BestDirection.Z};
	}
	// 基準面の番号。所有箱の軸番号に符号を足す。
	const FVector3D OwnerAxis = AxisD_Internal((bBestOnA ? A : B).Axes[BestFaceAxis]);
	const Toolbox::f64 Outward = OwnerAxis.X * FaceNormal.X + OwnerAxis.Y * FaceNormal.Y + OwnerAxis.Z * FaceNormal.Z;
	const Toolbox::int32 ReferenceFace = BestFaceAxis * 2 + (Outward > 0 ? 1 : 0);
	// 基準面の中心と接線。
	const FVector3D AxisN = AxisD_Internal(Reference.Axes[ReferenceFace / 2]);
	const Toolbox::f64 HalfN = ReferenceFace / 2 == 0 ? Reference.HalfExtents.X
	                           : (ReferenceFace / 2 == 1 ? Reference.HalfExtents.Y : Reference.HalfExtents.Z);
	FVector3D FaceCenter = {Reference.Center.X, Reference.Center.Y, Reference.Center.Z};
	FaceCenter.X += AxisN.X * (Outward > 0 ? HalfN : -HalfN);
	FaceCenter.Y += AxisN.Y * (Outward > 0 ? HalfN : -HalfN);
	FaceCenter.Z += AxisN.Z * (Outward > 0 ? HalfN : -HalfN);
	// 接線になる二軸。
	const Toolbox::int32 TangentA = (ReferenceFace / 2 + 1) % 3;
	const Toolbox::int32 TangentB = (ReferenceFace / 2 + 2) % 3;
	const FVector3D AxisA = AxisD_Internal(Reference.Axes[TangentA]);
	const FVector3D AxisB = AxisD_Internal(Reference.Axes[TangentB]);
	const Toolbox::f64 HalfA =
	    TangentA == 0 ? Reference.HalfExtents.X : (TangentA == 1 ? Reference.HalfExtents.Y : Reference.HalfExtents.Z);
	const Toolbox::f64 HalfB =
	    TangentB == 0 ? Reference.HalfExtents.X : (TangentB == 1 ? Reference.HalfExtents.Y : Reference.HalfExtents.Z);
	// 入射面は基準法線に最も逆らう面の四隅。
	Toolbox::f64 BestDot = 1e30;
	Toolbox::int32 IncidentFace = 0;
	for (Toolbox::int32 Face = 0; Face < 6; ++Face)
	{
		const FVector3D Axis = AxisD_Internal(Incident.Axes[Face / 2]);
		const Toolbox::f64 Sign = (Face & 1) == 0 ? -1 : 1;
		const Toolbox::f64 Alignment = (Axis.X * Sign) * FaceNormal.X + (Axis.Y * Sign) * FaceNormal.Y +
		                               (Axis.Z * Sign) * FaceNormal.Z;
		if (Alignment < BestDot)
		{
			BestDot = Alignment;
			IncidentFace = Face;
		}
	}
	(void)IncidentFace;
	FVector3D Quad[4];
	FaceVertices_Internal(Incident, IncidentFace, Quad);
	// 基準面の四つの側方平面で切り取る。頂点の環状順序を保つ単一走査。
	FVector3D Kept[8];
	FVector3D Spare[8];
	for (Toolbox::int32 Vertex = 0; Vertex < 4; ++Vertex)
	{
		Kept[Vertex] = Quad[Vertex];
	}
	Toolbox::int32 KeptCount = 4;
	for (Toolbox::int32 Side = 0; Side < 4; ++Side)
	{
		// 側方平面の法線と境界。
		const FVector3D PlaneAxis = (Side & 2) == 0 ? AxisA : AxisB;
		const Toolbox::f64 PlaneHalf = (Side & 2) == 0 ? HalfA : HalfB;
		const Toolbox::f64 PlaneSign = (Side & 1) == 0 ? 1 : -1;
		Toolbox::int32 SpareCount = 0;
		for (Toolbox::int32 Index = 0; Index < KeptCount; ++Index)
		{
			const Toolbox::int32 Next = (Index + 1) % KeptCount;
			// 面中心からの側方距離。
			const Toolbox::f64 LateralA = (Kept[Index].X - FaceCenter.X) * PlaneAxis.X * PlaneSign +
			                              (Kept[Index].Y - FaceCenter.Y) * PlaneAxis.Y * PlaneSign +
			                              (Kept[Index].Z - FaceCenter.Z) * PlaneAxis.Z * PlaneSign;
			const Toolbox::f64 LateralB = (Kept[Next].X - FaceCenter.X) * PlaneAxis.X * PlaneSign +
			                              (Kept[Next].Y - FaceCenter.Y) * PlaneAxis.Y * PlaneSign +
			                              (Kept[Next].Z - FaceCenter.Z) * PlaneAxis.Z * PlaneSign;
			const bool bInsideA = LateralA <= PlaneHalf;
			const bool bInsideB = LateralB <= PlaneHalf;
			if (bInsideA && SpareCount < 8)
			{
				Spare[SpareCount] = Kept[Index];
				++SpareCount;
			}
			if (bInsideA != bInsideB && SpareCount < 8)
			{
				const Toolbox::f64 Fraction = (PlaneHalf - LateralA) / (LateralB - LateralA);
				Spare[SpareCount].X = Kept[Index].X + (Kept[Next].X - Kept[Index].X) * Fraction;
				Spare[SpareCount].Y = Kept[Index].Y + (Kept[Next].Y - Kept[Index].Y) * Fraction;
				Spare[SpareCount].Z = Kept[Index].Z + (Kept[Next].Z - Kept[Index].Z) * Fraction;
				++SpareCount;
			}
		}
		for (Toolbox::int32 Index = 0; Index < SpareCount && Index < 8; ++Index)
		{
			Kept[Index] = Spare[Index];
		}
		KeptCount = SpareCount;
		if (KeptCount == 0)
		{
			return;
		}
	}
	// 法線はBからAへ向ける。最良軸はA→Bで作るため反転して保つ。
	const FVector3D Normal = {-BestDirection.X, -BestDirection.Y, -BestDirection.Z};
	// 特徴IDは基準面側の面番号に切り取り順を足す。
	const Toolbox::uint32 Base =
	    static_cast<Toolbox::uint32>((bBestOnA ? ReferenceFace : 8 + ReferenceFace) * 4);
	// 深い順に最大四点を選び、切り取り順で出す。
	Toolbox::int32 Chosen[4] = {-1, -1, -1, -1};
	Toolbox::int32 ChosenCount = 0;
	for (Toolbox::int32 Round = 0; Round < 4; ++Round)
	{
		Toolbox::f64 BestDepth = 1e30;
		Toolbox::int32 BestVertex = -1;
		for (Toolbox::int32 Index = 0; Index < KeptCount && Index < 8; ++Index)
		{
			bool bUsed = false;
			for (Toolbox::int32 Used = 0; Used < ChosenCount; ++Used)
			{
				if (Chosen[Used] == Index)
				{
					bUsed = true;
				}
			}
			if (bUsed)
			{
				continue;
			}
			// A面からB表面への符号付き距離。基準面がB側の場合は差の向きを入れ替える。
			const Toolbox::f64 DeltaX = bBestOnA ? FaceCenter.X - Kept[Index].X : Kept[Index].X - FaceCenter.X;
			const Toolbox::f64 DeltaY = bBestOnA ? FaceCenter.Y - Kept[Index].Y : Kept[Index].Y - FaceCenter.Y;
			const Toolbox::f64 DeltaZ = bBestOnA ? FaceCenter.Z - Kept[Index].Z : Kept[Index].Z - FaceCenter.Z;
			const Toolbox::f64 Separation = DeltaX * Normal.X + DeltaY * Normal.Y + DeltaZ * Normal.Z;
			if (Separation <= Slop && Separation < BestDepth)
			{
				BestDepth = Separation;
				BestVertex = Index;
			}
		}
		if (BestVertex < 0)
		{
			break;
		}
		Chosen[ChosenCount] = BestVertex;
		++ChosenCount;
	}
	// 切り取り順に並べ直して安定させる。
	for (Toolbox::int32 Slot = 0; Slot < ChosenCount; ++Slot)
	{
		for (Toolbox::int32 Other = Slot + 1; Other < ChosenCount; ++Other)
		{
			if (Chosen[Other] < Chosen[Slot])
			{
				const Toolbox::int32 Temporary = Chosen[Slot];
				Chosen[Slot] = Chosen[Other];
				Chosen[Other] = Temporary;
			}
		}
	}
	for (Toolbox::int32 Slot = 0; Slot < ChosenCount; ++Slot)
	{
		const FVector3D& Point = Kept[Chosen[Slot]];
		// A面からB表面への符号付き距離。B→A法線では貫通が負になる。
		// 基準面がB側の場合は差の向きを入れ替える。
		const Toolbox::f64 DeltaX = bBestOnA ? FaceCenter.X - Point.X : Point.X - FaceCenter.X;
		const Toolbox::f64 DeltaY = bBestOnA ? FaceCenter.Y - Point.Y : Point.Y - FaceCenter.Y;
		const Toolbox::f64 DeltaZ = bBestOnA ? FaceCenter.Z - Point.Z : Point.Z - FaceCenter.Z;
		const Toolbox::f64 Separation = DeltaX * Normal.X + DeltaY * Normal.Y + DeltaZ * Normal.Z;
		Toolbox::FContactPoint3D Hit;
		Hit.Position = {static_cast<Toolbox::f32>(Point.X), static_cast<Toolbox::f32>(Point.Y),
		                static_cast<Toolbox::f32>(Point.Z)};
		Hit.Normal = {static_cast<Toolbox::f32>(Normal.X), static_cast<Toolbox::f32>(Normal.Y),
		              static_cast<Toolbox::f32>(Normal.Z)};
		Hit.Separation = static_cast<Toolbox::f32>(Separation);
		Hit.FeatureId = Base + static_cast<Toolbox::uint32>(Chosen[Slot]);
		Out.PushBack(Hit);
	}
}
// 立体剛体の登録スロットと接触解決をまとめた実装。
struct FPhysicsWorld3D::FImpl
{
	// 別ワールドのID混入を検出する識別子。
	Toolbox::uint64 World = NextWorld_Internal();
	// スロット番号で直接参照する登録領域。
	Toolbox::TVector<FBodyRecord3D> Slots;
	// 再使用可能な空きスロット番号。
	Toolbox::TVector<Toolbox::size_t> Free;
	// スロット番号で直接参照するコライダー領域。
	Toolbox::TVector<FColliderRecord3D> Colliders;
	// 再使用可能な空きコライダー番号。
	Toolbox::TVector<Toolbox::size_t> ColliderFree;
	// 前回Impulseの再利用記録。
	Toolbox::TVector<FCachedImpulse3D> Cache;
	// ワールド全体の重力加速度。
	Toolbox::FVector3 Gravity{0, -9.8f, 0};
	// 接触拘束の解決設定。
	FContactSettings3D Contact;
	// 連続衝突の反復設定。
	FContinuousSettings3D Continuous;
	// 直近更新の連続衝突診断。
	FContinuousDiagnostics3D Diagnostics;
	// 休止の条件。
	FSleepSettings3D Sleep;
	// 新しいワールドへ重ならない識別子を発行する。
	static Toolbox::uint64 NextWorld_Internal()
	{
		// 単一スレッド利用を前提とした通し番号。
		static Toolbox::uint64 Next = 1;
		const Toolbox::uint64 Issued = Next;
		Next += 1;
		return Issued;
	}
	// IDが有効な登録を指す場合だけ記録を返す。
	FBodyRecord3D* Find_Internal(FBodyId3D Id) noexcept
	{
		if (Id.World != World)
		{
			return nullptr;
		}
		if (Id.Index >= Slots.Size())
		{
			return nullptr;
		}
		// 世代が一致する有効な登録。
		FBodyRecord3D& Record = Slots[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
	}
	// IDが有効な登録を指す場合だけ記録を返す。
	const FBodyRecord3D* Find_Internal(FBodyId3D Id) const noexcept
	{
		if (Id.World != World)
		{
			return nullptr;
		}
		if (Id.Index >= Slots.Size())
		{
			return nullptr;
		}
		// 世代が一致する有効な登録。
		const FBodyRecord3D& Record = Slots[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
	}
	// 有効な記録を返す。期限切れIDは例外で通知する。
	FBodyRecord3D& Resolve_Internal(FBodyId3D Id)
	{
		FBodyRecord3D* Record = Find_Internal(Id);
		if (Record == nullptr)
		{
			throw Toolbox::FException("Invalid 3D body id");
		}
		return *Record;
	}
	// 有効な記録を返す。期限切れIDは例外で通知する。
	const FBodyRecord3D& Resolve_Internal(FBodyId3D Id) const
	{
		const FBodyRecord3D* Record = Find_Internal(Id);
		if (Record == nullptr)
		{
			throw Toolbox::FException("Invalid 3D body id");
		}
		return *Record;
	}
	// IDが有効な登録を指す場合だけコライダーを返す。
	FColliderRecord3D* FindCollider_Internal(FColliderId3D Id) noexcept
	{
		return const_cast<FColliderRecord3D*>(static_cast<const FImpl*>(this)->FindCollider_Internal(Id));
	}
	// IDが有効な登録を指す場合だけコライダーを返す。
	const FColliderRecord3D* FindCollider_Internal(FColliderId3D Id) const noexcept
	{
		if (Id.Body.World != World)
		{
			return nullptr;
		}
		if (Id.Index >= Colliders.Size())
		{
			return nullptr;
		}
		// 世代と取り付け先が一致する有効な登録。
		const FColliderRecord3D& Record = Colliders[Id.Index];
		const bool bMatches = Record.bAlive && Record.Generation == Id.Generation && Record.Body == Id.Body;
		return bMatches ? &Record : nullptr;
	}
	// 正準順序が小さい方か調べる。
	static bool ColliderLess_Internal(const FColliderId3D& A, const FColliderId3D& B) noexcept
	{
		if (A.Body.Index != B.Body.Index)
		{
			return A.Body.Index < B.Body.Index;
		}
		if (A.Body.Generation != B.Body.Generation)
		{
			return A.Body.Generation < B.Body.Generation;
		}
		if (A.Index != B.Index)
		{
			return A.Index < B.Index;
		}
		return A.Generation < B.Generation;
	}
	// ローカル球をワールド形状へ変換する。
	static Toolbox::FSphere ToWorld_Internal(const FBodyRecord3D& Body, const Toolbox::FSphere& Local)
	{
		Toolbox::FSphere World = Local;
		// 姿勢で相対中心を回転する。
		const Toolbox::FVector3 Offset = Body.Orientation.Rotate({Local.Center.X, Local.Center.Y, Local.Center.Z});
		World.Center = {Body.Position.X + Offset.X, Body.Position.Y + Offset.Y, Body.Position.Z + Offset.Z};
		return World;
	}
	// ローカル箱をワールド形状へ変換する。
	static Toolbox::FOBB ToWorld_Internal(const FBodyRecord3D& Body, const Toolbox::FOBB& Local)
	{
		Toolbox::FOBB World = Local;
		// 中心の相対位置を姿勢で回転する。
		const Toolbox::FVector3 Offset = Body.Orientation.Rotate({Local.Center.X, Local.Center.Y, Local.Center.Z});
		World.Center = {Body.Position.X + Offset.X, Body.Position.Y + Offset.Y, Body.Position.Z + Offset.Z};
		// 箱軸も姿勢で回転する。
		for (Toolbox::size_t Axis = 0; Axis < 3; ++Axis)
		{
			World.Axes[Axis] = Body.Orientation.Rotate(Local.Axes[Axis]);
		}
		return World;
	}
	// ワールド逆慣性でベクトルを変換する。
	static FVector3D WorldInverseInertia_Internal(const FBodyRecord3D& Record, FVector3D Value) noexcept
	{
		const FQuaternionD Q = ToDouble_Internal(Record.Orientation);
		const FVector3D Inverse = {Record.InverseDiagonalInertia.X, Record.InverseDiagonalInertia.Y,
		                           Record.InverseDiagonalInertia.Z};
		return TransformDiagonal_Internal(Q, Inverse, Value);
	}
	// 接触点の相対速度を求める。
	static Toolbox::FVector3 RelativeVelocity_Internal(const FBodyRecord3D& BodyA, const FBodyRecord3D& BodyB,
	                                                   Toolbox::FVector3 Point) noexcept
	{
		// 腕の回転による速度。
		const Toolbox::f64 ArmAX = Toolbox::f64(Point.X) - BodyA.Position.X;
		const Toolbox::f64 ArmAY = Toolbox::f64(Point.Y) - BodyA.Position.Y;
		const Toolbox::f64 ArmAZ = Toolbox::f64(Point.Z) - BodyA.Position.Z;
		const Toolbox::f64 ArmBX = Toolbox::f64(Point.X) - BodyB.Position.X;
		const Toolbox::f64 ArmBY = Toolbox::f64(Point.Y) - BodyB.Position.Y;
		const Toolbox::f64 ArmBZ = Toolbox::f64(Point.Z) - BodyB.Position.Z;
		// 角速度と腕の外積。
		const Toolbox::f64 SpinAX = Toolbox::f64(BodyA.AngularVelocity.Y) * ArmAZ - Toolbox::f64(BodyA.AngularVelocity.Z) * ArmAY;
		const Toolbox::f64 SpinAY = Toolbox::f64(BodyA.AngularVelocity.Z) * ArmAX - Toolbox::f64(BodyA.AngularVelocity.X) * ArmAZ;
		const Toolbox::f64 SpinAZ = Toolbox::f64(BodyA.AngularVelocity.X) * ArmAY - Toolbox::f64(BodyA.AngularVelocity.Y) * ArmAX;
		const Toolbox::f64 SpinBX = Toolbox::f64(BodyB.AngularVelocity.Y) * ArmBZ - Toolbox::f64(BodyB.AngularVelocity.Z) * ArmBY;
		const Toolbox::f64 SpinBY = Toolbox::f64(BodyB.AngularVelocity.Z) * ArmBX - Toolbox::f64(BodyB.AngularVelocity.X) * ArmBZ;
		const Toolbox::f64 SpinBZ = Toolbox::f64(BodyB.AngularVelocity.X) * ArmBY - Toolbox::f64(BodyB.AngularVelocity.Y) * ArmBX;
		return {static_cast<Toolbox::f32>(Toolbox::f64(BodyA.Velocity.X) + SpinAX - Toolbox::f64(BodyB.Velocity.X) - SpinBX),
		        static_cast<Toolbox::f32>(Toolbox::f64(BodyA.Velocity.Y) + SpinAY - Toolbox::f64(BodyB.Velocity.Y) - SpinBY),
		        static_cast<Toolbox::f32>(Toolbox::f64(BodyA.Velocity.Z) + SpinAZ - Toolbox::f64(BodyB.Velocity.Z) - SpinBZ)};
	}
	// 速度へImpulseを適用する。一つ目に足し、二つ目から引く。
	static void ApplyImpulse_Internal(FBodyRecord3D& BodyA, FBodyRecord3D& BodyB, Toolbox::FVector3 Point,
	                                  FVector3D Push) noexcept
	{
		// 休止中は無限質量として扱う。
		const Toolbox::f32 InverseMassA = EffectiveInverseMass_Internal(BodyA);
		const Toolbox::f32 InverseMassB = EffectiveInverseMass_Internal(BodyB);
		const FVector3D InverseDiagonalA = EffectiveInverseDiagonal_Internal(BodyA);
		const FVector3D InverseDiagonalB = EffectiveInverseDiagonal_Internal(BodyB);
		// 腕。
		const FVector3D ArmA = {Toolbox::f64(Point.X) - BodyA.Position.X, Toolbox::f64(Point.Y) - BodyA.Position.Y,
		                        Toolbox::f64(Point.Z) - BodyA.Position.Z};
		const FVector3D ArmB = {Toolbox::f64(Point.X) - BodyB.Position.X, Toolbox::f64(Point.Y) - BodyB.Position.Y,
		                        Toolbox::f64(Point.Z) - BodyB.Position.Z};
		// 並進への反映。
		BodyA.Velocity += {static_cast<Toolbox::f32>(Push.X * InverseMassA),
		                   static_cast<Toolbox::f32>(Push.Y * InverseMassA),
		                   static_cast<Toolbox::f32>(Push.Z * InverseMassA)};
		BodyB.Velocity += {static_cast<Toolbox::f32>(-Push.X * InverseMassB),
		                   static_cast<Toolbox::f32>(-Push.Y * InverseMassB),
		                   static_cast<Toolbox::f32>(-Push.Z * InverseMassB)};
		// 腕とImpulseの外積をワールド逆慣性で角速度へ変換する。
		const FVector3D MomentA = {ArmA.Y * Push.Z - ArmA.Z * Push.Y, ArmA.Z * Push.X - ArmA.X * Push.Z,
		                           ArmA.X * Push.Y - ArmA.Y * Push.X};
		const FVector3D MomentB = {ArmB.Y * Push.Z - ArmB.Z * Push.Y, ArmB.Z * Push.X - ArmB.X * Push.Z,
		                           ArmB.X * Push.Y - ArmB.Y * Push.X};
		const FQuaternionD QuaternionA = ToDouble_Internal(BodyA.Orientation);
		const FQuaternionD QuaternionB = ToDouble_Internal(BodyB.Orientation);
		const FVector3D DeltaA = TransformDiagonal_Internal(QuaternionA, InverseDiagonalA, MomentA);
		const FVector3D DeltaB = TransformDiagonal_Internal(QuaternionB, InverseDiagonalB, MomentB);
		BodyA.AngularVelocity += {static_cast<Toolbox::f32>(DeltaA.X), static_cast<Toolbox::f32>(DeltaA.Y),
		                          static_cast<Toolbox::f32>(DeltaA.Z)};
		BodyB.AngularVelocity += {static_cast<Toolbox::f32>(-DeltaB.X), static_cast<Toolbox::f32>(-DeltaB.Y),
		                          static_cast<Toolbox::f32>(-DeltaB.Z)};
	}
	// 二つのコライダー組から接触点列を作る。
	void AppendPairManifold_Internal(const FColliderRecord3D& RecordA, const FColliderId3D& IdA,
	                                 const FColliderRecord3D& RecordB, const FColliderId3D& IdB,
	                                 const FBodyRecord3D& BodyA, const FBodyRecord3D& BodyB, FManifold3D& Manifold)
	{
		Manifold.ColliderA = IdA;
		Manifold.ColliderB = IdB;
		Manifold.BodyA = RecordA.Body;
		Manifold.BodyB = RecordB.Body;
		// 摩擦は相乗平均、反発は最大値で混合する。入れ替え対称。
		const Toolbox::f32 Friction = Toolbox::Sqrt(RecordA.Friction * RecordB.Friction);
		const Toolbox::f32 Restitution =
		    RecordA.Restitution > RecordB.Restitution ? RecordA.Restitution : RecordB.Restitution;
		const Toolbox::size_t IndexA = RecordA.Shape.Index();
		const Toolbox::size_t IndexB = RecordB.Shape.Index();
		if (IndexA == 0 && IndexB == 0)
		{
			const Toolbox::FContactPoint3D Hit =
			    Toolbox::FindContact(ToWorld_Internal(BodyA, RecordA.Shape.Get<0>()),
			                         ToWorld_Internal(BodyB, RecordB.Shape.Get<0>()));
			if (Hit.Separation > Contact.ContactSlop)
			{
				return;
			}
			FSolvePoint3D Point;
			Point.Position = Hit.Position;
			Point.Normal = Hit.Normal;
			Point.Separation = Hit.Separation;
			Point.FeatureId = Hit.FeatureId;
			Point.Friction = Friction;
			Point.Restitution = Restitution;
			Manifold.Points.PushBack(Point);
		}
		else if (IndexA == 0 && IndexB == 1)
		{
			const Toolbox::FContactPoint3D Hit =
			    Toolbox::FindContact(ToWorld_Internal(BodyA, RecordA.Shape.Get<0>()),
			                         ToWorld_Internal(BodyB, RecordB.Shape.Get<1>()));
			if (Hit.Separation > Contact.ContactSlop)
			{
				return;
			}
			FSolvePoint3D Point;
			Point.Position = Hit.Position;
			Point.Normal = Hit.Normal;
			Point.Separation = Hit.Separation;
			Point.FeatureId = Hit.FeatureId;
			Point.Friction = Friction;
			Point.Restitution = Restitution;
			Manifold.Points.PushBack(Point);
		}
		else if (IndexA == 1 && IndexB == 0)
		{
			const Toolbox::FContactPoint3D Hit =
			    Toolbox::FindContact(ToWorld_Internal(BodyA, RecordA.Shape.Get<1>()),
			                         ToWorld_Internal(BodyB, RecordB.Shape.Get<0>()));
			if (Hit.Separation > Contact.ContactSlop)
			{
				return;
			}
			FSolvePoint3D Point;
			Point.Position = Hit.Position;
			Point.Normal = Hit.Normal;
			Point.Separation = Hit.Separation;
			Point.FeatureId = Hit.FeatureId;
			Point.Friction = Friction;
			Point.Restitution = Restitution;
			Manifold.Points.PushBack(Point);
		}
		else
		{
			Toolbox::TVector<Toolbox::FContactPoint3D> Hits;
			FindBoxBoxContacts_Internal(ToWorld_Internal(BodyA, RecordA.Shape.Get<1>()),
			                            ToWorld_Internal(BodyB, RecordB.Shape.Get<1>()), Contact.ContactSlop, Hits);
			for (Toolbox::size_t Index = 0; Index < Hits.Size(); ++Index)
			{
				FSolvePoint3D Point;
				Point.Position = Hits[Index].Position;
				Point.Normal = Hits[Index].Normal;
				Point.Separation = Hits[Index].Separation;
				Point.FeatureId = Hits[Index].FeatureId;
				Point.Friction = Friction;
				Point.Restitution = Restitution;
				Manifold.Points.PushBack(Point);
			}
		}
	}
	// 全コライダー組から多様体列を作る。
	void GenerateManifolds_Internal(Toolbox::TVector<FManifold3D>& Out)
	{
		for (Toolbox::size_t First = 0; First < Colliders.Size(); ++First)
		{
			FColliderRecord3D& RecordA = Colliders[First];
			if (!RecordA.bAlive)
			{
				continue;
			}
			FBodyRecord3D* BodyA = Find_Internal(RecordA.Body);
			if (BodyA == nullptr)
			{
				continue;
			}
			for (Toolbox::size_t Second = First + 1; Second < Colliders.Size(); ++Second)
			{
				FColliderRecord3D& RecordB = Colliders[Second];
				if (!RecordB.bAlive)
				{
					continue;
				}
				FBodyRecord3D* BodyB = Find_Internal(RecordB.Body);
				if (BodyB == nullptr)
				{
					continue;
				}
				// 同一剛体の組は自分自身へ接触しない。
				if (RecordA.Body == RecordB.Body)
				{
					continue;
				}
				// 両方が非Dynamicの組は応答も運動もしない。
				if (BodyA->Type != EBodyType::Dynamic && BodyB->Type != EBodyType::Dynamic)
				{
					continue;
				}
				const FColliderId3D IdA = {RecordA.Body, First, RecordA.Generation};
				const FColliderId3D IdB = {RecordB.Body, Second, RecordB.Generation};
				FManifold3D Manifold;
				if (ColliderLess_Internal(IdB, IdA))
				{
					AppendPairManifold_Internal(RecordB, IdB, RecordA, IdA, *BodyB, *BodyA, Manifold);
				}
				else
				{
					AppendPairManifold_Internal(RecordA, IdA, RecordB, IdB, *BodyA, *BodyB, Manifold);
				}
				if (!Manifold.Points.IsEmpty())
				{
					Out.PushBack(Manifold);
				}
			}
		}
	}
	// 法線に直交する接平面の基底を作る。
	static void TangentBasis_Internal(FVector3D Normal, FVector3D& U, FVector3D& V) noexcept
	{
		// 法線に最も直交する軸を選ぶ。
		FVector3D Axis = {1, 0, 0};
		const Toolbox::f64 AbsX = Normal.X < 0 ? -Normal.X : Normal.X;
		const Toolbox::f64 AbsY = Normal.Y < 0 ? -Normal.Y : Normal.Y;
		const Toolbox::f64 AbsZ = Normal.Z < 0 ? -Normal.Z : Normal.Z;
		if (AbsY <= AbsX && AbsY <= AbsZ)
		{
			Axis = {0, 1, 0};
		}
		else if (AbsZ <= AbsX && AbsZ <= AbsY)
		{
			Axis = {0, 0, 1};
		}
		// 外積を正規化する。
		FVector3D Cross = {Normal.Y * Axis.Z - Normal.Z * Axis.Y, Normal.Z * Axis.X - Normal.X * Axis.Z,
		                   Normal.X * Axis.Y - Normal.Y * Axis.X};
		const Toolbox::f64 Length = Toolbox::Sqrt(Cross.X * Cross.X + Cross.Y * Cross.Y + Cross.Z * Cross.Z);
		if (Length > 1e-12)
		{
			Cross.X /= Length;
			Cross.Y /= Length;
			Cross.Z /= Length;
		}
		U = Cross;
		V = {Normal.Y * U.Z - Normal.Z * U.Y, Normal.Z * U.X - Normal.X * U.Z, Normal.X * U.Y - Normal.Y * U.X};
	}
	// 休止中の剛体を起こす。
	static void Wake_Internal(FBodyRecord3D& Record) noexcept
	{
		Record.bSleeping = false;
		Record.SleepTimer = 0;
	}
	// 全登録の休止を解く。設定変更で凍結を残さない。
	void WakeAll_Internal() noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			FBodyRecord3D& Record = Slots[Index];
			if (Record.bAlive)
			{
				Wake_Internal(Record);
			}
		}
	}
	// 前回確定の接触点相対運動で起床が必要か調べる。島伝播の第一段階。
	// Kinematicは力積分を受けないため現在速度を使い、Dynamicは今刻みの
	// 重力・外力分を除くため前回確定値を使う。休止中は止まっている。
	bool ShouldWakeForMotion_Internal(const FBodyRecord3D& BodyA, const FBodyRecord3D& BodyB,
	                                  Toolbox::FVector3 Point) const noexcept
	{
		if (!BodyA.bSleeping && !BodyB.bSleeping)
		{
			return false;
		}
		Toolbox::FVector3 PrevVelA = BodyA.Type == EBodyType::Kinematic ? BodyA.Velocity : BodyA.PrevVelocity;
		Toolbox::FVector3 PrevSpinA = BodyA.Type == EBodyType::Kinematic ? BodyA.AngularVelocity : BodyA.PrevAngularVelocity;
		Toolbox::FVector3 PrevVelB = BodyB.Type == EBodyType::Kinematic ? BodyB.Velocity : BodyB.PrevVelocity;
		Toolbox::FVector3 PrevSpinB = BodyB.Type == EBodyType::Kinematic ? BodyB.AngularVelocity : BodyB.PrevAngularVelocity;
		if (BodyA.bSleeping)
		{
			PrevVelA = {};
			PrevSpinA = {};
		}
		if (BodyB.bSleeping)
		{
			PrevVelB = {};
			PrevSpinB = {};
		}
		const FVector3D ArmA = {Toolbox::f64(Point.X) - BodyA.Position.X, Toolbox::f64(Point.Y) - BodyA.Position.Y,
		                        Toolbox::f64(Point.Z) - BodyA.Position.Z};
		const FVector3D ArmB = {Toolbox::f64(Point.X) - BodyB.Position.X, Toolbox::f64(Point.Y) - BodyB.Position.Y,
		                        Toolbox::f64(Point.Z) - BodyB.Position.Z};
		const FVector3D VelA = {Toolbox::f64(PrevVelA.X) + Toolbox::f64(PrevSpinA.Y) * ArmA.Z - Toolbox::f64(PrevSpinA.Z) * ArmA.Y,
		                        Toolbox::f64(PrevVelA.Y) + Toolbox::f64(PrevSpinA.Z) * ArmA.X - Toolbox::f64(PrevSpinA.X) * ArmA.Z,
		                        Toolbox::f64(PrevVelA.Z) + Toolbox::f64(PrevSpinA.X) * ArmA.Y - Toolbox::f64(PrevSpinA.Y) * ArmA.X};
		const FVector3D VelB = {Toolbox::f64(PrevVelB.X) + Toolbox::f64(PrevSpinB.Y) * ArmB.Z - Toolbox::f64(PrevSpinB.Z) * ArmB.Y,
		                        Toolbox::f64(PrevVelB.Y) + Toolbox::f64(PrevSpinB.Z) * ArmB.X - Toolbox::f64(PrevSpinB.X) * ArmB.Z,
		                        Toolbox::f64(PrevVelB.Z) + Toolbox::f64(PrevSpinB.X) * ArmB.Y - Toolbox::f64(PrevSpinB.Y) * ArmB.X};
		const Toolbox::f64 GapX = VelA.X - VelB.X;
		const Toolbox::f64 GapY = VelA.Y - VelB.Y;
		const Toolbox::f64 GapZ = VelA.Z - VelB.Z;
		return Toolbox::Sqrt(GapX * GapX + GapY * GapY + GapZ * GapZ) > Sleep.LinearSpeedLimit;
	}
	// 休止中は無限質量として扱う逆質量を返す。
	static Toolbox::f32 EffectiveInverseMass_Internal(const FBodyRecord3D& Record) noexcept
	{
		return Record.bSleeping ? 0 : Record.InverseMass;
	}
	// 休止中は無限慣性として扱う逆対角慣性を返す。
	static FVector3D EffectiveInverseDiagonal_Internal(const FBodyRecord3D& Record) noexcept
	{
		if (Record.bSleeping)
		{
			FVector3D Zero = {0, 0, 0};
			return Zero;
		}
		FVector3D Inverse = {Record.InverseDiagonalInertia.X, Record.InverseDiagonalInertia.Y,
		                     Record.InverseDiagonalInertia.Z};
		return Inverse;
	}
	// 前回Impulseを適用し、反発目標の基準速度を保存する。
	void WarmStart_Internal(FManifold3D& Manifold)
	{
		FBodyRecord3D* BodyA = Find_Internal(Manifold.BodyA);
		FBodyRecord3D* BodyB = Find_Internal(Manifold.BodyB);
		if (BodyA == nullptr || BodyB == nullptr)
		{
			return;
		}
		// 新しい接触は休止中の剛体を起こす。一方向ずつの伝播で島へ広がる。
		bool bKnownPair = false;
		for (Toolbox::size_t Index = 0; Index < Manifold.Points.Size(); ++Index)
		{
			FSolvePoint3D& Point = Manifold.Points[Index];
			const Toolbox::FVector3 Relative = RelativeVelocity_Internal(*BodyA, *BodyB, Point.Position);
			Point.ApproachSpeed = Toolbox::f64(Relative.X) * Point.Normal.X + Toolbox::f64(Relative.Y) * Point.Normal.Y +
			                      Toolbox::f64(Relative.Z) * Point.Normal.Z;
			if (ShouldWakeForMotion_Internal(*BodyA, *BodyB, Point.Position))
			{
				Wake_Internal(*BodyA);
				Wake_Internal(*BodyB);
			}
			Point.NormalImpulse = 0;
			Point.FrictionImpulse = {};
			for (Toolbox::size_t CacheIndex = 0; CacheIndex < Cache.Size(); ++CacheIndex)
			{
				const FCachedImpulse3D& Cached = Cache[CacheIndex];
				const bool bSamePair = Cached.ColliderA == Manifold.ColliderA && Cached.ColliderB == Manifold.ColliderB;
				if (!bSamePair || Cached.FeatureId != Point.FeatureId)
				{
					continue;
				}
				if (Cached.BodyGenerationA != BodyA->Generation || Cached.BodyGenerationB != BodyB->Generation)
				{
					continue;
				}
				// 法線が大きく変わった接触は再利用しない。
				const Toolbox::f64 Agreement = Toolbox::f64(Cached.Normal.X) * Point.Normal.X +
				                              Toolbox::f64(Cached.Normal.Y) * Point.Normal.Y +
				                              Toolbox::f64(Cached.Normal.Z) * Point.Normal.Z;
				if (Agreement < 0.99)
				{
					continue;
				}
				Point.NormalImpulse = Cached.NormalImpulse;
				Point.FrictionImpulse = Cached.FrictionImpulse;
				// 保存したImpulseを即時適用する。
				const FVector3D Push = {Toolbox::f64(Point.Normal.X) * Point.NormalImpulse + Cached.FrictionImpulse.X,
				                        Toolbox::f64(Point.Normal.Y) * Point.NormalImpulse + Cached.FrictionImpulse.Y,
				                        Toolbox::f64(Point.Normal.Z) * Point.NormalImpulse + Cached.FrictionImpulse.Z};
				ApplyImpulse_Internal(*BodyA, *BodyB, Point.Position, Push);
				bKnownPair = true;
				break;
			}
		}
		if (!bKnownPair)
		{
			// 未知の接触は両側を起こす。
			Wake_Internal(*BodyA);
			Wake_Internal(*BodyB);
		}
	}
	// 単一接触点の速度拘束を解く。
	static void SolvePoint_Internal(FBodyRecord3D& BodyA, FBodyRecord3D& BodyB, FSolvePoint3D& Point,
	                                Toolbox::f32 RestitutionThreshold) noexcept
	{
		const FVector3D Normal = {Point.Normal.X, Point.Normal.Y, Point.Normal.Z};
		// 休止中は無限質量として扱う。
		const Toolbox::f64 InverseMassA = EffectiveInverseMass_Internal(BodyA);
		const Toolbox::f64 InverseMassB = EffectiveInverseMass_Internal(BodyB);
		const FQuaternionD QuaternionA = ToDouble_Internal(BodyA.Orientation);
		const FQuaternionD QuaternionB = ToDouble_Internal(BodyB.Orientation);
		const FVector3D InverseDiagonalA = EffectiveInverseDiagonal_Internal(BodyA);
		const FVector3D InverseDiagonalB = EffectiveInverseDiagonal_Internal(BodyB);
		// 腕。
		const FVector3D ArmA = {Toolbox::f64(Point.Position.X) - BodyA.Position.X,
		                        Toolbox::f64(Point.Position.Y) - BodyA.Position.Y,
		                        Toolbox::f64(Point.Position.Z) - BodyA.Position.Z};
		const FVector3D ArmB = {Toolbox::f64(Point.Position.X) - BodyB.Position.X,
		                        Toolbox::f64(Point.Position.Y) - BodyB.Position.Y,
		                        Toolbox::f64(Point.Position.Z) - BodyB.Position.Z};
		// 腕と法線の外積。
		const FVector3D CrossNA = {ArmA.Y * Normal.Z - ArmA.Z * Normal.Y, ArmA.Z * Normal.X - ArmA.X * Normal.Z,
		                           ArmA.X * Normal.Y - ArmA.Y * Normal.X};
		const FVector3D CrossNB = {ArmB.Y * Normal.Z - ArmB.Z * Normal.Y, ArmB.Z * Normal.X - ArmB.X * Normal.Z,
		                           ArmB.X * Normal.Y - ArmB.Y * Normal.X};
		// 法線の有効質量。
		const FVector3D WeightedA = TransformDiagonal_Internal(QuaternionA, InverseDiagonalA, CrossNA);
		const FVector3D WeightedB = TransformDiagonal_Internal(QuaternionB, InverseDiagonalB, CrossNB);
		const Toolbox::f64 NormalMass = InverseMassA + InverseMassB +
		                                CrossNA.X * WeightedA.X + CrossNA.Y * WeightedA.Y + CrossNA.Z * WeightedA.Z +
		                                CrossNB.X * WeightedB.X + CrossNB.Y * WeightedB.Y + CrossNB.Z * WeightedB.Z;
		if (NormalMass <= 0)
		{
			return;
		}
		const Toolbox::FVector3 Relative = RelativeVelocity_Internal(BodyA, BodyB, Point.Position);
		const Toolbox::f64 NormalSpeed =
		    Toolbox::f64(Relative.X) * Normal.X + Toolbox::f64(Relative.Y) * Normal.Y + Toolbox::f64(Relative.Z) * Normal.Z;
		// 反発目標は反復前の接近速度から一度だけ決める。
		Toolbox::f64 Target = 0;
		if (Point.ApproachSpeed < -Toolbox::f64(RestitutionThreshold))
		{
			Target = -Toolbox::f64(Point.Restitution) * Point.ApproachSpeed;
		}
		const Toolbox::f64 Lambda = (Target - NormalSpeed) / NormalMass;
		const Toolbox::f64 Old = Point.NormalImpulse;
		Point.NormalImpulse = static_cast<Toolbox::f32>(Old + Lambda > 0 ? Old + Lambda : 0);
		const Toolbox::f64 Difference = Toolbox::f64(Point.NormalImpulse) - Old;
		ApplyImpulse_Internal(BodyA, BodyB, Point.Position,
		                      {Normal.X * Difference, Normal.Y * Difference, Normal.Z * Difference});
		// 接平面の基底。
		FVector3D U;
		FVector3D V;
		TangentBasis_Internal(Normal, U, V);
		const Toolbox::FVector3 Sliding = RelativeVelocity_Internal(BodyA, BodyB, Point.Position);
		const Toolbox::f64 SlidingU = Toolbox::f64(Sliding.X) * U.X + Toolbox::f64(Sliding.Y) * U.Y + Toolbox::f64(Sliding.Z) * U.Z;
		const Toolbox::f64 SlidingV = Toolbox::f64(Sliding.X) * V.X + Toolbox::f64(Sliding.Y) * V.Y + Toolbox::f64(Sliding.Z) * V.Z;
		// 対角近似の接線有効質量。
		const FVector3D CrossUA = {ArmA.Y * U.Z - ArmA.Z * U.Y, ArmA.Z * U.X - ArmA.X * U.Z, ArmA.X * U.Y - ArmA.Y * U.X};
		const FVector3D CrossUB = {ArmB.Y * U.Z - ArmB.Z * U.Y, ArmB.Z * U.X - ArmB.X * U.Z, ArmB.X * U.Y - ArmB.Y * U.X};
		const FVector3D CrossVA = {ArmA.Y * V.Z - ArmA.Z * V.Y, ArmA.Z * V.X - ArmA.X * V.Z, ArmA.X * V.Y - ArmA.Y * V.X};
		const FVector3D CrossVB = {ArmB.Y * V.Z - ArmB.Z * V.Y, ArmB.Z * V.X - ArmB.X * V.Z, ArmB.X * V.Y - ArmB.Y * V.X};
		const FVector3D WeightedUA = TransformDiagonal_Internal(QuaternionA, InverseDiagonalA, CrossUA);
		const FVector3D WeightedUB = TransformDiagonal_Internal(QuaternionB, InverseDiagonalB, CrossUB);
		const FVector3D WeightedVA = TransformDiagonal_Internal(QuaternionA, InverseDiagonalA, CrossVA);
		const FVector3D WeightedVB = TransformDiagonal_Internal(QuaternionB, InverseDiagonalB, CrossVB);
		const Toolbox::f64 MassU = InverseMassA + InverseMassB + CrossUA.X * WeightedUA.X +
		                           CrossUA.Y * WeightedUA.Y + CrossUA.Z * WeightedUA.Z + CrossUB.X * WeightedUB.X +
		                           CrossUB.Y * WeightedUB.Y + CrossUB.Z * WeightedUB.Z;
		const Toolbox::f64 MassV = InverseMassA + InverseMassB + CrossVA.X * WeightedVA.X +
		                           CrossVA.Y * WeightedVA.Y + CrossVA.Z * WeightedVA.Z + CrossVB.X * WeightedVB.X +
		                           CrossVB.Y * WeightedVB.Y + CrossVB.Z * WeightedVB.Z;
		if (MassU <= 0 || MassV <= 0)
		{
			return;
		}
		// 摩擦Impulseの増分。
		const Toolbox::f64 DeltaU = -SlidingU / MassU;
		const Toolbox::f64 DeltaV = -SlidingV / MassV;
		// 現在の蓄積を接平面基底へ分解する。
		const Toolbox::f64 OldU = Toolbox::f64(Point.FrictionImpulse.X) * U.X +
		                          Toolbox::f64(Point.FrictionImpulse.Y) * U.Y +
		                          Toolbox::f64(Point.FrictionImpulse.Z) * U.Z;
		const Toolbox::f64 OldV = Toolbox::f64(Point.FrictionImpulse.X) * V.X +
		                          Toolbox::f64(Point.FrictionImpulse.Y) * V.Y +
		                          Toolbox::f64(Point.FrictionImpulse.Z) * V.Z;
		// 蓄積を接平面の合成上限で制限する。
		const Toolbox::f64 Limit = Toolbox::f64(Point.Friction) * Point.NormalImpulse;
		const Toolbox::f64 AccumulatedU = OldU + DeltaU;
		const Toolbox::f64 AccumulatedV = OldV + DeltaV;
		const Toolbox::f64 AccumulatedLength = Toolbox::Sqrt(AccumulatedU * AccumulatedU + AccumulatedV * AccumulatedV);
		Toolbox::f64 ClampedU = AccumulatedU;
		Toolbox::f64 ClampedV = AccumulatedV;
		if (AccumulatedLength > Limit)
		{
			const Toolbox::f64 Scale = AccumulatedLength > 0 ? Limit / AccumulatedLength : 0;
			ClampedU *= Scale;
			ClampedV *= Scale;
		}
		const FVector3D Applied = {U.X * (ClampedU - OldU) + V.X * (ClampedV - OldV),
		                           U.Y * (ClampedU - OldU) + V.Y * (ClampedV - OldV),
		                           U.Z * (ClampedU - OldU) + V.Z * (ClampedV - OldV)};
		Point.FrictionImpulse = {static_cast<Toolbox::f32>(U.X * ClampedU + V.X * ClampedV),
		                         static_cast<Toolbox::f32>(U.Y * ClampedU + V.Y * ClampedV),
		                         static_cast<Toolbox::f32>(U.Z * ClampedU + V.Z * ClampedV)};
		ApplyImpulse_Internal(BodyA, BodyB, Point.Position, Applied);
	}
	// 多様体列の速度拘束を反復して解く。
	void SolveVelocities_Internal(Toolbox::TVector<FManifold3D>& Manifolds)
	{
		for (Toolbox::uint32 Iteration = 0; Iteration < Contact.VelocityIterations; ++Iteration)
		{
			for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
			{
				FManifold3D& Manifold = Manifolds[ManifoldIndex];
				FBodyRecord3D* BodyA = Find_Internal(Manifold.BodyA);
				FBodyRecord3D* BodyB = Find_Internal(Manifold.BodyB);
				if (BodyA == nullptr || BodyB == nullptr)
				{
					continue;
				}
				for (Toolbox::size_t PointIndex = 0; PointIndex < Manifold.Points.Size(); ++PointIndex)
				{
					SolvePoint_Internal(*BodyA, *BodyB, Manifold.Points[PointIndex], Contact.RestitutionThreshold);
				}
			}
		}
	}
	// 解決結果を再利用記録へ保存する。
	void StoreCache_Internal(const Toolbox::TVector<FManifold3D>& Manifolds)
	{
		// どの多様体にも属さない組の記録は捨てる。分離後の再接触へ
		// 古いImpulseを持ち越さない。特徴点の出入りでは捨てない。
		for (Toolbox::size_t CacheIndex = 0; CacheIndex < Cache.Size();)
		{
			const FCachedImpulse3D& Cached = Cache[CacheIndex];
			bool bActive = false;
			for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size() && !bActive; ++ManifoldIndex)
			{
				const FManifold3D& Manifold = Manifolds[ManifoldIndex];
				if (Cached.ColliderA == Manifold.ColliderA && Cached.ColliderB == Manifold.ColliderB)
				{
					bActive = true;
				}
			}
			if (bActive)
			{
				++CacheIndex;
			}
			else
			{
				Cache[CacheIndex] = Cache[Cache.Size() - 1];
				Cache.PopBack();
			}
		}
		for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
		{
			const FManifold3D& Manifold = Manifolds[ManifoldIndex];
			const FBodyRecord3D* BodyA = Find_Internal(Manifold.BodyA);
			const FBodyRecord3D* BodyB = Find_Internal(Manifold.BodyB);
			if (BodyA == nullptr || BodyB == nullptr)
			{
				continue;
			}
			for (Toolbox::size_t PointIndex = 0; PointIndex < Manifold.Points.Size(); ++PointIndex)
			{
				const FSolvePoint3D& Point = Manifold.Points[PointIndex];
				bool bStored = false;
				for (Toolbox::size_t CacheIndex = 0; CacheIndex < Cache.Size(); ++CacheIndex)
				{
					FCachedImpulse3D& Cached = Cache[CacheIndex];
					const bool bSamePair =
					    Cached.ColliderA == Manifold.ColliderA && Cached.ColliderB == Manifold.ColliderB;
					if (bSamePair && Cached.FeatureId == Point.FeatureId)
					{
						Cached.BodyGenerationA = BodyA->Generation;
						Cached.BodyGenerationB = BodyB->Generation;
						Cached.Normal = Point.Normal;
						Cached.NormalImpulse = Point.NormalImpulse;
						Cached.FrictionImpulse = Point.FrictionImpulse;
						bStored = true;
						break;
					}
				}
				if (!bStored)
				{
					// 記録が増えすぎたら作り直して無限肥大を防ぐ。
					if (Cache.Size() >= 4096)
					{
						Cache.Clear();
					}
					FCachedImpulse3D Cached;
					Cached.ColliderA = Manifold.ColliderA;
					Cached.ColliderB = Manifold.ColliderB;
					Cached.BodyGenerationA = BodyA->Generation;
					Cached.BodyGenerationB = BodyB->Generation;
					Cached.FeatureId = Point.FeatureId;
					Cached.Normal = Point.Normal;
					Cached.NormalImpulse = Point.NormalImpulse;
					Cached.FrictionImpulse = Point.FrictionImpulse;
					Cache.PushBack(Cached);
				}
			}
		}
	}
	// 低速接触の継続で休止し、支持を失ったら起こす。
	void UpdateSleep_Internal(const Toolbox::TVector<FManifold3D>& Manifolds, Toolbox::f64 Slice) noexcept
	{
		if (!Sleep.bEnabled)
		{
			return;
		}
		for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
		{
			const FManifold3D& Manifold = Manifolds[ManifoldIndex];
			FBodyRecord3D* BodyA = Find_Internal(Manifold.BodyA);
			FBodyRecord3D* BodyB = Find_Internal(Manifold.BodyB);
			if (BodyA != nullptr)
			{
				BodyA->bTouched = true;
			}
			if (BodyB != nullptr)
			{
				BodyB->bTouched = true;
			}
		}
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			FBodyRecord3D& Record = Slots[Index];
			if (!Record.bAlive || Record.Type != EBodyType::Dynamic || !Record.bAllowSleep)
			{
				Record.bTouched = false;
				continue;
			}
			if (!Record.bTouched)
			{
				// 支持を失ったら起こす。
				Record.SleepTimer = 0;
				Record.bSleeping = false;
				continue;
			}
			// 速度の大きさ。
			const Toolbox::f64 Speed = Toolbox::Sqrt(Toolbox::f64(Record.Velocity.X) * Record.Velocity.X +
			                                         Toolbox::f64(Record.Velocity.Y) * Record.Velocity.Y +
			                                         Toolbox::f64(Record.Velocity.Z) * Record.Velocity.Z);
			const Toolbox::f64 Spin = Toolbox::Sqrt(Toolbox::f64(Record.AngularVelocity.X) * Record.AngularVelocity.X +
			                                        Toolbox::f64(Record.AngularVelocity.Y) * Record.AngularVelocity.Y +
			                                        Toolbox::f64(Record.AngularVelocity.Z) * Record.AngularVelocity.Z);
			if (Speed <= Sleep.LinearSpeedLimit && Spin <= Sleep.AngularSpeedLimit)
			{
				Record.SleepTimer = static_cast<Toolbox::f32>(Toolbox::f64(Record.SleepTimer) + Slice);
				if (Record.SleepTimer >= Sleep.TimeoutSeconds)
				{
					Record.bSleeping = true;
					Record.Velocity = {};
					Record.AngularVelocity = {};
				}
			}
			else
			{
				Record.SleepTimer = 0;
			}
			Record.bTouched = false;
		}
	}
	// 許容幅を超える貫通を位置で補正する。運動エネルギーは注入しない。
	void CorrectPositions_Internal(const Toolbox::TVector<FManifold3D>& Manifolds) noexcept
	{
		for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
		{
			const FManifold3D& Manifold = Manifolds[ManifoldIndex];
			FBodyRecord3D* BodyA = Find_Internal(Manifold.BodyA);
			FBodyRecord3D* BodyB = Find_Internal(Manifold.BodyB);
			if (BodyA == nullptr || BodyB == nullptr)
			{
				continue;
			}
			// 逆質量の合計。休止中は無限質量として扱う。
			const Toolbox::f64 TotalInverse =
			    Toolbox::f64(EffectiveInverseMass_Internal(*BodyA)) + EffectiveInverseMass_Internal(*BodyB);
			if (TotalInverse <= 0)
			{
				continue;
			}
			for (Toolbox::size_t PointIndex = 0; PointIndex < Manifold.Points.Size(); ++PointIndex)
			{
				const FSolvePoint3D& Point = Manifold.Points[PointIndex];
				const Toolbox::f64 Excess = -(Toolbox::f64(Point.Separation) + Contact.ContactSlop);
				if (Excess <= 0)
				{
					continue;
				}
				// 一分割の補正量に上限を設ける。
				Toolbox::f64 Correction = Toolbox::f64(Contact.BaumgarteBeta) * Excess;
				if (Correction > Contact.MaxCorrection)
				{
					Correction = Contact.MaxCorrection;
				}
			const Toolbox::f64 WeightA = Toolbox::f64(EffectiveInverseMass_Internal(*BodyA)) / TotalInverse;
			const Toolbox::f64 WeightB = Toolbox::f64(EffectiveInverseMass_Internal(*BodyB)) / TotalInverse;
				BodyA->Position += {static_cast<Toolbox::f32>(Point.Normal.X * Correction * WeightA),
				                    static_cast<Toolbox::f32>(Point.Normal.Y * Correction * WeightA),
				                    static_cast<Toolbox::f32>(Point.Normal.Z * Correction * WeightA)};
				BodyB->Position += {static_cast<Toolbox::f32>(-Point.Normal.X * Correction * WeightB),
				                    static_cast<Toolbox::f32>(-Point.Normal.Y * Correction * WeightB),
				                    static_cast<Toolbox::f32>(-Point.Normal.Z * Correction * WeightB)};
			}
		}
	}
	// 移動区間の解決を行う剛体があるかを調べる。
	bool HasContinuousBody_Internal() const noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			const FBodyRecord3D& Record = Slots[Index];
			if (Record.bAlive && Record.Type == EBodyType::Dynamic && Record.bUseContinuous)
			{
				return true;
			}
		}
		return false;
	}
	// コライダー組の線形CCD対応を調べる。箱は世界姿勢で判定する。
	EContinuousSupport ClassifyPair_Internal(const FBodyRecord3D& BodyA, const FColliderRecord3D& RecordA,
	                                         const FBodyRecord3D& BodyB, const FColliderRecord3D& RecordB) const
	{
		const Toolbox::size_t IndexA = RecordA.Shape.Index();
		const Toolbox::size_t IndexB = RecordB.Shape.Index();
		if (IndexA == 1 && IndexB == 1)
		{
			return EContinuousSupport::UnsupportedPair;
		}
		if (IndexA == 1 && !IsAxisAlignedBox_Internal(ToWorld_Internal(BodyA, RecordA.Shape.Get<1>())))
		{
			return EContinuousSupport::UnsupportedRotation;
		}
		if (IndexB == 1 && !IsAxisAlignedBox_Internal(ToWorld_Internal(BodyB, RecordB.Shape.Get<1>())))
		{
			return EContinuousSupport::UnsupportedRotation;
		}
		return EContinuousSupport::Supported;
	}
	// コライダーの移動境界を求める。
	FSweptBounds3D SweptBoundsOf_Internal(const FBodyRecord3D& Body, const FColliderRecord3D& Record,
	                                      Toolbox::FVector3 Displacement) const
	{
		if (Record.Shape.Index() == 0)
		{
			return SweptSphere_Internal(ToWorld_Internal(Body, Record.Shape.Get<0>()), Displacement);
		}
		return SweptBox_Internal(ToWorld_Internal(Body, Record.Shape.Get<1>()), Displacement);
	}
	// 全剛体を位置だけ進める。力の積分は繰り返さない。休止中は動かさない。
	void AdvanceAll_Internal(Toolbox::f64 Slice) noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			FBodyRecord3D& Record = Slots[Index];
			if (!Record.bAlive || Record.bSleeping)
			{
				continue;
			}
			if (Record.Type == EBodyType::Dynamic || Record.Type == EBodyType::Kinematic)
			{
				IntegratePosition_Internal(Record, Slice);
				const FVector3D Angular = {Record.AngularVelocity.X, Record.AngularVelocity.Y,
				                           Record.AngularVelocity.Z};
				IntegrateOrientation_Internal(Record.Orientation, Angular, Slice);
			}
		}
	}
	// 世界箱を軸平行境界へ変換する。
	static Toolbox::FAABB ToBounds_Internal(const Toolbox::FOBB& Box) noexcept
	{
		Toolbox::FAABB Bounds;
		bool bFirst = true;
		for (Toolbox::size_t Corner = 0; Corner < 8; ++Corner)
		{
			const Toolbox::f64 SignX = (Corner & 1) == 0 ? -1 : 1;
			const Toolbox::f64 SignY = (Corner & 2) == 0 ? -1 : 1;
			const Toolbox::f64 SignZ = (Corner & 4) == 0 ? -1 : 1;
			const Toolbox::f64 X = Toolbox::f64(Box.Center.X) + SignX * Box.HalfExtents.X * Box.Axes[0].X +
			                       SignY * Box.HalfExtents.Y * Box.Axes[1].X + SignZ * Box.HalfExtents.Z * Box.Axes[2].X;
			const Toolbox::f64 Y = Toolbox::f64(Box.Center.Y) + SignX * Box.HalfExtents.X * Box.Axes[0].Y +
			                       SignY * Box.HalfExtents.Y * Box.Axes[1].Y + SignZ * Box.HalfExtents.Z * Box.Axes[2].Y;
			const Toolbox::f64 Z = Toolbox::f64(Box.Center.Z) + SignX * Box.HalfExtents.X * Box.Axes[0].Z +
			                       SignY * Box.HalfExtents.Y * Box.Axes[1].Z + SignZ * Box.HalfExtents.Z * Box.Axes[2].Z;
			if (bFirst)
			{
				Bounds.Min = {static_cast<Toolbox::f32>(X), static_cast<Toolbox::f32>(Y), static_cast<Toolbox::f32>(Z)};
				Bounds.Max = Bounds.Min;
				bFirst = false;
			}
			else
			{
				if (X < Bounds.Min.X)
				{
					Bounds.Min.X = static_cast<Toolbox::f32>(X);
				}
				if (Y < Bounds.Min.Y)
				{
					Bounds.Min.Y = static_cast<Toolbox::f32>(Y);
				}
				if (Z < Bounds.Min.Z)
				{
					Bounds.Min.Z = static_cast<Toolbox::f32>(Z);
				}
				if (X > Bounds.Max.X)
				{
					Bounds.Max.X = static_cast<Toolbox::f32>(X);
				}
				if (Y > Bounds.Max.Y)
				{
					Bounds.Max.Y = static_cast<Toolbox::f32>(Y);
				}
				if (Z > Bounds.Max.Z)
				{
					Bounds.Max.Z = static_cast<Toolbox::f32>(Z);
				}
			}
		}
		return Bounds;
	}
	// 現在位置の拘束をその場で解く。時刻は進めない。
	void SolveNow_Internal()
	{
		Toolbox::TVector<FManifold3D> Manifolds;
		GenerateManifolds_Internal(Manifolds);
		for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
		{
			WarmStart_Internal(Manifolds[ManifoldIndex]);
		}
		SolveVelocities_Internal(Manifolds);
		StoreCache_Internal(Manifolds);
	}
	// 最初接触の解決まで位置を進める。残り時間は診断へ残す。
	void AdvanceContinuous_Internal(Toolbox::f64 Slice)
	{
		Toolbox::f64 Remaining = Slice;
		// 進行なし解決の繰り返しを防ぐ。
		bool bStalled = false;
		for (Toolbox::uint32 Iteration = 0; Iteration < Continuous.MaxIterations; ++Iteration)
		{
			// 最も早い接触時刻を探す。
			Toolbox::f64 BestT = 1;
			bool bFound = false;
			// 時刻ゼロで接近中の組があるか。
			bool bZeroApproach = false;
			for (Toolbox::size_t First = 0; First < Colliders.Size(); ++First)
			{
				const FColliderRecord3D& RecordA = Colliders[First];
				if (!RecordA.bAlive)
				{
					continue;
				}
				const FBodyRecord3D* BodyA = Find_Internal(RecordA.Body);
				if (BodyA == nullptr)
				{
					continue;
				}
				for (Toolbox::size_t Second = First + 1; Second < Colliders.Size(); ++Second)
				{
					const FColliderRecord3D& RecordB = Colliders[Second];
					if (!RecordB.bAlive)
					{
						continue;
					}
					const FBodyRecord3D* BodyB = Find_Internal(RecordB.Body);
					if (BodyB == nullptr)
					{
						continue;
					}
					// 同一剛体の組は自分自身へ接触しない。
					if (RecordA.Body == RecordB.Body)
					{
						continue;
					}
					if (BodyA->Type != EBodyType::Dynamic && BodyB->Type != EBodyType::Dynamic)
					{
						continue;
					}
					// 残り時間の変位。
					const bool bMoverA = BodyA->Type == EBodyType::Dynamic && BodyA->bUseContinuous;
					const bool bMoverB = BodyB->Type == EBodyType::Dynamic && BodyB->bUseContinuous;
					Toolbox::FVector3 DisplacementA;
					Toolbox::FVector3 DisplacementB;
					if (BodyA->Type != EBodyType::Static)
					{
						DisplacementA = {static_cast<Toolbox::f32>(Toolbox::f64(BodyA->Velocity.X) * Remaining),
						                 static_cast<Toolbox::f32>(Toolbox::f64(BodyA->Velocity.Y) * Remaining),
						                 static_cast<Toolbox::f32>(Toolbox::f64(BodyA->Velocity.Z) * Remaining)};
					}
					if (BodyB->Type != EBodyType::Static)
					{
						DisplacementB = {static_cast<Toolbox::f32>(Toolbox::f64(BodyB->Velocity.X) * Remaining),
						                 static_cast<Toolbox::f32>(Toolbox::f64(BodyB->Velocity.Y) * Remaining),
						                 static_cast<Toolbox::f32>(Toolbox::f64(BodyB->Velocity.Z) * Remaining)};
					}
					// CCD対象自身が止まっていても相手の移動で交差するため、要求と相対運動を分けて判定する。
					const bool bRequireCCD = bMoverA || bMoverB;
					if (!bRequireCCD)
					{
						continue;
					}
					const Toolbox::f64 RelativeX = Toolbox::f64(DisplacementA.X) - DisplacementB.X;
					const Toolbox::f64 RelativeY = Toolbox::f64(DisplacementA.Y) - DisplacementB.Y;
					const Toolbox::f64 RelativeZ = Toolbox::f64(DisplacementA.Z) - DisplacementB.Z;
					const Toolbox::f64 RelativeLength =
					    Toolbox::Sqrt(RelativeX * RelativeX + RelativeY * RelativeY + RelativeZ * RelativeZ);
					if (RelativeLength < Contact.ContactSlop)
					{
						continue;
					}
					if (!SweptOverlaps_Internal(SweptBoundsOf_Internal(*BodyA, RecordA, DisplacementA),
					                            SweptBoundsOf_Internal(*BodyB, RecordB, DisplacementB)))
					{
						continue;
					}
					if (ClassifyPair_Internal(*BodyA, RecordA, *BodyB, RecordB) != EContinuousSupport::Supported)
					{
						if (Iteration == 0)
						{
							Diagnostics.FallbackPairs++;
						}
						continue;
					}
					const Toolbox::size_t IndexA = RecordA.Shape.Index();
					const Toolbox::size_t IndexB = RecordB.Shape.Index();
					Toolbox::FSweepHit3D Hit;
					Hit.bHit = false;
					if (IndexA == 0 && IndexB == 0)
					{
						Hit = Toolbox::Sweep(ToWorld_Internal(*BodyA, RecordA.Shape.Get<0>()), DisplacementA,
						                     ToWorld_Internal(*BodyB, RecordB.Shape.Get<0>()), DisplacementB);
					}
					else if (IndexA == 0)
					{
						Hit = Toolbox::Sweep(ToWorld_Internal(*BodyA, RecordA.Shape.Get<0>()), DisplacementA,
						                     ToBounds_Internal(ToWorld_Internal(*BodyB, RecordB.Shape.Get<1>())), DisplacementB);
					}
					else
					{
						Hit = Toolbox::Sweep(ToBounds_Internal(ToWorld_Internal(*BodyA, RecordA.Shape.Get<1>())), DisplacementA,
						                     ToWorld_Internal(*BodyB, RecordB.Shape.Get<0>()), DisplacementB);
					}
					if (Hit.bHit && Hit.Time <= 0)
					{
						// 中心速度と法線で接近か離反かを区別する。
						const Toolbox::f64 Approach =
						    (Toolbox::f64(BodyA->Velocity.X) - BodyB->Velocity.X) * Hit.Normal.X +
						    (Toolbox::f64(BodyA->Velocity.Y) - BodyB->Velocity.Y) * Hit.Normal.Y +
						    (Toolbox::f64(BodyA->Velocity.Z) - BodyB->Velocity.Z) * Hit.Normal.Z;
						if (Approach < -1e-9)
						{
							bZeroApproach = true;
						}
						continue;
					}
					if (Hit.bHit && Hit.Time < BestT)
					{
						BestT = Hit.Time;
						bFound = true;
					}
				}
			}
			Diagnostics.ToiIterations++;
			if (bFound && BestT < 1 && BestT > 0)
			{
				const Toolbox::f64 Advance = BestT * Remaining;
				if (Advance < Continuous.MinAdvanceSeconds)
				{
					// 残り時間を無条件に進めず保守停止する。
					Diagnostics.UnprocessedSeconds += Remaining;
					Remaining = 0;
					break;
				}
				AdvanceAll_Internal(Advance);
				Remaining -= Advance;
				// 接触時刻の拘束を解く。
				SolveNow_Internal();
				Diagnostics.HitsResolved++;
				bStalled = false;
				continue;
			}
			if (bZeroApproach && !bStalled)
			{
				// 接近中の初期接触はその場で解いて走査し直す。
				SolveNow_Internal();
				bStalled = true;
				continue;
			}
			if (bZeroApproach)
			{
				// 進行なし解決の繰り返しは保守停止する。
				Diagnostics.UnprocessedSeconds += Remaining;
				Remaining = 0;
				break;
			}
			// 接触がなければ残りを進める。
			AdvanceAll_Internal(Remaining);
			Remaining = 0;
			break;
		}
		if (Remaining > 0)
		{
			// 反復上限で残した時間を診断へ残す。
			Diagnostics.UnprocessedSeconds += Remaining;
		}
	}
};
// Dynamicの並進速度だけを更新する。位置は呼び出し元が進める。
static void IntegrateVelocity_Internal(FBodyRecord3D& Record, Toolbox::FVector3 Gravity, Toolbox::f64 StepSeconds)
{
	// 減衰後の速度へ加速度を足す半陰的Euler。
	const Toolbox::f64 DampLinear = 1.0 / (1.0 + static_cast<Toolbox::f64>(Record.LinearDamping) * StepSeconds);
	const Toolbox::f64 DampAngular = 1.0 / (1.0 + static_cast<Toolbox::f64>(Record.AngularDamping) * StepSeconds);
	// 速度の各成分。
	Toolbox::f64 VelocityX = static_cast<Toolbox::f64>(Record.Velocity.X) * DampLinear;
	Toolbox::f64 VelocityY = static_cast<Toolbox::f64>(Record.Velocity.Y) * DampLinear;
	Toolbox::f64 VelocityZ = static_cast<Toolbox::f64>(Record.Velocity.Z) * DampLinear;
	// 重力と蓄積力による加速度。
	const Toolbox::f64 GravityX = static_cast<Toolbox::f64>(Gravity.X) * Record.GravityScale;
	const Toolbox::f64 GravityY = static_cast<Toolbox::f64>(Gravity.Y) * Record.GravityScale;
	const Toolbox::f64 GravityZ = static_cast<Toolbox::f64>(Gravity.Z) * Record.GravityScale;
	const Toolbox::f64 ForceX = static_cast<Toolbox::f64>(Record.Force.X) * Record.InverseMass;
	const Toolbox::f64 ForceY = static_cast<Toolbox::f64>(Record.Force.Y) * Record.InverseMass;
	const Toolbox::f64 ForceZ = static_cast<Toolbox::f64>(Record.Force.Z) * Record.InverseMass;
	VelocityX += (GravityX + ForceX) * StepSeconds;
	VelocityY += (GravityY + ForceY) * StepSeconds;
	VelocityZ += (GravityZ + ForceZ) * StepSeconds;
	Record.Velocity = {static_cast<Toolbox::f32>(VelocityX), static_cast<Toolbox::f32>(VelocityY),
	                   static_cast<Toolbox::f32>(VelocityZ)};
	// 角速度はジャイロ項を含めて更新する。
	FVector3D Angular = {static_cast<Toolbox::f64>(Record.AngularVelocity.X) * DampAngular,
	                     static_cast<Toolbox::f64>(Record.AngularVelocity.Y) * DampAngular,
	                     static_cast<Toolbox::f64>(Record.AngularVelocity.Z) * DampAngular};
	const FQuaternionD Q = ToDouble_Internal(Record.Orientation);
	const FVector3D Diagonal = {Record.DiagonalInertia.X, Record.DiagonalInertia.Y, Record.DiagonalInertia.Z};
	const FVector3D Momentum = TransformDiagonal_Internal(Q, Diagonal, Angular);
	const FVector3D Gyro = {Angular.Y * Momentum.Z - Angular.Z * Momentum.Y,
	                        Angular.Z * Momentum.X - Angular.X * Momentum.Z,
	                        Angular.X * Momentum.Y - Angular.Y * Momentum.X};
	const FVector3D Applied = {Record.Torque.X, Record.Torque.Y, Record.Torque.Z};
	const FVector3D Net = {Applied.X - Gyro.X, Applied.Y - Gyro.Y, Applied.Z - Gyro.Z};
	const FVector3D Inverse = {Record.InverseDiagonalInertia.X, Record.InverseDiagonalInertia.Y,
	                           Record.InverseDiagonalInertia.Z};
	const FVector3D Alpha = TransformDiagonal_Internal(Q, Inverse, Net);
	Angular.X += Alpha.X * StepSeconds;
	Angular.Y += Alpha.Y * StepSeconds;
	Angular.Z += Alpha.Z * StepSeconds;
	Record.AngularVelocity = {static_cast<Toolbox::f32>(Angular.X), static_cast<Toolbox::f32>(Angular.Y),
	                          static_cast<Toolbox::f32>(Angular.Z)};
}
// 更新後の速度で位置を進める。
static void IntegratePosition_Internal(FBodyRecord3D& Record, Toolbox::f64 StepSeconds) noexcept
{
	Record.PrevVelocity = Record.Velocity;
	Record.PrevAngularVelocity = Record.AngularVelocity;
	Record.Position += {static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Velocity.X) * StepSeconds),
	                    static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Velocity.Y) * StepSeconds),
	                    static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Velocity.Z) * StepSeconds)};
}
FPhysicsWorld3D::FPhysicsWorld3D() : m_pImpl(Toolbox::MakeUnique<FImpl>())
{
}
FPhysicsWorld3D::~FPhysicsWorld3D() = default;
FBodyId3D FPhysicsWorld3D::CreateBody(const FBodyDescription3D& Description)
{
	if (!Description.Position.IsValid() || !Description.Velocity.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body position or velocity");
	}
	if (!Description.AngularVelocity.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body angular velocity");
	}
	if (!Toolbox::IsFinite(Description.LinearDamping) || Description.LinearDamping < 0)
	{
		throw Toolbox::FException("Invalid 3D body linear damping");
	}
	if (!Toolbox::IsFinite(Description.AngularDamping) || Description.AngularDamping < 0)
	{
		throw Toolbox::FException("Invalid 3D body angular damping");
	}
	if (!Toolbox::IsFinite(Description.GravityScale))
	{
		throw Toolbox::FException("Invalid 3D body gravity scale");
	}
	// 正規化可能な姿勢だけを受け付ける。
	Toolbox::FQuaternion Orientation;
	try
	{
		Orientation = Description.Orientation.Normalized();
	}
	catch (const Toolbox::FException&)
	{
		throw Toolbox::FException("Invalid 3D body orientation");
	}
	// 新しい登録の初期状態。
	FBodyRecord3D Record;
	Record.Type = Description.Type;
	Record.Position = Description.Position;
	Record.Orientation = Orientation;
	Record.Velocity = Description.Velocity;
	Record.AngularVelocity = Description.AngularVelocity;
	Record.LinearDamping = Description.LinearDamping;
	Record.AngularDamping = Description.AngularDamping;
	Record.GravityScale = Description.GravityScale;
	Record.bUseContinuous = Description.bUseContinuous;
	Record.bAllowSleep = Description.bAllowSleep;
	if (Description.Type == EBodyType::Dynamic)
	{
		if (!Toolbox::IsFinite(Description.Mass) || Description.Mass <= 0)
		{
			throw Toolbox::FException("Invalid 3D body mass");
		}
		if (!Description.DiagonalInertia.IsValid())
		{
			throw Toolbox::FException("Invalid 3D body inertia");
		}
		if (Description.DiagonalInertia.X <= 0 || Description.DiagonalInertia.Y <= 0 ||
		    Description.DiagonalInertia.Z <= 0)
		{
			throw Toolbox::FException("Invalid 3D body inertia");
		}
		Record.InverseMass = 1.0f / Description.Mass;
		Record.DiagonalInertia = Description.DiagonalInertia;
		Record.InverseDiagonalInertia = {1.0f / Description.DiagonalInertia.X,
		                                 1.0f / Description.DiagonalInertia.Y,
		                                 1.0f / Description.DiagonalInertia.Z};
	}
	else
	{
		Record.InverseMass = 0;
		Record.InverseDiagonalInertia = {0, 0, 0};
		Record.DiagonalInertia = {0, 0, 0};
	}
	// 空きスロットの再使用または末尾への追加。
	Toolbox::size_t Index = 0;
	if (!m_pImpl->Free.IsEmpty())
	{
		Index = m_pImpl->Free.Back();
		m_pImpl->Free.PopBack();
		FBodyRecord3D& Slot = m_pImpl->Slots[Index];
		// 破棄時に進めた世代を引き継ぎ、古いIDと区別する。
		const Toolbox::uint64 NextGeneration = Slot.Generation + 1;
		Slot = Record;
		Slot.Generation = NextGeneration;
		Slot.bAlive = true;
	}
	else
	{
		Index = m_pImpl->Slots.Size();
		Record.Generation = 1;
		Record.bAlive = true;
		m_pImpl->Slots.PushBack(Record);
	}
	return {m_pImpl->World, Index, m_pImpl->Slots[Index].Generation};
}
bool FPhysicsWorld3D::DestroyBody(FBodyId3D Id) noexcept
{
	FBodyRecord3D* Record = m_pImpl->Find_Internal(Id);
	if (Record == nullptr)
	{
		return false;
	}
	Record->bAlive = false;
	Record->Generation += 1;
	Record->Force = {};
	Record->Torque = {};
	m_pImpl->Free.PushBack(Id.Index);
	// 取り付け済みのコライダーも失効させる。
	for (Toolbox::size_t Index = 0; Index < m_pImpl->Colliders.Size(); ++Index)
	{
		FColliderRecord3D& Collider = m_pImpl->Colliders[Index];
		if (Collider.bAlive && Collider.Body == Id)
		{
			Collider.bAlive = false;
			Collider.Generation += 1;
			m_pImpl->ColliderFree.PushBack(Index);
		}
	}
	// 古い接触記録を使い回さない。
	m_pImpl->Cache.Clear();
	return true;
}
bool FPhysicsWorld3D::IsAlive(FBodyId3D Id) const noexcept
{
	return m_pImpl->Find_Internal(Id) != nullptr;
}
Toolbox::FVector3 FPhysicsWorld3D::GetPosition(FBodyId3D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Position;
}
Toolbox::FQuaternion FPhysicsWorld3D::GetOrientation(FBodyId3D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Orientation;
}
Toolbox::FVector3 FPhysicsWorld3D::GetVelocity(FBodyId3D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Velocity;
}
Toolbox::FVector3 FPhysicsWorld3D::GetAngularVelocity(FBodyId3D Id) const
{
	return m_pImpl->Resolve_Internal(Id).AngularVelocity;
}
Toolbox::FVector3 FPhysicsWorld3D::GetAngularMomentum(FBodyId3D Id) const
{
	// 非Dynamicの運動量は追跡しない。
	const FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		return {};
	}
	// 対角慣性で角速度を運動量へ変換し、姿勢でワールド座標へ戻す。
	const FQuaternionD Q = ToDouble_Internal(Record.Orientation);
	const FVector3D Diagonal = {Record.DiagonalInertia.X, Record.DiagonalInertia.Y, Record.DiagonalInertia.Z};
	const FVector3D Angular = {Record.AngularVelocity.X, Record.AngularVelocity.Y, Record.AngularVelocity.Z};
	const FVector3D Momentum = TransformDiagonal_Internal(Q, Diagonal, Angular);
	return {static_cast<Toolbox::f32>(Momentum.X), static_cast<Toolbox::f32>(Momentum.Y),
	        static_cast<Toolbox::f32>(Momentum.Z)};
}
void FPhysicsWorld3D::SetVelocity(FBodyId3D Id, Toolbox::FVector3 Velocity)
{
	if (!Velocity.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body velocity");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type == EBodyType::Static)
	{
		throw Toolbox::FException("Static 3D body has no velocity");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.Velocity = Velocity;
}
void FPhysicsWorld3D::SetAngularVelocity(FBodyId3D Id, Toolbox::FVector3 AngularVelocity)
{
	if (!AngularVelocity.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body angular velocity");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type == EBodyType::Static)
	{
		throw Toolbox::FException("Static 3D body has no angular velocity");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.AngularVelocity = AngularVelocity;
}
void FPhysicsWorld3D::ApplyForce(FBodyId3D Id, Toolbox::FVector3 Force)
{
	if (!Force.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body force");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 3D bodies accept forces");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.Force += Force;
}
void FPhysicsWorld3D::ApplyTorque(FBodyId3D Id, Toolbox::FVector3 Torque)
{
	if (!Torque.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body torque");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 3D bodies accept torques");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.Torque += Torque;
}
void FPhysicsWorld3D::ApplyLinearImpulse(FBodyId3D Id, Toolbox::FVector3 Impulse)
{
	if (!Impulse.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body impulse");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 3D bodies accept impulses");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	// 力積は速度へ即時反映し、分割数に依存しない。
	Record.Velocity += Impulse * Record.InverseMass;
}
void FPhysicsWorld3D::ApplyAngularImpulse(FBodyId3D Id, Toolbox::FVector3 Impulse)
{
	if (!Impulse.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body angular impulse");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 3D bodies accept impulses");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	// 力積モーメントをワールド逆慣性で角速度へ変換する。
	const FQuaternionD Q = ToDouble_Internal(Record.Orientation);
	const FVector3D Vector = {Impulse.X, Impulse.Y, Impulse.Z};
	const FVector3D Inverse = {Record.InverseDiagonalInertia.X, Record.InverseDiagonalInertia.Y,
	                           Record.InverseDiagonalInertia.Z};
	const FVector3D Delta = TransformDiagonal_Internal(Q, Inverse, Vector);
	Record.AngularVelocity += {static_cast<Toolbox::f32>(Delta.X), static_cast<Toolbox::f32>(Delta.Y),
	                           static_cast<Toolbox::f32>(Delta.Z)};
}
void FPhysicsWorld3D::ApplyImpulseAtPoint(FBodyId3D Id, Toolbox::FVector3 Impulse, Toolbox::FVector3 WorldPoint)
{
	if (!Impulse.IsValid() || !WorldPoint.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body impulse point");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 3D bodies accept impulses");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	// 重心からの腕と力積の外積が回転を生む。
	const FVector3D Arm = {static_cast<Toolbox::f64>(WorldPoint.X) - Record.Position.X,
	                       static_cast<Toolbox::f64>(WorldPoint.Y) - Record.Position.Y,
	                       static_cast<Toolbox::f64>(WorldPoint.Z) - Record.Position.Z};
	Record.Velocity += Impulse * Record.InverseMass;
	const FVector3D Vector = {Impulse.X, Impulse.Y, Impulse.Z};
	const FVector3D Moment = {Arm.Y * Vector.Z - Arm.Z * Vector.Y, Arm.Z * Vector.X - Arm.X * Vector.Z,
	                          Arm.X * Vector.Y - Arm.Y * Vector.X};
	const FQuaternionD Q = ToDouble_Internal(Record.Orientation);
	const FVector3D Inverse = {Record.InverseDiagonalInertia.X, Record.InverseDiagonalInertia.Y,
	                           Record.InverseDiagonalInertia.Z};
	const FVector3D Delta = TransformDiagonal_Internal(Q, Inverse, Moment);
	Record.AngularVelocity += {static_cast<Toolbox::f32>(Delta.X), static_cast<Toolbox::f32>(Delta.Y),
	                           static_cast<Toolbox::f32>(Delta.Z)};
}
void FPhysicsWorld3D::SetGravity(Toolbox::FVector3 Gravity)
{
	if (!Gravity.IsValid())
	{
		throw Toolbox::FException("Invalid 3D gravity");
	}
	// 重力の変化は休止島へ影響するため起こす。同じ値は起こさない。
	if (!(m_pImpl->Gravity == Gravity))
	{
		m_pImpl->Gravity = Gravity;
		m_pImpl->WakeAll_Internal();
	}
}
Toolbox::FVector3 FPhysicsWorld3D::GetGravity() const noexcept
{
	return m_pImpl->Gravity;
}
FColliderId3D FPhysicsWorld3D::AttachCollider(FBodyId3D Body, const FColliderDescription3D& Description)
{
	FBodyRecord3D& Target = m_pImpl->Resolve_Internal(Body);
	(void)Target;
	if (Description.Shape.Index() == 0)
	{
		const Toolbox::FSphere& Local = Description.Shape.Get<0>();
		if (!Local.Center.IsValid() || !Toolbox::IsFinite(Local.Radius) || Local.Radius < 0)
		{
			throw Toolbox::FException("Invalid 3D sphere collider");
		}
	}
	else
	{
		const Toolbox::FOBB& Local = Description.Shape.Get<1>();
		if (!Local.Center.IsValid() || !Local.HalfExtents.IsValid())
		{
			throw Toolbox::FException("Invalid 3D box collider");
		}
		if (Local.HalfExtents.X < 0 || Local.HalfExtents.Y < 0 || Local.HalfExtents.Z < 0)
		{
			throw Toolbox::FException("Invalid 3D box collider");
		}
	}
	if (!Toolbox::IsFinite(Description.Friction) || Description.Friction < 0)
	{
		throw Toolbox::FException("Invalid 3D collider friction");
	}
	if (!Toolbox::IsFinite(Description.Restitution) || Description.Restitution < 0 || Description.Restitution > 1)
	{
		throw Toolbox::FException("Invalid 3D collider restitution");
	}
	// 新しい登録の初期状態。
	FColliderRecord3D Record;
	Record.Body = Body;
	Record.Shape = Description.Shape;
	Record.Friction = Description.Friction;
	Record.Restitution = Description.Restitution;
	// 空きスロットの再使用または末尾への追加。
	Toolbox::size_t Index = 0;
	if (!m_pImpl->ColliderFree.IsEmpty())
	{
		Index = m_pImpl->ColliderFree.Back();
		m_pImpl->ColliderFree.PopBack();
		FColliderRecord3D& Slot = m_pImpl->Colliders[Index];
		// 破棄時に進めた世代を引き継ぎ、古いIDと区別する。
		const Toolbox::uint64 NextGeneration = Slot.Generation + 1;
		Slot = Record;
		Slot.Generation = NextGeneration;
		Slot.bAlive = true;
	}
	else
	{
		Index = m_pImpl->Colliders.Size();
		Record.Generation = 1;
		Record.bAlive = true;
		m_pImpl->Colliders.PushBack(Record);
	}
	return {Body, Index, m_pImpl->Colliders[Index].Generation};
}
bool FPhysicsWorld3D::DetachCollider(FColliderId3D Id) noexcept
{
	FColliderRecord3D* Record = m_pImpl->FindCollider_Internal(Id);
	if (Record == nullptr)
	{
		return false;
	}
	Record->bAlive = false;
	Record->Generation += 1;
	m_pImpl->ColliderFree.PushBack(Id.Index);
	// 古い接触記録を使い回さない。
	m_pImpl->Cache.Clear();
	return true;
}
void FPhysicsWorld3D::SetContactSettings(const FContactSettings3D& Settings)
{
	if (!Toolbox::IsFinite(Settings.ContactSlop) || Settings.ContactSlop < 0)
	{
		throw Toolbox::FException("Invalid 3D contact slop");
	}
	if (!Toolbox::IsFinite(Settings.BaumgarteBeta) || Settings.BaumgarteBeta < 0 || Settings.BaumgarteBeta > 1)
	{
		throw Toolbox::FException("Invalid 3D contact beta");
	}
	if (!Toolbox::IsFinite(Settings.MaxCorrection) || Settings.MaxCorrection <= 0)
	{
		throw Toolbox::FException("Invalid 3D contact correction");
	}
	if (!Toolbox::IsFinite(Settings.RestitutionThreshold) || Settings.RestitutionThreshold < 0)
	{
		throw Toolbox::FException("Invalid 3D restitution threshold");
	}
	if (Settings.VelocityIterations < 1 || Settings.VelocityIterations > 64)
	{
		throw Toolbox::FException("Invalid 3D solver iterations");
	}
	m_pImpl->Contact = Settings;
}
FContactSettings3D FPhysicsWorld3D::GetContactSettings() const noexcept
{
	return m_pImpl->Contact;
}
void FPhysicsWorld3D::SetContinuousSettings(const FContinuousSettings3D& Settings)
{
	if (Settings.MaxIterations < 1 || Settings.MaxIterations > 32)
	{
		throw Toolbox::FException("Invalid 3D continuous iterations");
	}
	if (!Toolbox::IsFinite(Settings.MinAdvanceSeconds) || Settings.MinAdvanceSeconds < 0)
	{
		throw Toolbox::FException("Invalid 3D continuous progress");
	}
	m_pImpl->Continuous = Settings;
}
FContinuousSettings3D FPhysicsWorld3D::GetContinuousSettings() const noexcept
{
	return m_pImpl->Continuous;
}
void FPhysicsWorld3D::SetContinuous(FBodyId3D Id, bool bEnabled)
{
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	Record.bUseContinuous = bEnabled;
}
bool FPhysicsWorld3D::IsContinuous(FBodyId3D Id) const
{
	return m_pImpl->Resolve_Internal(Id).bUseContinuous;
}
EContinuousSupport FPhysicsWorld3D::QueryContinuousSupport(FColliderId3D A, FColliderId3D B) const
{
	const FColliderRecord3D* RecordA = m_pImpl->FindCollider_Internal(A);
	const FColliderRecord3D* RecordB = m_pImpl->FindCollider_Internal(B);
	if (RecordA == nullptr || RecordB == nullptr)
	{
		throw Toolbox::FException("Invalid 3D collider id");
	}
	const FBodyRecord3D* BodyA = m_pImpl->Find_Internal(RecordA->Body);
	const FBodyRecord3D* BodyB = m_pImpl->Find_Internal(RecordB->Body);
	if (BodyA == nullptr || BodyB == nullptr)
	{
		throw Toolbox::FException("Invalid 3D collider body");
	}
	return m_pImpl->ClassifyPair_Internal(*BodyA, *RecordA, *BodyB, *RecordB);
}
FContinuousDiagnostics3D FPhysicsWorld3D::GetContinuousDiagnostics() const noexcept
{
	return m_pImpl->Diagnostics;
}
void FPhysicsWorld3D::SetSleepSettings(const FSleepSettings3D& Settings)
{
	if (!Toolbox::IsFinite(Settings.TimeoutSeconds) || Settings.TimeoutSeconds <= 0)
	{
		throw Toolbox::FException("Invalid 3D sleep timeout");
	}
	if (!Toolbox::IsFinite(Settings.LinearSpeedLimit) || Settings.LinearSpeedLimit < 0)
	{
		throw Toolbox::FException("Invalid 3D sleep speed");
	}
	if (!Toolbox::IsFinite(Settings.AngularSpeedLimit) || Settings.AngularSpeedLimit < 0)
	{
		throw Toolbox::FException("Invalid 3D sleep spin");
	}
	// 無効化で凍結した剛体を残さない。
	const bool bWasEnabled = m_pImpl->Sleep.bEnabled;
	m_pImpl->Sleep = Settings;
	if (bWasEnabled && !Settings.bEnabled)
	{
		m_pImpl->WakeAll_Internal();
	}
}
FSleepSettings3D FPhysicsWorld3D::GetSleepSettings() const noexcept
{
	return m_pImpl->Sleep;
}
bool FPhysicsWorld3D::IsSleeping(FBodyId3D Id) const
{
	return m_pImpl->Resolve_Internal(Id).bSleeping;
}
bool FPhysicsWorld3D::WakeUp(FBodyId3D Id) noexcept
{
	FBodyRecord3D* Record = m_pImpl->Find_Internal(Id);
	if (Record == nullptr || Record->Type != EBodyType::Dynamic)
	{
		return false;
	}
	Record->bSleeping = false;
	Record->SleepTimer = 0;
	return true;
}
void FPhysicsWorld3D::SetBodyTransform(FBodyId3D Id, Toolbox::FVector3 Position, Toolbox::FQuaternion Orientation)
{
	if (!Position.IsValid())
	{
		throw Toolbox::FException("Invalid 3D body transform");
	}
	Toolbox::FQuaternion Normalized;
	try
	{
		Normalized = Orientation.Normalized();
	}
	catch (const Toolbox::FException&)
	{
		throw Toolbox::FException("Invalid 3D body transform");
	}
	FBodyRecord3D& Record = m_pImpl->Resolve_Internal(Id);
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.Position = Position;
	Record.Orientation = Normalized;
}
void FPhysicsWorld3D::ClearContactCache() noexcept
{
	m_pImpl->Cache.Clear();
}
bool FPhysicsWorld3D::IsColliderAlive(FColliderId3D Id) const noexcept
{
	return m_pImpl->FindCollider_Internal(Id) != nullptr;
}
void FPhysicsWorld3D::Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps)
{
	if (!Toolbox::IsFinite(DeltaSeconds) || DeltaSeconds <= 0)
	{
		throw Toolbox::FException("Invalid 3D step seconds");
	}
	if (SubSteps < 1 || SubSteps > 1024)
	{
		throw Toolbox::FException("Invalid 3D sub step count");
	}
	// 一回の更新を等分割し、蓄積力は全分割で保持する。
	const Toolbox::f64 Slice = DeltaSeconds / static_cast<Toolbox::f64>(SubSteps);
	// 診断は更新ごとに作り直す。
	m_pImpl->Diagnostics = {};
	// 移動区間の解決を行うか。
	const bool bContinuous = m_pImpl->Continuous.bEnabled && m_pImpl->HasContinuousBody_Internal();
	for (Toolbox::uint32 SliceIndex = 0; SliceIndex < SubSteps; ++SliceIndex)
	{
		// 力と重力を速度へ反映する。
		for (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)
		{
			FBodyRecord3D& Record = m_pImpl->Slots[Index];
			if (!Record.bAlive || Record.Type != EBodyType::Dynamic || Record.bSleeping)
			{
				continue;
			}
			IntegrateVelocity_Internal(Record, m_pImpl->Gravity, Slice);
		}
		// 現在位置の接触を集めて速度拘束を解く。
		Toolbox::TVector<FManifold3D> Manifolds;
		m_pImpl->GenerateManifolds_Internal(Manifolds);
		for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
		{
			m_pImpl->WarmStart_Internal(Manifolds[ManifoldIndex]);
		}
		m_pImpl->SolveVelocities_Internal(Manifolds);
		m_pImpl->StoreCache_Internal(Manifolds);
		if (bContinuous)
		{
			// 最初接触まで進めて残りを解決する。
			m_pImpl->AdvanceContinuous_Internal(Slice);
			// 移動後の分離で貫通を補正する。
			Toolbox::TVector<FManifold3D> Touched;
			m_pImpl->GenerateManifolds_Internal(Touched);
			m_pImpl->CorrectPositions_Internal(Touched);
			m_pImpl->UpdateSleep_Internal(Touched, Slice);
			continue;
		}
		// 更新後の速度で位置を進める。
		for (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)
		{
			FBodyRecord3D& Record = m_pImpl->Slots[Index];
			if (!Record.bAlive)
			{
				continue;
			}
			if (Record.Type == EBodyType::Dynamic)
			{
				IntegratePosition_Internal(Record, Slice);
				// 指定角速度で姿勢を進める。
				const FVector3D Angular = {Record.AngularVelocity.X, Record.AngularVelocity.Y,
				                           Record.AngularVelocity.Z};
				IntegrateOrientation_Internal(Record.Orientation, Angular, Slice);
			}
			else if (Record.Type == EBodyType::Kinematic)
			{
				IntegratePosition_Internal(Record, Slice);
				// 指定角速度で姿勢を進める。
				const FVector3D Angular = {Record.AngularVelocity.X, Record.AngularVelocity.Y,
				                           Record.AngularVelocity.Z};
				IntegrateOrientation_Internal(Record.Orientation, Angular, Slice);
			}
		}
		// 許容幅を超える貫通を位置で補正する。
		m_pImpl->CorrectPositions_Internal(Manifolds);
		m_pImpl->UpdateSleep_Internal(Manifolds, Slice);
	}
	// 蓄積した力とトルクを一度だけ消去する。
	for (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)
	{
		FBodyRecord3D& Record = m_pImpl->Slots[Index];
		Record.Force = {};
		Record.Torque = {};
	}
}
} // namespace Dxf
