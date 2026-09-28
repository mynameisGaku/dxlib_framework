// SPDX-License-Identifier: NOASSERTION
#include "Dxf/RigidBody2D.h"
#include "PhysicsSnapshotBuilder.h"
#include "ParallelPhysicsCore.h"
#include "QueryCandidates.h"
#include "QueryResultOrder.h"
#include "SolverPairs.h"
#include "WorldQueryShapes2D.h"
#include "WorldEventTracker.h"
#include "WorldEventCandidates.h"
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
#include "WorldInteractionProbe.h"
#endif
#include "Toolbox/CapsuleContact2D.h"
#include "Toolbox/CapsuleQuery2D.h"
#include "Toolbox/ContinuousCollision.h"
#include "Toolbox/SegmentIntersection2D.h"
#include "Toolbox/ShapeSweep2D.h"
#include "Toolbox/ShapeContactQuery2D.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
// 一つの平面剛体が持つ運動状態と蓄積した外力。
struct FBodyRecord2D
{
	// スロット再使用を見分ける世代。
	Toolbox::uint64 Generation = 0;
	// 有効な登録か。
	bool bAlive = false;
	// 運動区分。
	EBodyType Type = EBodyType::Dynamic;
	// 重心位置。メートル単位でY軸が上向き。
	Toolbox::FVector2 Position;
	// 姿勢角。ラジアン単位で反時計回りが正。
	Toolbox::f32 Angle = 0;
	// 重心速度。メートル毎秒単位。
	Toolbox::FVector2 Velocity;
	// 角速度。ラジアン毎秒単位で反時計回りが正。
	Toolbox::f32 AngularVelocity = 0;
	// 直前の位置更新が終わった時点の速度。起床判定の相対運動に使う。
	Toolbox::FVector2 PrevVelocity;
	// 直前の位置更新が終わった時点の角速度。起床判定の相対運動に使う。
	Toolbox::f32 PrevAngularVelocity = 0;
	// 質量の逆数。StaticとKinematicはゼロ。
	Toolbox::f32 InverseMass = 1;
	// 重心回り慣性の逆数。StaticとKinematicはゼロ。
	Toolbox::f32 InverseInertia = 1;
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
	// 今回分割で、休止判定で支持となるconstraint（ContactまたはJoint）へ
	// 参加しているか。SensorはSolver拘束ではないので参加を印さない。
	bool bTouched = false;
	// 次の更新で使う蓄積力。ニュートン単位。
	Toolbox::FVector2 Force;
	// 次の更新で使う蓄積トルク。ニュートンメートル単位。
	Toolbox::f32 Torque = 0;
};
// 更新後の速度で位置と姿勢を進める。
static void IntegratePosition_Internal(FBodyRecord2D& Record, Toolbox::f64 StepSeconds) noexcept;
// 指定速度どおりに運動させる。外力と減衰は適用しない。
static void IntegrateKinematic_Internal(FBodyRecord2D& Record, Toolbox::f64 StepSeconds) noexcept;
// 剛体へ取り付けた形状と材質の登録。
struct FColliderRecord2D
{
	// スロット再使用を見分ける世代。
	Toolbox::uint64 Generation = 0;
	// 有効な登録か。
	bool bAlive = false;
	// 取り付け先の剛体。
	FBodyId2D Body;
	// 重心相対の形状。
	decltype(FColliderDescription2D::Shape) Shape;
	// 摩擦係数。
	Toolbox::f32 Friction = 0.5f;
	// 反発係数。
	Toolbox::f32 Restitution = 0;
	// 線分問い合わせ用のカテゴリ。0は問い合わせ対象外。接触には使わない。
	Toolbox::uint32 QueryCategory = 1u;
	// Solid／Sensorの区分。Sensorを含む組は物理応答をしない。
	EColliderResponse Response = EColliderResponse::Solid;
	// 接触・Triggerの組を調べるかを決める衝突カテゴリとマスク。
	FColliderCollisionFilter Collision;
};
// 速度拘束の反復で使う単一接触点。
struct FSolvePoint2D
{
	// 接触位置。メートル単位。
	Toolbox::FVector2 Position;
	// 二つ目から一つ目へ向く単位法線。
	Toolbox::FVector2 Normal{1, 0};
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
	// 蓄積した接線Impulse。
	Toolbox::f32 TangentImpulse = 0;
	// 反復前の法線相対速度。反発目標の保存値。
	Toolbox::f64 ApproachSpeed = 0;
};
// 正準順序のコライダー組と接触点列。
struct FManifold2D
{
	// 正準順序の一つ目のコライダー。
	FColliderId2D ColliderA;
	// 正準順序の二つ目のコライダー。
	FColliderId2D ColliderB;
	// 一つ目の剛体。
	FBodyId2D BodyA;
	// 二つ目の剛体。
	FBodyId2D BodyB;
	// 解決する接触点列。
	Toolbox::TVector<FSolvePoint2D> Points;
};
// 前回Impulseの再利用記録。
struct FCachedImpulse2D
{
	// 正準順序の一つ目のコライダー。
	FColliderId2D ColliderA;
	// 正準順序の二つ目のコライダー。
	FColliderId2D ColliderB;
	// 一つ目の剛体の世代。
	Toolbox::uint64 BodyGenerationA = 0;
	// 二つ目の剛体の世代。
	Toolbox::uint64 BodyGenerationB = 0;
	// 接触点の特徴ID。
	Toolbox::uint32 FeatureId = 0;
	// 保存時の法線。
	Toolbox::FVector2 Normal{1, 0};
	// 保存した法線Impulse。
	Toolbox::f32 NormalImpulse = 0;
	// 保存した接線Impulse。
	Toolbox::f32 TangentImpulse = 0;
};
// 箱の世界軸を求める。UがローカルX軸、VがローカルY軸。
static void BoxAxes_Internal(const Toolbox::FOrientedBox2D& Box, Toolbox::FVector2& U, Toolbox::FVector2& V) noexcept
{
	// 回転角の余弦と正弦。
	const Toolbox::f64 Cosine = Toolbox::Cos(Toolbox::f64(Box.Angle));
	const Toolbox::f64 Sine = Toolbox::Sin(Toolbox::f64(Box.Angle));
	U = {static_cast<Toolbox::f32>(Cosine), static_cast<Toolbox::f32>(Sine)};
	V = {static_cast<Toolbox::f32>(-Sine), static_cast<Toolbox::f32>(Cosine)};
}
// 箱の頂点を求める。符号で四隅を区別する。
static Toolbox::FVector2 BoxVertex_Internal(const Toolbox::FOrientedBox2D& Box, Toolbox::FVector2 U,
                                            Toolbox::FVector2 V, Toolbox::f64 SignX, Toolbox::f64 SignY) noexcept
{
	const Toolbox::f64 X = Toolbox::f64(Box.Center.X) + SignX * Box.HalfExtents.X * U.X + SignY * Box.HalfExtents.Y * V.X;
	const Toolbox::f64 Y = Toolbox::f64(Box.Center.Y) + SignX * Box.HalfExtents.X * U.Y + SignY * Box.HalfExtents.Y * V.Y;
	return {static_cast<Toolbox::f32>(X), static_cast<Toolbox::f32>(Y)};
}
// 箱同士の接触を最大二点求める。分離時は空。法線はB→A。
static void FindBoxContacts_Internal(const Toolbox::FOrientedBox2D& A, const Toolbox::FOrientedBox2D& B,
                                     Toolbox::f32 Slop, Toolbox::TVector<Toolbox::FContactPoint2D>& Out)
{
	Toolbox::FVector2 AxesA[2];
	Toolbox::FVector2 AxesB[2];
	BoxAxes_Internal(A, AxesA[0], AxesA[1]);
	BoxAxes_Internal(B, AxesB[0], AxesB[1]);
	// 中心差。
	const Toolbox::f64 DeltaX = Toolbox::f64(B.Center.X) - A.Center.X;
	const Toolbox::f64 DeltaY = Toolbox::f64(B.Center.Y) - A.Center.Y;
	// 最大分離とその軸の所有箱。
	Toolbox::f64 BestSeparation = -1e30;
	Toolbox::f64 BestX = 1;
	Toolbox::f64 BestY = 0;
	bool bBestOnA = true;
	Toolbox::int32 BestFace = 1;
	for (Toolbox::int32 Owner = 0; Owner < 2; ++Owner)
	{
		Toolbox::FVector2* Axes = Owner == 0 ? AxesA : AxesB;
		for (Toolbox::int32 Axis = 0; Axis < 2; ++Axis)
		{
			// 軸方向の中心距離。
			const Toolbox::f64 Distance = DeltaX * Axes[Axis].X + DeltaY * Axes[Axis].Y;
			// 両箱の軸への投影半径。
			const Toolbox::f64 ProjectA = Toolbox::f64(A.HalfExtents.X) * Toolbox::Abs(Toolbox::f64(AxesA[0].X) * Axes[Axis].X + Toolbox::f64(AxesA[0].Y) * Axes[Axis].Y) +
			                              Toolbox::f64(A.HalfExtents.Y) * Toolbox::Abs(Toolbox::f64(AxesA[1].X) * Axes[Axis].X + Toolbox::f64(AxesA[1].Y) * Axes[Axis].Y);
			const Toolbox::f64 ProjectB = Toolbox::f64(B.HalfExtents.X) * Toolbox::Abs(Toolbox::f64(AxesB[0].X) * Axes[Axis].X + Toolbox::f64(AxesB[0].Y) * Axes[Axis].Y) +
			                              Toolbox::f64(B.HalfExtents.Y) * Toolbox::Abs(Toolbox::f64(AxesB[1].X) * Axes[Axis].X + Toolbox::f64(AxesB[1].Y) * Axes[Axis].Y);
			const Toolbox::f64 Separation = Toolbox::Abs(Distance) - ProjectA - ProjectB;
			if (Separation > BestSeparation)
			{
				BestSeparation = Separation;
				// AからBへ向く軸方向。
				const Toolbox::f64 Sign = Distance >= 0 ? 1 : -1;
				BestX = Axes[Axis].X * Sign;
				BestY = Axes[Axis].Y * Sign;
				bBestOnA = Owner == 0;
				BestFace = Axis * 2 + (Sign > 0 ? 1 : 0);
			}
		}
	}
	if (BestSeparation > Slop)
	{
		return;
	}
	// 基準面は相手箱へ向く側の面を使う。所有箱の最良面が遠い側の場合は近傍面へ読み替える。
	Toolbox::int32 ReferenceFace = BestFace;
	if (!bBestOnA)
	{
		ReferenceFace = BestFace ^ 1;
	}
	const Toolbox::FOrientedBox2D& Reference = bBestOnA ? A : B;
	const Toolbox::FOrientedBox2D& Incident = bBestOnA ? B : A;
	// 基準面の法線はAからBへ向く。
	Toolbox::FVector2 FaceNormal = {static_cast<Toolbox::f32>(BestX), static_cast<Toolbox::f32>(BestY)};
	if (!bBestOnA)
	{
		FaceNormal = {-FaceNormal.X, -FaceNormal.Y};
	}
	// 基準面の接線と接線方向の半辺長。
	Toolbox::FVector2 Tangent = {-FaceNormal.Y, FaceNormal.X};
	Toolbox::FVector2 ReferenceU;
	Toolbox::FVector2 ReferenceV;
	BoxAxes_Internal(Reference, ReferenceU, ReferenceV);
	const Toolbox::int32 FaceAxis = ReferenceFace / 2;
	const Toolbox::f32 ReferenceHalf =
	    FaceAxis == 0 ? Reference.HalfExtents.Y : Reference.HalfExtents.X;
	// 基準面の中心。
	Toolbox::FVector2 FaceCenter = Reference.Center;
	if (ReferenceFace == 1)
	{
		FaceCenter += {ReferenceU.X * Reference.HalfExtents.X, ReferenceU.Y * Reference.HalfExtents.X};
	}
	else if (ReferenceFace == 0)
	{
		FaceCenter += {-ReferenceU.X * Reference.HalfExtents.X, -ReferenceU.Y * Reference.HalfExtents.X};
	}
	else if (ReferenceFace == 3)
	{
		FaceCenter += {ReferenceV.X * Reference.HalfExtents.Y, ReferenceV.Y * Reference.HalfExtents.Y};
	}
	else
	{
		FaceCenter += {-ReferenceV.X * Reference.HalfExtents.Y, -ReferenceV.Y * Reference.HalfExtents.Y};
	}
	// 入射辺は基準法線に最も逆らう面の両端。
	Toolbox::FVector2 IncidentU;
	Toolbox::FVector2 IncidentV;
	BoxAxes_Internal(Incident, IncidentU, IncidentV);
	Toolbox::f64 BestDot = 1e30;
	Toolbox::int32 IncidentFace = 0;
	const Toolbox::FVector2 Normals[4] = {{-IncidentU.X, -IncidentU.Y},
	                                      {IncidentU.X, IncidentU.Y},
	                                      {-IncidentV.X, -IncidentV.Y},
	                                      {IncidentV.X, IncidentV.Y}};
	for (Toolbox::int32 Face = 0; Face < 4; ++Face)
	{
		const Toolbox::f64 Alignment = Toolbox::f64(Normals[Face].X) * FaceNormal.X + Toolbox::f64(Normals[Face].Y) * FaceNormal.Y;
		if (Alignment < BestDot)
		{
			BestDot = Alignment;
			IncidentFace = Face;
		}
	}
	// 入射辺の両端点。
	Toolbox::FVector2 Edge[2];
	if (IncidentFace == 0 || IncidentFace == 1)
	{
		const Toolbox::f64 SignX = IncidentFace == 1 ? 1 : -1;
		Edge[0] = BoxVertex_Internal(Incident, IncidentU, IncidentV, SignX, -1);
		Edge[1] = BoxVertex_Internal(Incident, IncidentU, IncidentV, SignX, 1);
	}
	else
	{
		const Toolbox::f64 SignY = IncidentFace == 3 ? 1 : -1;
		Edge[0] = BoxVertex_Internal(Incident, IncidentU, IncidentV, -1, SignY);
		Edge[1] = BoxVertex_Internal(Incident, IncidentU, IncidentV, 1, SignY);
	}
	// 基準面の側方平面で切り取る。
	Toolbox::FVector2 Clipped[2] = {Edge[0], Edge[1]};
	Toolbox::int32 ClippedCount = 2;
	for (Toolbox::int32 Side = 0; Side < 2; ++Side)
	{
		const Toolbox::f64 Bound = Side == 0 ? -Toolbox::f64(ReferenceHalf) : Toolbox::f64(ReferenceHalf);
		Toolbox::FVector2 Kept[2];
		Toolbox::int32 KeptCount = 0;
		for (Toolbox::int32 Index = 0; Index < ClippedCount; ++Index)
		{
			const Toolbox::f64 Lateral =
			    (Toolbox::f64(Clipped[Index].X) - FaceCenter.X) * Tangent.X + (Toolbox::f64(Clipped[Index].Y) - FaceCenter.Y) * Tangent.Y;
			const bool bInside = Side == 0 ? Lateral >= Bound : Lateral <= Bound;
			if (bInside)
			{
				Kept[KeptCount] = Clipped[Index];
				++KeptCount;
			}
		}
		// 平面をまたぐ辺は交点を残す。
		for (Toolbox::int32 Index = 0; Index < ClippedCount; ++Index)
		{
			const Toolbox::int32 Next = (Index + 1) % ClippedCount;
			const Toolbox::f64 LateralA =
			    (Toolbox::f64(Clipped[Index].X) - FaceCenter.X) * Tangent.X + (Toolbox::f64(Clipped[Index].Y) - FaceCenter.Y) * Tangent.Y;
			const Toolbox::f64 LateralB =
			    (Toolbox::f64(Clipped[Next].X) - FaceCenter.X) * Tangent.X + (Toolbox::f64(Clipped[Next].Y) - FaceCenter.Y) * Tangent.Y;
			const bool bInsideA = Side == 0 ? LateralA >= Bound : LateralA <= Bound;
			const bool bInsideB = Side == 0 ? LateralB >= Bound : LateralB <= Bound;
			if (bInsideA != bInsideB && KeptCount < 2)
			{
				const Toolbox::f64 Fraction = (Bound - LateralA) / (LateralB - LateralA);
				Kept[KeptCount] = {static_cast<Toolbox::f32>(Clipped[Index].X + (Clipped[Next].X - Clipped[Index].X) * Fraction),
				                   static_cast<Toolbox::f32>(Clipped[Index].Y + (Clipped[Next].Y - Clipped[Index].Y) * Fraction)};
				++KeptCount;
			}
		}
		ClippedCount = KeptCount;
		if (ClippedCount == 0)
		{
			return;
		}
		Clipped[0] = Kept[0];
		if (KeptCount > 1)
		{
			Clipped[1] = Kept[1];
		}
	}
	// 法線はBからAへ向ける。最良軸はA→Bで作るため反転して保つ。
	Toolbox::FVector2 Normal = {static_cast<Toolbox::f32>(-BestX), static_cast<Toolbox::f32>(-BestY)};
	// 特徴IDは基準面側の面番号に切り取り順を足す。
	const Toolbox::uint32 Base =
	    static_cast<Toolbox::uint32>((bBestOnA ? ReferenceFace : 8 + ReferenceFace) * 2);
	for (Toolbox::int32 Index = 0; Index < ClippedCount && Index < 2; ++Index)
	{
		// A面からB表面への符号付き距離。B→A法線では貫通が負になる。
		// 基準面がB側の場合は入射側がAになるため差の向きを入れ替える。
		const Toolbox::f64 GapX = bBestOnA ? Toolbox::f64(FaceCenter.X) - Clipped[Index].X
		                                   : Toolbox::f64(Clipped[Index].X) - FaceCenter.X;
		const Toolbox::f64 GapY = bBestOnA ? Toolbox::f64(FaceCenter.Y) - Clipped[Index].Y
		                                   : Toolbox::f64(Clipped[Index].Y) - FaceCenter.Y;
		const Toolbox::f64 Separation = GapX * Normal.X + GapY * Normal.Y;
		if (Separation > Slop)
		{
			continue;
		}
		Toolbox::FContactPoint2D Hit;
		Hit.Position = Clipped[Index];
		Hit.Normal = Normal;
		Hit.Separation = static_cast<Toolbox::f32>(Separation);
		Hit.FeatureId = Base + static_cast<Toolbox::uint32>(Index);
		Out.PushBack(Hit);
	}
}
// 移動区間を覆う軸平行境界。
struct FSweptBounds2D
{
	// 二軸の最小座標。
	Toolbox::FVector2 Min;
	// 二軸の最大座標。
	Toolbox::FVector2 Max;
};
// 円の移動区間を覆う境界を求める。
static FSweptBounds2D SweptCircle_Internal(Toolbox::FCircle2D Circle, Toolbox::FVector2 Displacement) noexcept
{
	FSweptBounds2D Bounds;
	// 始点と終点の両端に半径を足す。
	const Toolbox::f64 StartX = Circle.Center.X;
	const Toolbox::f64 StartY = Circle.Center.Y;
	const Toolbox::f64 EndX = StartX + Displacement.X;
	const Toolbox::f64 EndY = StartY + Displacement.Y;
	Bounds.Min = {static_cast<Toolbox::f32>((StartX < EndX ? StartX : EndX) - Circle.Radius),
	              static_cast<Toolbox::f32>((StartY < EndY ? StartY : EndY) - Circle.Radius)};
	Bounds.Max = {static_cast<Toolbox::f32>((StartX > EndX ? StartX : EndX) + Circle.Radius),
	              static_cast<Toolbox::f32>((StartY > EndY ? StartY : EndY) + Circle.Radius)};
	return Bounds;
}
// 矩形の移動区間を覆う境界を求める。
static FSweptBounds2D SweptBox_Internal(Toolbox::FOrientedBox2D Box, Toolbox::FVector2 Displacement) noexcept
{
	// 回転角の余弦と正弦。
	const Toolbox::f64 Cosine = Toolbox::Cos(Toolbox::f64(Box.Angle));
	const Toolbox::f64 Sine = Toolbox::Sin(Toolbox::f64(Box.Angle));
	FSweptBounds2D Bounds;
	// 四隅の始点と終点で範囲を広げる。
	bool bFirst = true;
	for (Toolbox::int32 Corner = 0; Corner < 4; ++Corner)
	{
		const Toolbox::f64 SignX = (Corner & 1) == 0 ? -1 : 1;
		const Toolbox::f64 SignY = (Corner & 2) == 0 ? -1 : 1;
		const Toolbox::f64 LocalX = SignX * Box.HalfExtents.X;
		const Toolbox::f64 LocalY = SignY * Box.HalfExtents.Y;
		const Toolbox::f64 StartX = Toolbox::f64(Box.Center.X) + Cosine * LocalX - Sine * LocalY;
		const Toolbox::f64 StartY = Toolbox::f64(Box.Center.Y) + Sine * LocalX + Cosine * LocalY;
		const Toolbox::f64 PointsX[2] = {StartX, StartX + Displacement.X};
		const Toolbox::f64 PointsY[2] = {StartY, StartY + Displacement.Y};
		for (Toolbox::int32 Point = 0; Point < 2; ++Point)
		{
			if (bFirst)
			{
				Bounds.Min = {static_cast<Toolbox::f32>(PointsX[Point]), static_cast<Toolbox::f32>(PointsY[Point])};
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
				if (PointsX[Point] > Bounds.Max.X)
				{
					Bounds.Max.X = static_cast<Toolbox::f32>(PointsX[Point]);
				}
				if (PointsY[Point] > Bounds.Max.Y)
				{
					Bounds.Max.Y = static_cast<Toolbox::f32>(PointsY[Point]);
				}
			}
		}
	}
	return Bounds;
}
// カプセルの移動区間を覆う境界を求める。
static FSweptBounds2D SweptCapsule_Internal(const Toolbox::FCapsule2D& Capsule, Toolbox::FVector2 Displacement) noexcept
{
	const Toolbox::FAABB2D Start = Toolbox::CapsuleBounds(Capsule);
	const Toolbox::f64 Move[2] = {Displacement.X, Displacement.Y};
	const Toolbox::f64 Low[2] = {Start.Min.X, Start.Min.Y};
	const Toolbox::f64 High[2] = {Start.Max.X, Start.Max.Y};
	Toolbox::f32 Min[2]{};
	Toolbox::f32 Max[2]{};
	for (Toolbox::int32 Axis = 0; Axis < 2; ++Axis)
	{
		Min[Axis] = static_cast<Toolbox::f32>(Move[Axis] < 0 ? Low[Axis] + Move[Axis] : Low[Axis]);
		Max[Axis] = static_cast<Toolbox::f32>(Move[Axis] > 0 ? High[Axis] + Move[Axis] : High[Axis]);
	}
	FSweptBounds2D Bounds;
	Bounds.Min = {Min[0], Min[1]};
	Bounds.Max = {Max[0], Max[1]};
	return Bounds;
}
// カプセルを含まない組（呼ばれない）。
template <typename TA, typename TB>
static Toolbox::uint32 CapsuleContacts_Internal(const TA&, const TB&, Toolbox::f32,
                                                Toolbox::FContactPoint2D (&)[Toolbox::MaxCapsuleContacts2D]) noexcept
{
	return 0;
}
// 一点の接触をMargin以下なら加える。
static Toolbox::uint32 SingleContact_Internal(const Toolbox::FContactPoint2D& Hit, Toolbox::f32 Margin,
                                              Toolbox::FContactPoint2D (&Out)[Toolbox::MaxCapsuleContacts2D]) noexcept
{
	if (!(Hit.Separation <= Margin))
	{
		return 0;
	}
	Out[0] = Hit;
	return 1;
}
// 円とカプセル（一点）。法線はB→A。
static Toolbox::uint32 CapsuleContacts_Internal(const Toolbox::FCircle2D& A, const Toolbox::FCapsule2D& B,
                                                Toolbox::f32 Margin,
                                                Toolbox::FContactPoint2D (&Out)[Toolbox::MaxCapsuleContacts2D])
{
	return SingleContact_Internal(Toolbox::FindContact(A, B), Margin, Out);
}
// カプセルと円（一点）。法線はB→A。
static Toolbox::uint32 CapsuleContacts_Internal(const Toolbox::FCapsule2D& A, const Toolbox::FCircle2D& B,
                                                Toolbox::f32 Margin,
                                                Toolbox::FContactPoint2D (&Out)[Toolbox::MaxCapsuleContacts2D])
{
	return SingleContact_Internal(Toolbox::FindContact(A, B), Margin, Out);
}
// カプセルと矩形（最も近い点と中心線の両端、最大三点）。法線はB→A。
static Toolbox::uint32 CapsuleContacts_Internal(const Toolbox::FCapsule2D& A, const Toolbox::FOrientedBox2D& B,
                                                Toolbox::f32 Margin,
                                                Toolbox::FContactPoint2D (&Out)[Toolbox::MaxCapsuleContacts2D])
{
	return Toolbox::FindCapsuleContacts(A, B, Margin, Out);
}
// 矩形とカプセル。カプセルと矩形の接触の法線を反転する（B→A）。
static Toolbox::uint32 CapsuleContacts_Internal(const Toolbox::FOrientedBox2D& A, const Toolbox::FCapsule2D& B,
                                                Toolbox::f32 Margin,
                                                Toolbox::FContactPoint2D (&Out)[Toolbox::MaxCapsuleContacts2D])
{
	const Toolbox::uint32 Count = Toolbox::FindCapsuleContacts(B, A, Margin, Out);
	for (Toolbox::uint32 Index = 0; Index < Count; ++Index)
	{
		Out[Index].Normal = -Out[Index].Normal;
	}
	return Count;
}
// カプセル同士（最大三点）。法線はB→A。
static Toolbox::uint32 CapsuleContacts_Internal(const Toolbox::FCapsule2D& A, const Toolbox::FCapsule2D& B,
                                                Toolbox::f32 Margin,
                                                Toolbox::FContactPoint2D (&Out)[Toolbox::MaxCapsuleContacts2D])
{
	return Toolbox::FindCapsuleContacts(A, B, Margin, Out);
}
// 二つの移動境界が重なるかを調べる。
static bool SweptOverlaps_Internal(const FSweptBounds2D& A, const FSweptBounds2D& B) noexcept
{
	return A.Min.X <= B.Max.X && B.Min.X <= A.Max.X && A.Min.Y <= B.Max.Y && B.Min.Y <= A.Max.Y;
}
// 世界箱が軸平行かを調べる。箱の対称性から周期は180度。
static bool IsAxisAligned_Internal(Toolbox::f32 Angle) noexcept
{
	return Toolbox::Abs(Toolbox::Sin(2.0 * Toolbox::f64(Angle))) < 1e-6;
}
// 平面剛体の登録スロットと接触解決をまとめた実装。
// 問い合わせ索引へ最後に反映したBodyの姿勢。Stepの完了時に、姿勢が変わったBodyのColliderだけを合わせ直す。
struct FIndexedPose2D
{
	// 記録があるか。
	// 索引へ最後に反映した重心位置。
	Toolbox::FVector2 Position;
	// 索引へ最後に反映した角度。
	Toolbox::f32 Angle = 0;
	bool bValid = false;
};
// 距離拘束の登録スロット。Colliderとは独立で、Body同士の拘束だけを保持する。
struct FJointRecord2D
{
	// スロットを破棄して再使用するたびに増える世代。
	Toolbox::uint64 Generation = 0;
	// 登録中か。
	bool bAlive = false;
	// 拘束する二つのBodyの完全なID。
	FBodyId2D BodyA;
	FBodyId2D BodyB;
	// BodyA側のLocal Anchor（重心基準）。
	Toolbox::FVector2 LocalAnchorA;
	// BodyB側のLocal Anchor（重心基準）。
	Toolbox::FVector2 LocalAnchorB;
	// 維持するAnchor間距離。
	Toolbox::f64 Length = 0;
	// Warm Start用に保持する、前回Stepの確定Impulse（NewtonsSecond相当）。
	// Joint Recordの寿命内だけ有効であり、DestroyJointとBody破棄で消失する。
	Toolbox::f64 AccumulatedImpulse = 0;
	// 軸が縮退したStepで用いる保存軸。A→B規約。
	Toolbox::FVector2 LastValidAxis{1, 0};
	// LastValidAxisを実際に更新済みか。
	bool bHasLastValidAxis = false;
};
struct FPhysicsWorld2D::FImpl
{
	// 別ワールドのID混入を検出する識別子。
	Toolbox::uint64 World = NextWorld_Internal();
	// スロット番号で直接参照する登録領域。
	Toolbox::TVector<FBodyRecord2D> Slots;
	// 再使用可能な空きスロット番号。
	Toolbox::TVector<Toolbox::size_t> Free;
	// スロット番号で直接参照するコライダー領域。
	Toolbox::TVector<FColliderRecord2D> Colliders;
	// 再使用可能な空きコライダー番号。
	Toolbox::TVector<Toolbox::size_t> ColliderFree;
	// 前回Impulseの再利用記録。
	Toolbox::TVector<FCachedImpulse2D> Cache;
	// スロット番号で直接参照する距離拘束の領域。
	Toolbox::TVector<FJointRecord2D> Joints;
	// 再使用可能な空き拘束スロット番号。
	Toolbox::TVector<Toolbox::size_t> JointFree;
	// ワールド全体の重力加速度。
	Toolbox::FVector2 Gravity{0, -9.8f};
	// 接触拘束の解決設定。
	FContactSettings2D Contact;
	// 連続衝突の反復設定。
	FContinuousSettings2D Continuous;
	// 直近更新の連続衝突診断。
	FContinuousDiagnostics2D Diagnostics;
	// Step内部だけで利用する非所有Job Systemと並列化設定。
	FPhysicsExecutionSettings Execution;
	// 直近更新で集計した並列実行診断。
	FPhysicsExecutionDiagnostics ExecutionDiagnostics;
	// 明示的なSnapshot採取に渡すStep完了情報。
	PhysicsPrivate::FSnapshotStepState SnapshotState;
	// 接触・Triggerのイベントの記録（無効な間は領域を持たない）。
	PhysicsPrivate::TWorldEventTracker<FColliderId2D, Toolbox::FVector2> Events;
	// イベントの組の候補を作る作業領域（Stepをまたいで容量を使い回す）。
	Toolbox::TVector<PhysicsPrivate::FBroadPhaseEntry> EventEntries;
	// 接触の詳細判定に使う固定上限の作業領域。
	Toolbox::TVector<Toolbox::FContactPoint2D> EventHits;
	// 休止の条件。
	FSleepSettings2D Sleep;
	// World問い合わせの索引（Colliderの検索用の派生情報。形状・Bodyの所有者はこのWorld）。
	PhysicsPrivate::TQueryIndex<2> QueryIndex;
	// 生存しているColliderの数。
	Toolbox::size_t AliveColliders = 0;
	// Bodyスロットごとの、索引へ最後に反映した姿勢。
	Toolbox::TVector<FIndexedPose2D> IndexedPoses;
	// Colliderスロットごとの、索引へ最後に反映した姿勢でのWorld形状（索引の経路の問い合わせが使う）。
	Toolbox::TVector<decltype(FColliderRecord2D::Shape)> QueryWorldShapes;
	// 問い合わせで索引を使うか（検証用に総当たりの参照経路へ切り替えられる）。
	bool bQueryIndexEnabled = true;
	// Solverとイベントの組の候補に索引を使うか（検証用に総当たりの参照経路へ切り替えられる）。
	bool bSolverIndexEnabled = true;
	// 索引から集めた組の候補（Stepをまたいで容量を使い回す）。
	Toolbox::TVector<PhysicsPrivate::FColliderPair> SolverPairs;
	// 問い合わせの集計を加算するか。
	bool bQueryDiagnostics = false;
	// 問い合わせの集計（診断が有効な間だけconstの問い合わせから加算する）。
	mutable FWorldQueryDiagnostics QueryTotals;
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
	FBodyRecord2D* Find_Internal(FBodyId2D Id) noexcept
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
		FBodyRecord2D& Record = Slots[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
	}
	// IDが有効な登録を指す場合だけ記録を返す。
	const FBodyRecord2D* Find_Internal(FBodyId2D Id) const noexcept
	{		if (Id.World != World)
		{
			return nullptr;
		}
		if (Id.Index >= Slots.Size())
		{
			return nullptr;
		}
		// 世代が一致する有効な登録。
		const FBodyRecord2D& Record = Slots[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
	}
	// IDが有効な拘束を指す場合だけ記録を返す。別World・世代違いはnullptr。
	FJointRecord2D* FindJoint_Internal(FJointId2D Id) noexcept
	{
		if (Id.World != World || Id.Index >= Joints.Size())
		{
			return nullptr;
		}
		FJointRecord2D& Record = Joints[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
	}
	// IDが有効な拘束を指す場合だけ記録を返す。別World・世代違いはnullptr。
	const FJointRecord2D* FindJoint_Internal(FJointId2D Id) const noexcept
	{
		if (Id.World != World || Id.Index >= Joints.Size())
		{
			return nullptr;
		}
		const FJointRecord2D& Record = Joints[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
	}
	// 有効な拘束の記録を返す。期限切れIDは例外で通知する。
	FJointRecord2D& ResolveJoint_Internal(FJointId2D Id)
	{
		FJointRecord2D* Record = FindJoint_Internal(Id);
		if (Record == nullptr)
		{
			throw Toolbox::FException("Invalid 2D joint id");
		}
		return *Record;
	}
	// Bodyの重心と回転から、Local AnchorのWorld位置を出す。
	static Toolbox::FVector2 AnchorWorld_Internal(const FBodyRecord2D& Body, const Toolbox::FVector2& Local) noexcept
	{
		const Toolbox::f64 Cosine = Toolbox::Cos(Toolbox::f64(Body.Angle));
		const Toolbox::f64 Sine = Toolbox::Sin(Toolbox::f64(Body.Angle));
		return {static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.X) + Cosine * Toolbox::f64(Local.X) -
		                                  Sine * Toolbox::f64(Local.Y)),
		        static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.Y) + Sine * Toolbox::f64(Local.X) +
		                                  Cosine * Toolbox::f64(Local.Y))};
	}
	// 有効な記録を返す。期限切れIDは例外で通知する。
	FBodyRecord2D& Resolve_Internal(FBodyId2D Id)
	{
		FBodyRecord2D* Record = Find_Internal(Id);
		if (Record == nullptr)
		{
			throw Toolbox::FException("Invalid 2D body id");
		}
		return *Record;
	}
	// 有効な記録を返す。期限切れIDは例外で通知する。
	const FBodyRecord2D& Resolve_Internal(FBodyId2D Id) const
	{
		const FBodyRecord2D* Record = Find_Internal(Id);
		if (Record == nullptr)
		{
			throw Toolbox::FException("Invalid 2D body id");
		}
		return *Record;
	}
	// IDが有効な登録を指す場合だけコライダーを返す。
	FColliderRecord2D* FindCollider_Internal(FColliderId2D Id) noexcept
	{
		return const_cast<FColliderRecord2D*>(static_cast<const FImpl*>(this)->FindCollider_Internal(Id));
	}
	// IDが有効な登録を指す場合だけコライダーを返す。
	const FColliderRecord2D* FindCollider_Internal(FColliderId2D Id) const noexcept
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
		const FColliderRecord2D& Record = Colliders[Id.Index];
		const bool bMatches = Record.bAlive && Record.Generation == Id.Generation && Record.Body == Id.Body;
		return bMatches ? &Record : nullptr;
	}
	// 線分問い合わせとカテゴリ操作は、Step中と途中失敗後を拒否する（Snapshotと同じ状態ガード）。
	void RequireQueryState_Internal() const
	{
		if (SnapshotState.bInStep || !SnapshotState.bCaptureAllowed)
		{
			throw Toolbox::FException("2D world query requires an idle World with no incomplete Step");
		}
	}
	// 問い合わせカテゴリを操作するColliderを返す。無効・別World・削除済み・旧世代は例外。
	const FColliderRecord2D& ResolveQueryCollider_Internal(FColliderId2D Id) const
	{
		RequireQueryState_Internal();
		const FColliderRecord2D* Record = FindCollider_Internal(Id);
		if (Record == nullptr)
		{
			throw Toolbox::FException("Invalid 2D collider id");
		}
		return *Record;
	}
	// 二つのColliderが物理的に応答する組か（両方Solidで、衝突フィルターが互いに許す）。
	static bool RespondsTogether_Internal(const FColliderRecord2D& A, const FColliderRecord2D& B) noexcept
	{
		return A.Response == EColliderResponse::Solid && B.Response == EColliderResponse::Solid &&
		       AllowsCollisionPair(A.Collision, B.Collision);
	}
	// 区分・衝突フィルターの変更後、古い接触の記録を捨て、休止中のDynamicの剛体を起こす。
	void AfterResponseChange_Internal() noexcept
	{
		Cache.Clear();
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			FBodyRecord2D& Record = Slots[Index];
			if (Record.bAlive && Record.Type == EBodyType::Dynamic)
			{
				Record.bSleeping = false;
				Record.SleepTimer = 0;
			}
		}
	}
	// イベントの組の種類。Sensorを含む組はTrigger（一方がStatic以外）、Solid同士はContact（一方がDynamic）。
	// 衝突フィルターが許さない組・対象外の組は空。
	static Toolbox::TOptional<EWorldEventKind> EventKind_Internal(const FColliderRecord2D& A,
	                                                              const FBodyRecord2D& BodyA,
	                                                              const FColliderRecord2D& B,
	                                                              const FBodyRecord2D& BodyB) noexcept
	{
		if (!AllowsCollisionPair(A.Collision, B.Collision))
		{
			return {};
		}
		if (A.Response == EColliderResponse::Sensor || B.Response == EColliderResponse::Sensor)
		{
			if (BodyA.Type == EBodyType::Static && BodyB.Type == EBodyType::Static)
			{
				return {};
			}
			return EWorldEventKind::Trigger;
		}
		if (BodyA.Type != EBodyType::Dynamic && BodyB.Type != EBodyType::Dynamic)
		{
			return {};
		}
		return EWorldEventKind::Contact;
	}
	// 現在の形状の距離がMargin以下か。求められた場合はBからAへ向く法線を返す。
	bool EventTouch_Internal(const FColliderRecord2D& A, const FBodyRecord2D& BodyA, const FColliderRecord2D& B,
	                         const FBodyRecord2D& BodyB, Toolbox::f32 Margin,
	                         Toolbox::TOptional<Toolbox::FVector2>& Normal)
	{
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
		// 実形状の判定中は候補側の計測を止め、別の区間へ記録する。
		PhysicsPrivate::FWorldInteractionProbe::FRegion ExactProbe(
		    PhysicsPrivate::FWorldInteractionProbe::EPhase::Exact);
#endif
		const Toolbox::size_t IndexA = A.Shape.Index();
		const Toolbox::size_t IndexB = B.Shape.Index();
		Toolbox::FContactPoint2D Hit;
		if (IndexA == 0 && IndexB == 0)
		{
			Hit = Toolbox::FindContact(ToWorld_Internal(BodyA, A.Shape.template Get<0>()),
			                           ToWorld_Internal(BodyB, B.Shape.template Get<0>()));
		}
		else if (IndexA == 0 && IndexB == 1)
		{
			Hit = Toolbox::FindContact(ToWorld_Internal(BodyA, A.Shape.template Get<0>()),
			                           ToWorld_Internal(BodyB, B.Shape.template Get<1>()));
		}
		else if (IndexA == 1 && IndexB == 0)
		{
			Hit = Toolbox::FindContact(ToWorld_Internal(BodyA, A.Shape.template Get<1>()),
			                           ToWorld_Internal(BodyB, B.Shape.template Get<0>()));
		}
		else if (IndexA == 2 || IndexB == 2)
		{
			// カプセルを含む組は、Margin以下の点のうち最も深い点。
			Toolbox::FContactPoint2D Points[Toolbox::MaxCapsuleContacts2D];
			const Toolbox::uint32 Count = FindCapsulePair_Internal(A, BodyA, B, BodyB, Margin, Points);
			if (Count == 0)
			{
				return false;
			}
			Hit = Points[0];
			for (Toolbox::uint32 Index = 1; Index < Count; ++Index)
			{
				if (Points[Index].Separation < Hit.Separation)
				{
					Hit = Points[Index];
				}
			}
		}
		else
		{
			EventHits.Clear();
			FindBoxContacts_Internal(ToWorld_Internal(BodyA, A.Shape.template Get<1>()),
			                         ToWorld_Internal(BodyB, B.Shape.template Get<1>()), Margin, EventHits);
			if (EventHits.IsEmpty())
			{
				return false;
			}
			Hit = EventHits[0];
		}
		if (!(Hit.Separation <= Margin))
		{
			return false;
		}
		if (Hit.Normal.IsValid())
		{
			Normal = Hit.Normal;
		}
		return true;
	}
	// 成功したStepの完了姿勢から接触・Triggerの組を集める。正準順への整列は発行時に行う。
	void CollectEvents_Internal()
	{
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
		// 境界の作成・並べ替え・走査・絞り込み。入れ子のEventTouchだけは別集計。
		PhysicsPrivate::FWorldInteractionProbe::FRegion CandidateProbe(
		    PhysicsPrivate::FWorldInteractionProbe::EPhase::Candidate);
#endif
		const Toolbox::f32 Margin = Events.GetSettings().ContactMargin;
		// Static以外のBodyのColliderを「動く側」として、索引から組の候補を確保なしで通知する（Static同士は調べない）。
		// 発行時に正準順へ並べるため、候補の通知順は結果に影響しない。
		if (bSolverIndexEnabled && PhysicsPrivate::VisitIndexedPairs_Internal(
		                               QueryIndex, Colliders, Margin,
		                               [&](Toolbox::size_t Slot)
		                               {
			                               const FBodyRecord2D* Body = Find_Internal(Colliders[Slot].Body);
			                               return Body != nullptr && Body->Type != EBodyType::Static;
		                               },
		                               [&](Toolbox::size_t First, Toolbox::size_t Second)
		                               {
			                               ConsiderEventPair_Internal(First, Second, Margin);
		                               }))
		{
			return;
		}
		EventEntries.Clear();
		for (Toolbox::size_t Index = 0; Index < Colliders.Size(); ++Index)
		{
			const FColliderRecord2D& Record = Colliders[Index];
			if (!Record.bAlive)
			{
				continue;
			}
			const FBodyRecord2D* Body = Find_Internal(Record.Body);
			if (Body == nullptr)
			{
				continue;
			}
			PhysicsPrivate::FBroadPhaseEntry Entry;
			Entry.ColliderIndex = Index;
			Entry.BodyIndex = Record.Body.Index;
			Entry.BodyGeneration = Record.Body.Generation;
			// Static同士の組は動かないため調べない（BroadPhaseの「動く側」の印に使う）。
			Entry.bDynamic = Body->Type != EBodyType::Static;
			Entry.Bounds = ToBounds_Internal(*Body, Record);
			EventEntries.PushBack(Entry);
		}
		PhysicsPrivate::VisitWorldEventCandidates_Internal(EventEntries, Margin,
		                                                   [&](Toolbox::size_t First, Toolbox::size_t Second)
		                                                   {
			                                                   ConsiderEventPair_Internal(First, Second, Margin);
		                                                   });
	}
	// イベントの組の候補（Firstのスロットが小さい方）を調べ、接触・Triggerなら記録する。
	void ConsiderEventPair_Internal(Toolbox::size_t First, Toolbox::size_t Second, Toolbox::f32 Margin)
	{
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
		PhysicsPrivate::FWorldInteractionProbe::CountCandidate_Internal();
#endif
		const FColliderRecord2D& RecordA = Colliders[First];
		const FColliderRecord2D& RecordB = Colliders[Second];
		const FBodyRecord2D* BodyA = Find_Internal(RecordA.Body);
		const FBodyRecord2D* BodyB = Find_Internal(RecordB.Body);
		if (BodyA == nullptr || BodyB == nullptr)
		{
			return;
		}
		const Toolbox::TOptional<EWorldEventKind> Kind = EventKind_Internal(RecordA, *BodyA, RecordB, *BodyB);
		if (!Kind)
		{
			return;
		}
		typename decltype(Events)::FPair Pair;
		Pair.A = {RecordA.Body, First, RecordA.Generation};
		Pair.B = {RecordB.Body, Second, RecordB.Generation};
		Pair.Kind = *Kind;
		if (!EventTouch_Internal(RecordA, *BodyA, RecordB, *BodyB, *Kind == EWorldEventKind::Trigger ? 0.0f : Margin,
		                         Pair.Normal))
		{
			return;
		}
		if (*Kind == EWorldEventKind::Trigger)
		{
			Pair.Normal.Reset();
		}
		Events.Add(Pair);
	}
	// 前回の組が今回ない理由。
	EWorldEventEndReason EndReason_Internal(const typename decltype(Events)::FPair& Pair) const noexcept
	{
		const FColliderRecord2D* A = FindCollider_Internal(Pair.A);
		const FColliderRecord2D* B = FindCollider_Internal(Pair.B);
		const FBodyRecord2D* BodyA = A != nullptr ? Find_Internal(A->Body) : nullptr;
		const FBodyRecord2D* BodyB = B != nullptr ? Find_Internal(B->Body) : nullptr;
		if (BodyA == nullptr || BodyB == nullptr)
		{
			return EWorldEventEndReason::Removed;
		}
		const Toolbox::TOptional<EWorldEventKind> Kind = EventKind_Internal(*A, *BodyA, *B, *BodyB);
		if (!Kind || *Kind != Pair.Kind)
		{
			return EWorldEventEndReason::FilterChanged;
		}
		return EWorldEventEndReason::Separated;
	}
	// 正準順序が小さい方か調べる。
	static bool ColliderLess_Internal(const FColliderId2D& A, const FColliderId2D& B) noexcept
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
	// ローカル円をワールド形状へ変換する。
	static Toolbox::FCircle2D ToWorld_Internal(const FBodyRecord2D& Body, const Toolbox::FCircle2D& Local) noexcept
	{
		// 回転角の余弦と正弦。
		const Toolbox::f64 Cosine = Toolbox::Cos(Toolbox::f64(Body.Angle));
		const Toolbox::f64 Sine = Toolbox::Sin(Toolbox::f64(Body.Angle));
		Toolbox::FCircle2D World = Local;
		World.Center = {static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.X) + Cosine * Local.Center.X - Sine * Local.Center.Y),
		                static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.Y) + Sine * Local.Center.X + Cosine * Local.Center.Y)};
		return World;
	}
	// ローカル矩形をワールド形状へ変換する。
	static Toolbox::FOrientedBox2D ToWorld_Internal(const FBodyRecord2D& Body, const Toolbox::FOrientedBox2D& Local) noexcept
	{
		// 回転角の余弦と正弦。
		const Toolbox::f64 Cosine = Toolbox::Cos(Toolbox::f64(Body.Angle));
		const Toolbox::f64 Sine = Toolbox::Sin(Toolbox::f64(Body.Angle));
		Toolbox::FOrientedBox2D World = Local;
		World.Center = {static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.X) + Cosine * Local.Center.X - Sine * Local.Center.Y),
		                static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.Y) + Sine * Local.Center.X + Cosine * Local.Center.Y)};
		World.Angle = Body.Angle + Local.Angle;
		return World;
	}
	// ローカルカプセルをワールド形状へ変換する。中心線の両端を角度で回して重心へ足す。
	static Toolbox::FCapsule2D ToWorld_Internal(const FBodyRecord2D& Body, const Toolbox::FCapsule2D& Local) noexcept
	{
		// 回転角の余弦と正弦。
		const Toolbox::f64 Cosine = Toolbox::Cos(Toolbox::f64(Body.Angle));
		const Toolbox::f64 Sine = Toolbox::Sin(Toolbox::f64(Body.Angle));
		Toolbox::FCapsule2D World = Local;
		World.Start = {
		    static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.X) + Cosine * Local.Start.X - Sine * Local.Start.Y),
		    static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.Y) + Sine * Local.Start.X + Cosine * Local.Start.Y)};
		World.End = {
		    static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.X) + Cosine * Local.End.X - Sine * Local.End.Y),
		    static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.Y) + Sine * Local.End.X + Cosine * Local.End.Y)};
		return World;
	}
	// Colliderの現在の姿勢のワールド形状。
	static decltype(FColliderRecord2D::Shape) WorldShape_Internal(const FBodyRecord2D& Body,
	                                                              const FColliderRecord2D& Record) noexcept
	{
		return Record.Shape.Visit(
		    [&](const auto& Local)
		    {
			    return decltype(FColliderRecord2D::Shape){ToWorld_Internal(Body, Local)};
		    });
	}
	// カプセルを含む組の接触点（最大三点、Margin以下）。法線はB→A。
	static Toolbox::uint32 FindCapsulePair_Internal(const FColliderRecord2D& A, const FBodyRecord2D& BodyA,
	                                                const FColliderRecord2D& B, const FBodyRecord2D& BodyB,
	                                                Toolbox::f32 Margin,
	                                                Toolbox::FContactPoint2D (&Out)[Toolbox::MaxCapsuleContacts2D])
	{
		const auto WorldA = WorldShape_Internal(BodyA, A);
		const auto WorldB = WorldShape_Internal(BodyB, B);
		return WorldA.Visit(
		    [&](const auto& ShapeA)
		    {
			    return WorldB.Visit(
			        [&](const auto& ShapeB)
			        {
				        return CapsuleContacts_Internal(ShapeA, ShapeB, Margin, Out);
			        });
		    });
	}
	// Colliderの現在の姿勢での索引用の境界。
	static PhysicsPrivate::TQueryShapeBounds<2> ColliderQueryBounds_Internal(
	    const FBodyRecord2D& Body, const FColliderRecord2D& Record,
	    decltype(FColliderRecord2D::Shape)& OutWorld) noexcept
	{
		return Record.Shape.Visit(
		    [&](const auto& Local)
		    {
			    const auto World = ToWorld_Internal(Body, Local);
			    OutWorld = World;
			    return PhysicsPrivate::QueryShapeBounds_Internal(World);
		    });
	}
	// 問い合わせで使うColliderのWorld形状。索引の経路は索引へ反映した姿勢で保存した形状、総当たりは現在の姿勢から変換する。
	decltype(FColliderRecord2D::Shape) QueryWorldShape_Internal(Toolbox::size_t Slot, bool bIndexed) const
	{
		if (bIndexed)
		{
			return QueryWorldShapes[Slot];
		}
		const FColliderRecord2D& Record = Colliders[Slot];
		const FBodyRecord2D& Body = Resolve_Internal(Record.Body);
		return Record.Shape.Visit(
		    [&](const auto& Local)
		    {
			    return decltype(FColliderRecord2D::Shape){ToWorld_Internal(Body, Local)};
		    });
	}
	// Bodyに付くすべてのColliderの索引を、現在の姿勢へ合わせる。
	void RefreshBodyColliders_Internal(Toolbox::size_t BodySlot) noexcept
	{
		const FBodyRecord2D& Body = Slots[BodySlot];
		IndexedPoses[BodySlot].Position = Body.Position;
		IndexedPoses[BodySlot].Angle = Body.Angle;
		IndexedPoses[BodySlot].bValid = true;
		Toolbox::int32 Current = QueryIndex.GetFirstCollider(BodySlot);
		while (Current != PhysicsPrivate::TQueryIndex<2>::None)
		{
			const Toolbox::size_t Slot = static_cast<Toolbox::size_t>(Current);
			QueryIndex.Refresh(Slot, ColliderQueryBounds_Internal(Body, Colliders[Slot], QueryWorldShapes[Slot]));
			Current = QueryIndex.GetNextCollider(Slot);
		}
	}
	// Bodyの現在の姿勢が、索引へ最後に反映した姿勢と同じか（ビット単位の一致）。
	bool IsIndexedPose_Internal(Toolbox::size_t BodySlot) const noexcept
	{
		const FIndexedPose2D& Pose = IndexedPoses[BodySlot];
		const FBodyRecord2D& Body = Slots[BodySlot];
		return Pose.bValid && Pose.Position == Body.Position && Pose.Angle == Body.Angle;
	}
	// Stepで動き得るBody（Static以外）のうち、姿勢が変わったBodyのColliderの索引を、現在の姿勢へ合わせる。
	void RefreshMovingColliders_Internal() noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			const FBodyRecord2D& Body = Slots[Index];
			if (!Body.bAlive || Body.Type == EBodyType::Static || IsIndexedPose_Internal(Index))
			{
				continue;
			}
			RefreshBodyColliders_Internal(Index);
		}
	}
	// 問い合わせの候補の走査に使う状態。
	PhysicsPrivate::TQuerySource<2, FColliderRecord2D> QuerySource_Internal() const noexcept
	{
		return {QueryIndex, Colliders, bQueryIndexEnabled, bQueryDiagnostics, QueryTotals};
	}
	// 二つのコライダー組から接触点列を作る。
	void AppendPairManifold_Internal(const FColliderRecord2D& RecordA, const FColliderId2D& IdA,
	                                 const FColliderRecord2D& RecordB, const FColliderId2D& IdB,
	                                 const FBodyRecord2D& BodyA, const FBodyRecord2D& BodyB, FManifold2D& Manifold)
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
			const Toolbox::FContactPoint2D Hit =
			    Toolbox::FindContact(ToWorld_Internal(BodyA, RecordA.Shape.Get<0>()),
			                         ToWorld_Internal(BodyB, RecordB.Shape.Get<0>()));
			if (Hit.Separation > Contact.ContactSlop)
			{
				return;
			}
			FSolvePoint2D Point;
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
			const Toolbox::FContactPoint2D Hit =
			    Toolbox::FindContact(ToWorld_Internal(BodyA, RecordA.Shape.Get<0>()),
			                         ToWorld_Internal(BodyB, RecordB.Shape.Get<1>()));
			if (Hit.Separation > Contact.ContactSlop)
			{
				return;
			}
			FSolvePoint2D Point;
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
			const Toolbox::FContactPoint2D Hit =
			    Toolbox::FindContact(ToWorld_Internal(BodyA, RecordA.Shape.Get<1>()),
			                         ToWorld_Internal(BodyB, RecordB.Shape.Get<0>()));
			if (Hit.Separation > Contact.ContactSlop)
			{
				return;
			}
			FSolvePoint2D Point;
			Point.Position = Hit.Position;
			Point.Normal = Hit.Normal;
			Point.Separation = Hit.Separation;
			Point.FeatureId = Hit.FeatureId;
			Point.Friction = Friction;
			Point.Restitution = Restitution;
			Manifold.Points.PushBack(Point);
		}
		else if (IndexA == 2 || IndexB == 2)
		{
			// カプセルを含む組。最も近い点と中心線の両端（特徴ID 0〜2）。
			Toolbox::FContactPoint2D Hits[Toolbox::MaxCapsuleContacts2D];
			const Toolbox::uint32 Count =
			    FindCapsulePair_Internal(RecordA, BodyA, RecordB, BodyB, Contact.ContactSlop, Hits);
			for (Toolbox::uint32 Index = 0; Index < Count; ++Index)
			{
				FSolvePoint2D Point;
				Point.Position = Hits[Index].Position;
				Point.Normal = Hits[Index].Normal;
				Point.Separation = Hits[Index].Separation;
				Point.FeatureId = Hits[Index].FeatureId;
				Point.Friction = Friction;
				Point.Restitution = Restitution;
				Manifold.Points.PushBack(Point);
			}
		}
		else
		{
			Toolbox::TVector<Toolbox::FContactPoint2D> Hits;
			FindBoxContacts_Internal(ToWorld_Internal(BodyA, RecordA.Shape.Get<1>()),
			                         ToWorld_Internal(BodyB, RecordB.Shape.Get<1>()), Contact.ContactSlop, Hits);
			for (Toolbox::size_t Index = 0; Index < Hits.Size(); ++Index)
			{
				FSolvePoint2D Point;
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
	// 指定機能へ借用Job Systemを使うか。1レーンでも同じ並列経路を通す。
	bool UseBorrowedJobs_Internal(bool bEnabled) const noexcept
	{
		return bEnabled && Execution.JobSystem != nullptr;
	}
	// 借用Job Systemの実行レーン数を返す。借用なしは1。
	Toolbox::uint32 ExecutionLanes_Internal() const noexcept
	{
		return Execution.JobSystem != nullptr ? Execution.JobSystem->GetExecutionThreadCount() : 1;
	}
	// コライダーのワールド軸平行境界を作る。計算はf32で行いf64へ広げる。
	static PhysicsPrivate::FBroadPhaseBounds ToBounds_Internal(const FBodyRecord2D& Body,
	                                                          const FColliderRecord2D& Record) noexcept
	{
		PhysicsPrivate::FBroadPhaseBounds Bounds;
		if (Record.Shape.Index() == 0)
		{
			// 中心と半径のワールド円。
			const Toolbox::FCircle2D World = ToWorld_Internal(Body, Record.Shape.Get<0>());
			Bounds.MinX = Toolbox::f64(World.Center.X - World.Radius);
			Bounds.MinY = Toolbox::f64(World.Center.Y - World.Radius);
			Bounds.MaxX = Toolbox::f64(World.Center.X + World.Radius);
			Bounds.MaxY = Toolbox::f64(World.Center.Y + World.Radius);
			return Bounds;
		}
		if (Record.Shape.Index() == 2)
		{
			// 中心線の両端の円を覆うワールド境界。
			const Toolbox::FAABB2D Box = Toolbox::CapsuleBounds(ToWorld_Internal(Body, Record.Shape.Get<2>()));
			Bounds.MinX = Box.Min.X;
			Bounds.MinY = Box.Min.Y;
			Bounds.MaxX = Box.Max.X;
			Bounds.MaxY = Box.Max.Y;
			return Bounds;
		}
		// 回転矩形のワールド形状。
		const Toolbox::FOrientedBox2D World = ToWorld_Internal(Body, Record.Shape.Get<1>());
		// 回転角の余弦と正弦。
		const Toolbox::f64 Cosine = Toolbox::Cos(Toolbox::f64(World.Angle));
		const Toolbox::f64 Sine = Toolbox::Sin(Toolbox::f64(World.Angle));
		// 四隅から範囲を広げる。
		bool bFirst = true;
		for (Toolbox::int32 Corner = 0; Corner < 4; ++Corner)
		{
			const Toolbox::f64 SignX = (Corner & 1) == 0 ? -1 : 1;
			const Toolbox::f64 SignY = (Corner & 2) == 0 ? -1 : 1;
			const Toolbox::f64 PointX =
			    Toolbox::f64(World.Center.X) + Cosine * SignX * World.HalfExtents.X - Sine * SignY * World.HalfExtents.Y;
			const Toolbox::f64 PointY =
			    Toolbox::f64(World.Center.Y) + Sine * SignX * World.HalfExtents.X + Cosine * SignY * World.HalfExtents.Y;
			if (bFirst)
			{
				Bounds.MinX = PointX;
				Bounds.MinY = PointY;
				Bounds.MaxX = PointX;
				Bounds.MaxY = PointY;
				bFirst = false;
			}
			else
			{
				if (PointX < Bounds.MinX)
				{
					Bounds.MinX = PointX;
				}
				if (PointY < Bounds.MinY)
				{
					Bounds.MinY = PointY;
				}
				if (PointX > Bounds.MaxX)
				{
					Bounds.MaxX = PointX;
				}
				if (PointY > Bounds.MaxY)
				{
					Bounds.MaxY = PointY;
				}
			}
		}
		return Bounds;
	}
	// BroadPhaseの入力を有効なコライダーから作る。
	void BuildBroadPhaseEntries_Internal(Toolbox::TVector<PhysicsPrivate::FBroadPhaseEntry>& Out) const
	{
		Out.Clear();
		for (Toolbox::size_t Index = 0; Index < Colliders.Size(); ++Index)
		{
			const FColliderRecord2D& Record = Colliders[Index];
			if (!Record.bAlive)
			{
				continue;
			}
			const FBodyRecord2D* Body = Find_Internal(Record.Body);
			if (Body == nullptr)
			{
				continue;
			}
			PhysicsPrivate::FBroadPhaseEntry Entry;
			Entry.ColliderIndex = Index;
			Entry.BodyIndex = Record.Body.Index;
			Entry.BodyGeneration = Record.Body.Generation;
			Entry.bDynamic = Body->Type == EBodyType::Dynamic;
			Entry.Bounds = ToBounds_Internal(*Body, Record);
			Out.PushBack(Entry);
		}
	}
	// 一つの候補組から接触点列を作る。呼び出し側の専用領域だけを書く。
	// 共有状態は読み取りだけにし、構造変更は行わない。
	void GeneratePairManifold_Internal(const PhysicsPrivate::FBroadPhasePair& Pair, FManifold2D& Manifold)
	{
		const FColliderRecord2D& RecordA = Colliders[Pair.FirstColliderIndex];
		const FColliderRecord2D& RecordB = Colliders[Pair.SecondColliderIndex];
		const FBodyRecord2D* BodyA = Find_Internal(RecordA.Body);
		const FBodyRecord2D* BodyB = Find_Internal(RecordB.Body);
		if (BodyA == nullptr || BodyB == nullptr)
		{
			return;
		}
		// Sensorを含む組と、衝突フィルターが許さない組は物理応答をしない。
		if (!RespondsTogether_Internal(RecordA, RecordB))
		{
			return;
		}
		const FColliderId2D IdA = {RecordA.Body, Pair.FirstColliderIndex, RecordA.Generation};
		const FColliderId2D IdB = {RecordB.Body, Pair.SecondColliderIndex, RecordB.Generation};
		if (ColliderLess_Internal(IdB, IdA))
		{
			AppendPairManifold_Internal(RecordB, IdB, RecordA, IdA, *BodyB, *BodyA, Manifold);
		}
		else
		{
			AppendPairManifold_Internal(RecordA, IdA, RecordB, IdB, *BodyA, *BodyB, Manifold);
		}
	}
	// BroadPhaseの候補組から多様体列を作る。組順序を保ち、空の多様体は除く。
	void GenerateParallelManifolds_Internal(Toolbox::TVector<FManifold2D>& Out)
	{
		Toolbox::TVector<PhysicsPrivate::FBroadPhaseEntry> Entries;
		BuildBroadPhaseEntries_Internal(Entries);
		Toolbox::TVector<PhysicsPrivate::FBroadPhasePair> Pairs;
		Toolbox::FJobSystem* BroadJobs = UseBorrowedJobs_Internal(Execution.bParallelBroadPhase) ? Execution.JobSystem : nullptr;
		PhysicsPrivate::FBroadPhase::Generate(Entries, Contact.ContactSlop, BroadJobs, Pairs);
		ExecutionDiagnostics.CandidatePairCount += Pairs.Size();
		// 組ごとの専用多様体。
		Toolbox::TVector<FManifold2D> PairManifolds(Pairs.Size());
		auto GenerateOne = [&](Toolbox::size_t Index)
		{
			GeneratePairManifold_Internal(Pairs[Index], PairManifolds[Index]);
		};
		if (UseBorrowedJobs_Internal(Execution.bParallelNarrowPhase))
		{
			if (!Toolbox::ParallelFor(*Execution.JobSystem, Pairs.Size(), GenerateOne, 8))
			{
				throw Toolbox::FException("Parallel 2D narrow phase failed");
			}
		}
		else
		{
			for (Toolbox::size_t Index = 0; Index < Pairs.Size(); ++Index)
			{
				GenerateOne(Index);
			}
		}
		for (Toolbox::size_t Index = 0; Index < PairManifolds.Size(); ++Index)
		{
			if (!PairManifolds[Index].Points.IsEmpty())
			{
				Out.PushBack(Toolbox::Move(PairManifolds[Index]));
			}
		}
		ExecutionDiagnostics.ManifoldCount += Out.Size();
	}
	// 設定に応じて直列またはBroadPhase経由で多様体列を作る。
	void GenerateStepManifolds_Internal(Toolbox::TVector<FManifold2D>& Out)
	{
		if (Execution.JobSystem == nullptr)
		{
			GenerateManifolds_Internal(Out);
			return;
		}
		GenerateParallelManifolds_Internal(Out);
	}
	// コライダー組（First<Secondのスロット）の接触を調べ、接触点があれば多様体を加える。索引の経路と総当たりで共用する。
	void ConsiderSolverPair_Internal(Toolbox::size_t First, Toolbox::size_t Second, Toolbox::TVector<FManifold2D>& Out)
	{
		FColliderRecord2D& RecordA = Colliders[First];
		FColliderRecord2D& RecordB = Colliders[Second];
		if (!RecordA.bAlive || !RecordB.bAlive)
		{
			return;
		}
		FBodyRecord2D* BodyA = Find_Internal(RecordA.Body);
		FBodyRecord2D* BodyB = Find_Internal(RecordB.Body);
		if (BodyA == nullptr || BodyB == nullptr)
		{
			return;
		}
		// 同一剛体の組は自分自身へ接触しない。
		if (RecordA.Body == RecordB.Body)
		{
			return;
		}
		// 両方が非Dynamicの組は応答も運動もしない。
		if (BodyA->Type != EBodyType::Dynamic && BodyB->Type != EBodyType::Dynamic)
		{
			return;
		}
		// Sensorを含む組と、衝突フィルターが許さない組は物理応答をしない。
		if (!RespondsTogether_Internal(RecordA, RecordB))
		{
			return;
		}
		++ExecutionDiagnostics.CandidatePairCount;
		const FColliderId2D IdA = {RecordA.Body, First, RecordA.Generation};
		const FColliderId2D IdB = {RecordB.Body, Second, RecordB.Generation};
		FManifold2D Manifold;
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
			++ExecutionDiagnostics.ManifoldCount;
			Out.PushBack(Manifold);
		}
	}
	// 接触する組から多様体列を作る。索引の経路は、索引を現在の姿勢へ合わせてから候補の組を集め、総当たりと同じ
	// (First, Second)の昇順で同じ組の関数を呼ぶ（結果は総当たりとビット単位で同じ）。索引を使えない場合は総当たり。
	void GenerateManifolds_Internal(Toolbox::TVector<FManifold2D>& Out)
	{
		if (bSolverIndexEnabled)
		{
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
			// 索引の更新・候補の収集・並べ替え（BroadPhase）だけを記録する。
			PhysicsPrivate::FWorldInteractionProbe::FRegion PairProbe(
			    PhysicsPrivate::FWorldInteractionProbe::EPhase::SolverPairs);
#endif
			RefreshMovingColliders_Internal();
			const bool bIndexed = PhysicsPrivate::CollectIndexedPairs_Internal(
			    QueryIndex, Colliders, Contact.ContactSlop,
			    [&](Toolbox::size_t Slot)
			    {
				    const FBodyRecord2D* Body = Find_Internal(Colliders[Slot].Body);
				    return Body != nullptr && Body->Type == EBodyType::Dynamic;
			    },
			    SolverPairs);
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
			PairProbe.Stop();
#endif
			if (bIndexed)
			{
				for (Toolbox::size_t Index = 0; Index < SolverPairs.Size(); ++Index)
				{
					ConsiderSolverPair_Internal(SolverPairs[Index].First, SolverPairs[Index].Second, Out);
				}
				return;
			}
		}
		for (Toolbox::size_t First = 0; First < Colliders.Size(); ++First)
		{
			for (Toolbox::size_t Second = First + 1; Second < Colliders.Size(); ++Second)
			{
				ConsiderSolverPair_Internal(First, Second, Out);
			}
		}
	}
	// 接触点の相対速度を求める。
	static Toolbox::FVector2 RelativeVelocity_Internal(const FBodyRecord2D& BodyA, const FBodyRecord2D& BodyB,
	                                                  Toolbox::FVector2 Point) noexcept
	{
		// 腕の回転による速度。
		const Toolbox::f64 ArmAX = Toolbox::f64(Point.X) - BodyA.Position.X;
		const Toolbox::f64 ArmAY = Toolbox::f64(Point.Y) - BodyA.Position.Y;
		const Toolbox::f64 ArmBX = Toolbox::f64(Point.X) - BodyB.Position.X;
		const Toolbox::f64 ArmBY = Toolbox::f64(Point.Y) - BodyB.Position.Y;
		const Toolbox::f64 VelocityAX = Toolbox::f64(BodyA.Velocity.X) - Toolbox::f64(BodyA.AngularVelocity) * ArmAY;
		const Toolbox::f64 VelocityAY = Toolbox::f64(BodyA.Velocity.Y) + Toolbox::f64(BodyA.AngularVelocity) * ArmAX;
		const Toolbox::f64 VelocityBX = Toolbox::f64(BodyB.Velocity.X) - Toolbox::f64(BodyB.AngularVelocity) * ArmBY;
		const Toolbox::f64 VelocityBY = Toolbox::f64(BodyB.Velocity.Y) + Toolbox::f64(BodyB.AngularVelocity) * ArmBX;
		return {static_cast<Toolbox::f32>(VelocityAX - VelocityBX),
		        static_cast<Toolbox::f32>(VelocityAY - VelocityBY)};
	}
	// 休止中の剛体を起こす。
	static void Wake_Internal(FBodyRecord2D& Record) noexcept
	{
		// 既に起きている対象への書き込みは並列Islandの競合になるため省く。
		if (!Record.bSleeping && Record.SleepTimer == 0)
		{
			return;
		}
		Record.bSleeping = false;
		Record.SleepTimer = 0;
	}
	// 全登録の休止を解く。設定変更で凍結を残さない。
	void WakeAll_Internal() noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			FBodyRecord2D& Record = Slots[Index];
			if (Record.bAlive)
			{
				Wake_Internal(Record);
			}
		}
	}
	// 休止中は無限質量として扱う逆質量を返す。
	static Toolbox::f32 EffectiveInverseMass_Internal(const FBodyRecord2D& Record) noexcept
	{
		return Record.bSleeping ? 0 : Record.InverseMass;
	}
	// 休止中は無限慣性として扱う逆慣性を返す。
	static Toolbox::f32 EffectiveInverseInertia_Internal(const FBodyRecord2D& Record) noexcept
	{
		return Record.bSleeping ? 0 : Record.InverseInertia;
	}
	// 前回確定の接触点相対運動で起床が必要か調べる。島伝播の第一段階。
	// Kinematicは力積分を受けないため現在速度を使い、Dynamicは今刻みの
	// 重力・外力分を除くため前回確定値を使う。休止中は止まっている。
	bool ShouldWakeForMotion_Internal(const FBodyRecord2D& BodyA, const FBodyRecord2D& BodyB,
	                                  Toolbox::FVector2 Point) const noexcept
	{
		if (!BodyA.bSleeping && !BodyB.bSleeping)
		{
			return false;
		}
		Toolbox::FVector2 PrevVelA = BodyA.Type == EBodyType::Kinematic ? BodyA.Velocity : BodyA.PrevVelocity;
		Toolbox::f32 PrevSpinA = BodyA.Type == EBodyType::Kinematic ? BodyA.AngularVelocity : BodyA.PrevAngularVelocity;
		Toolbox::FVector2 PrevVelB = BodyB.Type == EBodyType::Kinematic ? BodyB.Velocity : BodyB.PrevVelocity;
		Toolbox::f32 PrevSpinB = BodyB.Type == EBodyType::Kinematic ? BodyB.AngularVelocity : BodyB.PrevAngularVelocity;
		if (BodyA.bSleeping)
		{
			PrevVelA = {};
			PrevSpinA = 0;
		}
		if (BodyB.bSleeping)
		{
			PrevVelB = {};
			PrevSpinB = 0;
		}
		const Toolbox::f64 ArmAX = Toolbox::f64(Point.X) - BodyA.Position.X;
		const Toolbox::f64 ArmAY = Toolbox::f64(Point.Y) - BodyA.Position.Y;
		const Toolbox::f64 ArmBX = Toolbox::f64(Point.X) - BodyB.Position.X;
		const Toolbox::f64 ArmBY = Toolbox::f64(Point.Y) - BodyB.Position.Y;
		const Toolbox::f64 PointAX = Toolbox::f64(PrevVelA.X) - Toolbox::f64(PrevSpinA) * ArmAY;
		const Toolbox::f64 PointAY = Toolbox::f64(PrevVelA.Y) + Toolbox::f64(PrevSpinA) * ArmAX;
		const Toolbox::f64 PointBX = Toolbox::f64(PrevVelB.X) - Toolbox::f64(PrevSpinB) * ArmBY;
		const Toolbox::f64 PointBY = Toolbox::f64(PrevVelB.Y) + Toolbox::f64(PrevSpinB) * ArmBX;
		const Toolbox::f64 GapX = PointAX - PointBX;
		const Toolbox::f64 GapY = PointAY - PointBY;
		return Toolbox::Sqrt(GapX * GapX + GapY * GapY) > Sleep.LinearSpeedLimit;
	}
	// 前回Impulseを適用し、反発目標の基準速度を保存する。
	void WarmStart_Internal(FManifold2D& Manifold)
	{
		FBodyRecord2D* BodyA = Find_Internal(Manifold.BodyA);
		FBodyRecord2D* BodyB = Find_Internal(Manifold.BodyB);
		if (BodyA == nullptr || BodyB == nullptr)
		{
			return;
		}
		// 新しい接触は休止中の剛体を起こす。一方向ずつの伝播で島へ広がる。
		bool bKnownPair = false;
		for (Toolbox::size_t Index = 0; Index < Manifold.Points.Size(); ++Index)
		{
			FSolvePoint2D& Point = Manifold.Points[Index];
			const Toolbox::FVector2 Relative = RelativeVelocity_Internal(*BodyA, *BodyB, Point.Position);
			Point.ApproachSpeed = Toolbox::f64(Relative.X) * Point.Normal.X + Toolbox::f64(Relative.Y) * Point.Normal.Y;
			if (ShouldWakeForMotion_Internal(*BodyA, *BodyB, Point.Position))
			{
				Wake_Internal(*BodyA);
				Wake_Internal(*BodyB);
			}
			Point.NormalImpulse = 0;
			Point.TangentImpulse = 0;
			for (Toolbox::size_t CacheIndex = 0; CacheIndex < Cache.Size(); ++CacheIndex)
			{
				const FCachedImpulse2D& Cached = Cache[CacheIndex];
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
				                              Toolbox::f64(Cached.Normal.Y) * Point.Normal.Y;
				if (Agreement < 0.99)
				{
					continue;
				}
				Point.NormalImpulse = Cached.NormalImpulse;
				Point.TangentImpulse = Cached.TangentImpulse;
				// 保存したImpulseを即時適用する。
				const Toolbox::FVector2 Tangent = {-Point.Normal.Y, Point.Normal.X};
				const Toolbox::FVector2 Push = {Point.Normal.X * Point.NormalImpulse + Tangent.X * Point.TangentImpulse,
				                                Point.Normal.Y * Point.NormalImpulse + Tangent.Y * Point.TangentImpulse};
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
	// 速度へImpulseを適用する。一つ目に足し、二つ目から引く。
	static void ApplyImpulse_Internal(FBodyRecord2D& BodyA, FBodyRecord2D& BodyB, Toolbox::FVector2 Point,
	                                  Toolbox::FVector2 Push) noexcept
	{
		// 休止中は無限質量として扱う。
		const Toolbox::f32 InverseMassA = EffectiveInverseMass_Internal(BodyA);
		const Toolbox::f32 InverseMassB = EffectiveInverseMass_Internal(BodyB);
		const Toolbox::f32 InverseInertiaA = EffectiveInverseInertia_Internal(BodyA);
		const Toolbox::f32 InverseInertiaB = EffectiveInverseInertia_Internal(BodyB);
		// 腕とImpulseの外積。
		const Toolbox::f64 ArmAX = Toolbox::f64(Point.X) - BodyA.Position.X;
		const Toolbox::f64 ArmAY = Toolbox::f64(Point.Y) - BodyA.Position.Y;
		const Toolbox::f64 ArmBX = Toolbox::f64(Point.X) - BodyB.Position.X;
		const Toolbox::f64 ArmBY = Toolbox::f64(Point.Y) - BodyB.Position.Y;
		const Toolbox::f64 PushX = Push.X;
		const Toolbox::f64 PushY = Push.Y;
		// 実効質量がゼロの対象への書き込みは並列Islandの競合になるため省く。
		// 有限値へのゼロ加算と変わらず、非有限値も保存される。
		if (InverseMassA != 0 || InverseInertiaA != 0)
		{
			BodyA.Velocity += {static_cast<Toolbox::f32>(PushX * InverseMassA),
			                   static_cast<Toolbox::f32>(PushY * InverseMassA)};
			BodyA.AngularVelocity = static_cast<Toolbox::f32>(Toolbox::f64(BodyA.AngularVelocity) +
			                                                   (ArmAX * PushY - ArmAY * PushX) * InverseInertiaA);
		}
		if (InverseMassB != 0 || InverseInertiaB != 0)
		{
			BodyB.Velocity += {static_cast<Toolbox::f32>(-PushX * InverseMassB),
			                   static_cast<Toolbox::f32>(-PushY * InverseMassB)};
			BodyB.AngularVelocity = static_cast<Toolbox::f32>(Toolbox::f64(BodyB.AngularVelocity) -
			                                                   (ArmBX * PushY - ArmBY * PushX) * InverseInertiaB);
		}
	}
	// 単一接触点の速度拘束を解く。
	static void SolvePoint_Internal(FBodyRecord2D& BodyA, FBodyRecord2D& BodyB, FSolvePoint2D& Point,
	                                Toolbox::f32 RestitutionThreshold) noexcept
	{
		// 休止中は無限質量として扱う。
		const Toolbox::f64 InverseMassA = EffectiveInverseMass_Internal(BodyA);
		const Toolbox::f64 InverseMassB = EffectiveInverseMass_Internal(BodyB);
		const Toolbox::f64 InverseInertiaA = EffectiveInverseInertia_Internal(BodyA);
		const Toolbox::f64 InverseInertiaB = EffectiveInverseInertia_Internal(BodyB);
		// 腕。
		const Toolbox::f64 ArmAX = Toolbox::f64(Point.Position.X) - BodyA.Position.X;
		const Toolbox::f64 ArmAY = Toolbox::f64(Point.Position.Y) - BodyA.Position.Y;
		const Toolbox::f64 ArmBX = Toolbox::f64(Point.Position.X) - BodyB.Position.X;
		const Toolbox::f64 ArmBY = Toolbox::f64(Point.Position.Y) - BodyB.Position.Y;
		// 法線の有効質量。
		const Toolbox::f64 CrossNA = ArmAX * Point.Normal.Y - ArmAY * Point.Normal.X;
		const Toolbox::f64 CrossNB = ArmBX * Point.Normal.Y - ArmBY * Point.Normal.X;
		const Toolbox::f64 NormalMass = InverseMassA + InverseMassB + InverseInertiaA * CrossNA * CrossNA +
		                                InverseInertiaB * CrossNB * CrossNB;
		if (NormalMass <= 0)
		{
			return;
		}
		const Toolbox::FVector2 Relative = RelativeVelocity_Internal(BodyA, BodyB, Point.Position);
		const Toolbox::f64 NormalSpeed = Toolbox::f64(Relative.X) * Point.Normal.X + Toolbox::f64(Relative.Y) * Point.Normal.Y;
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
		                      {static_cast<Toolbox::f32>(Point.Normal.X * Difference),
		                       static_cast<Toolbox::f32>(Point.Normal.Y * Difference)});
		// 接線の有効質量。
		const Toolbox::f64 TangentX = -Toolbox::f64(Point.Normal.Y);
		const Toolbox::f64 TangentY = Toolbox::f64(Point.Normal.X);
		const Toolbox::f64 CrossTA = ArmAX * TangentY - ArmAY * TangentX;
		const Toolbox::f64 CrossTB = ArmBX * TangentY - ArmBY * TangentX;
		const Toolbox::f64 TangentMass = InverseMassA + InverseMassB + InverseInertiaA * CrossTA * CrossTA +
		                                 InverseInertiaB * CrossTB * CrossTB;
		if (TangentMass <= 0)
		{
			return;
		}
		const Toolbox::FVector2 Sliding = RelativeVelocity_Internal(BodyA, BodyB, Point.Position);
		const Toolbox::f64 TangentSpeed = Toolbox::f64(Sliding.X) * TangentX + Toolbox::f64(Sliding.Y) * TangentY;
		const Toolbox::f64 LambdaT = -TangentSpeed / TangentMass;
		// 摩擦の合計は法線Impulseに比例する。
		const Toolbox::f64 Limit = Toolbox::f64(Point.Friction) * Point.NormalImpulse;
		const Toolbox::f64 OldT = Point.TangentImpulse;
		const Toolbox::f64 Clamped = Toolbox::Clamp(OldT + LambdaT, -Limit, Limit);
		Point.TangentImpulse = static_cast<Toolbox::f32>(Clamped);
		const Toolbox::f64 DifferenceT = Clamped - OldT;
		ApplyImpulse_Internal(BodyA, BodyB, Point.Position,
		                      {static_cast<Toolbox::f32>(TangentX * DifferenceT),
		                       static_cast<Toolbox::f32>(TangentY * DifferenceT)});
	}
	// 軸の縮退を判定する距離閾値。Contactの分離判定と同じ精度帯に置く。
	// これ未満のDeltaは方向を決められないため保存軸か拘束不受 理へ落とす。
	static constexpr Toolbox::f64 JointAxisEpsilon = 1e-6;
	// 位置補正が1Stepで動かす距離の上限。ContactのMaxCorrectionと同量で、
	// 誤差が重なっても1StepのOutlineを超えないようにする。
	static constexpr Toolbox::f64 JointMaxPositionCorrection = 0.05;
	// 位置補正で1Stepに解消する誤差の割合。ContactのBaumgarteBetaと同値で、
	// 1/60秒固定Stepの過減衰相当になり、600Stepで振動せず収束する。
	static constexpr Toolbox::f64 JointPositionBeta = 0.2;
	// 位置補正の反復回数。姿勢補正でAxisが変わるため、Frameを作り直しながら解く。
	// 3Dと同じ4回を使う。1回だと回転後に古いAxisで補正し、誤差が積み上がる。
	static constexpr Toolbox::uint32 JointPositionIterations = 4;
	// Local AnchorをBodyの姿勢で回転し、World Anchorと腕を求める。
	// 節点ごとにFJointFrame2Dを生成し、速度拘束と位置補正で共有する。
	struct FJointFrame2D
	{
		// A→Bの単位軸。
		Toolbox::FVector2 Axis{1, 0};
		// 重心からA側AnchorまでのWorldベクトル。
		Toolbox::FVector2 ArmA;
		// 重心からB側AnchorまでのWorldベクトル。
		Toolbox::FVector2 ArmB;
		// A側AnchorのWorld位置。
		Toolbox::FVector2 PositionA;
		// B側AnchorのWorld位置。
		Toolbox::FVector2 PositionB;
		// Anchor間距離。
		Toolbox::f64 Distance = 0;
		// 軸が確定せず、方向を捏造せずにImpulseを生成できない状態か。
		bool bAxisUnavailable = false;
		// 距離誤差。正なら伸びている。
		Toolbox::f64 Error = 0;
	};
	// 現在のBody姿勢からJointのWorld Anchorと軸を組み立てる。
	// 距離0かつ正のLengthでは保存軸を使い、無ければ軸未確定として扱う。
	bool BuildJointFrame_Internal(const FJointRecord2D& Joint, const FBodyRecord2D& BodyA, const FBodyRecord2D& BodyB,
	                              FJointFrame2D& Out) const noexcept	{
		// Local Anchorを各Bodyの姿勢でWorld方向へ回す。World座標として足さない。
		const Toolbox::f64 CosineA = Toolbox::Cos(Toolbox::f64(BodyA.Angle));
		const Toolbox::f64 SineA = Toolbox::Sin(Toolbox::f64(BodyA.Angle));
		const Toolbox::f64 CosineB = Toolbox::Cos(Toolbox::f64(BodyB.Angle));
		const Toolbox::f64 SineB = Toolbox::Sin(Toolbox::f64(BodyB.Angle));
		const Toolbox::f64 LocalAX = Toolbox::f64(Joint.LocalAnchorA.X);
		const Toolbox::f64 LocalAY = Toolbox::f64(Joint.LocalAnchorA.Y);
		const Toolbox::f64 LocalBX = Toolbox::f64(Joint.LocalAnchorB.X);
		const Toolbox::f64 LocalBY = Toolbox::f64(Joint.LocalAnchorB.Y);
		Out.ArmA = {static_cast<Toolbox::f32>(CosineA * LocalAX - SineA * LocalAY),
		            static_cast<Toolbox::f32>(SineA * LocalAX + CosineA * LocalAY)};
		Out.ArmB = {static_cast<Toolbox::f32>(CosineB * LocalBX - SineB * LocalBY),
		            static_cast<Toolbox::f32>(SineB * LocalBX + CosineB * LocalBY)};
		Out.PositionA = {static_cast<Toolbox::f32>(Toolbox::f64(BodyA.Position.X) + Toolbox::f64(Out.ArmA.X)),
		                 static_cast<Toolbox::f32>(Toolbox::f64(BodyA.Position.Y) + Toolbox::f64(Out.ArmA.Y))};
		Out.PositionB = {static_cast<Toolbox::f32>(Toolbox::f64(BodyB.Position.X) + Toolbox::f64(Out.ArmB.X)),
		                 static_cast<Toolbox::f32>(Toolbox::f64(BodyB.Position.Y) + Toolbox::f64(Out.ArmB.Y))};
		const Toolbox::f64 DeltaX = Toolbox::f64(Out.PositionB.X) - Toolbox::f64(Out.PositionA.X);
		const Toolbox::f64 DeltaY = Toolbox::f64(Out.PositionB.Y) - Toolbox::f64(Out.PositionA.Y);
		const Toolbox::f64 Square = DeltaX * DeltaX + DeltaY * DeltaY;
		Out.Distance = Toolbox::Sqrt(Square);
		Out.Error = Out.Distance - Joint.Length;
		// 非有限は成功した0Impulseへ落とさず、軸未確定としてStepを止める。
		if (!Toolbox::IsFinite(Square) || !Toolbox::IsFinite(Out.Error))
		{
			Out.bAxisUnavailable = true;
			return false;
		}
		if (Out.Distance > JointAxisEpsilon)
		{
			Out.Axis = {static_cast<Toolbox::f32>(DeltaX / Out.Distance), static_cast<Toolbox::f32>(DeltaY / Out.Distance)};
			Out.bAxisUnavailable = false;
			return true;
		}
		// 距離が縮退したとき。Length0なら拘束は既に満たされるので何もしない。
		if (Joint.Length <= 0)
		{
			Out.bAxisUnavailable = true;
			return false;
		}
		// 正のLengthでAnchorが重なった場合は保存軸を南方。捏造した固定軸は使わない。
		if (!Joint.bHasLastValidAxis)
		{
			Out.bAxisUnavailable = true;
			return false;
		}
		Out.Axis = Joint.LastValidAxis;
		Out.bAxisUnavailable = false;
		return true;
	}
	// 保存軸を今回の確定軸で更新する。距離0のStepでは上書きしない。
	static void UpdateLastAxis_Internal(FJointRecord2D& Joint, const FJointFrame2D& Frame) noexcept
	{
		if (Frame.bAxisUnavailable)
		{
			return;
		}
		Joint.LastValidAxis = Frame.Axis;
		Joint.bHasLastValidAxis = true;
	}
	// 生存JointへWarm StartのImpulseを適用する。
	// Axisは現在の姿勢から作り直し、前回Stepの軸をまたいで使わない。
	void WarmStartJoints_Internal() noexcept
	{
		for (Toolbox::size_t Slot = 0; Slot < Joints.Size(); ++Slot)
		{
			FJointRecord2D& Joint = Joints[Slot];
			if (!Joint.bAlive)
			{
				continue;
			}
			// 両Bodyの速度と並列化フラグはWarm Start後に変わるため、Main側で更新する。
			FBodyRecord2D* BodyA = Find_Internal(Joint.BodyA);
			FBodyRecord2D* BodyB = Find_Internal(Joint.BodyB);
			if (BodyA == nullptr || BodyB == nullptr)
			{
				continue;
			}
			FJointFrame2D Frame;
			if (!BuildJointFrame_Internal(Joint, *BodyA, *BodyB, Frame))
			{
				// 軸が確定できないStepでは前回のImpulseを適用しない。
				Joint.AccumulatedImpulse = 0;
				continue;
			}
			UpdateLastAxis_Internal(Joint, Frame);
			if (Joint.AccumulatedImpulse == 0)
			{
				continue;
			}
			// Warm Startは前回の確定Impulseを即時適用する。
			// AccumulatedImpulseは残し、速度拘束が前回総量への差分として積み直す。
			ApplyImpulse_Internal(*BodyA, *BodyB, Frame.PositionA,
			                      {static_cast<Toolbox::f32>(Frame.Axis.X * Joint.AccumulatedImpulse),
			                       static_cast<Toolbox::f32>(Frame.Axis.Y * Joint.AccumulatedImpulse)});
		}
	}
	// 単一Distance Jointの速度拘束を解く。Hard両側拘束なので0クランプしない。
	void SolveDistanceJoint_Internal(Toolbox::size_t JointSlot)
	{
		if (JointSlot >= Joints.Size())
		{
			return;
		}
		FJointRecord2D& Joint = Joints[JointSlot];
		if (!Joint.bAlive)
		{
			return;
		}
		FBodyRecord2D* BodyA = Find_Internal(Joint.BodyA);
		FBodyRecord2D* BodyB = Find_Internal(Joint.BodyB);
		if (BodyA == nullptr || BodyB == nullptr)
		{
			return;
		}
		FJointFrame2D Frame;
		if (!BuildJointFrame_Internal(Joint, *BodyA, *BodyB, Frame))
		{
			return;
		}
		const Toolbox::f64 InverseMassA = EffectiveInverseMass_Internal(*BodyA);
		const Toolbox::f64 InverseMassB = EffectiveInverseMass_Internal(*BodyB);
		const Toolbox::f64 InverseInertiaA = EffectiveInverseInertia_Internal(*BodyA);
		const Toolbox::f64 InverseInertiaB = EffectiveInverseInertia_Internal(*BodyB);
		// 腕と軸の外積。接触点と同じ形の有効質量を作る。
		const Toolbox::f64 ArmAX = Toolbox::f64(Frame.ArmA.X);
		const Toolbox::f64 ArmAY = Toolbox::f64(Frame.ArmA.Y);
		const Toolbox::f64 ArmBX = Toolbox::f64(Frame.ArmB.X);
		const Toolbox::f64 ArmBY = Toolbox::f64(Frame.ArmB.Y);
		const Toolbox::f64 AxisX = Toolbox::f64(Frame.Axis.X);
		const Toolbox::f64 AxisY = Toolbox::f64(Frame.Axis.Y);
		const Toolbox::f64 CrossA = ArmAX * AxisY - ArmAY * AxisX;
		const Toolbox::f64 CrossB = ArmBX * AxisY - ArmBY * AxisX;
		const Toolbox::f64 Mass = InverseMassA + InverseMassB + InverseInertiaA * CrossA * CrossA +
		                          InverseInertiaB * CrossB * CrossB;
		// 有効質量が0以下なら両側無限質量、またはSpring無効なので何もしない。
		if (Mass <= 0 || !Toolbox::IsFinite(Mass))
		{
			return;
		}
		// RelativeVelocity = vA - vB（既存Contactと同じ規約）。
		const Toolbox::f64 Relative = EffectiveProjectionVelocity_Internal(Frame, *BodyA, *BodyB, AxisX, AxisY);
		// A→B軸なのでCdot = -dot(Relative, n)。CdotはAnchor間隔の増減率。
		const Toolbox::f64 CDot = -Relative;
		// BodyA += P / BodyB -= P なので dCdot = -lambda * K。
		// Cdotを0にするlambdaは lambda = Cdot / K。
		// 両側拘束（張る・押す）なので0クランプしない。
		const Toolbox::f64 Lambda = CDot / Mass;
		const Toolbox::f64 Old = Joint.AccumulatedImpulse;
		Joint.AccumulatedImpulse = Old + Lambda;
		const Toolbox::f64 Difference = Joint.AccumulatedImpulse - Old;
		ApplyImpulse_Internal(*BodyA, *BodyB, Frame.PositionA,
		                      {static_cast<Toolbox::f32>(AxisX * Difference), static_cast<Toolbox::f32>(AxisY * Difference)});
	}
	// Anchor点の相対速度（vA - vB）を返す。Contactと同じω×r規約を使う。
	static Toolbox::FVector2 AnchorRelativeVelocity_Internal(const FJointFrame2D& Frame, const FBodyRecord2D& BodyA,
	                                                         const FBodyRecord2D& BodyB) noexcept
	{
		// ω × r = {-ω*r.y, ω*r.x}。既存ContactのPoint速度と同じ形。
		const Toolbox::f64 VelocityAX = Toolbox::f64(BodyA.Velocity.X) -
		                                 Toolbox::f64(BodyA.AngularVelocity) * Toolbox::f64(Frame.ArmA.Y);
		const Toolbox::f64 VelocityAY = Toolbox::f64(BodyA.Velocity.Y) +
		                                 Toolbox::f64(BodyA.AngularVelocity) * Toolbox::f64(Frame.ArmA.X);
		const Toolbox::f64 VelocityBX = Toolbox::f64(BodyB.Velocity.X) -
		                                 Toolbox::f64(BodyB.AngularVelocity) * Toolbox::f64(Frame.ArmB.Y);
		const Toolbox::f64 VelocityBY = Toolbox::f64(BodyB.Velocity.Y) +
		                                 Toolbox::f64(BodyB.AngularVelocity) * Toolbox::f64(Frame.ArmB.X);
		return {static_cast<Toolbox::f32>(VelocityAX - VelocityBX), static_cast<Toolbox::f32>(VelocityAY - VelocityBY)};
	}
	// Anchor点速度をA→B軸へ射影した値を返す。速度拘束と起床判定で共有。
	static Toolbox::f64 EffectiveProjectionVelocity_Internal(const FJointFrame2D& Frame, const FBodyRecord2D& BodyA,
	                                                         const FBodyRecord2D& BodyB, Toolbox::f64 AxisX,
	                                                         Toolbox::f64 AxisY) noexcept
	{
		const Toolbox::FVector2 Relative = AnchorRelativeVelocity_Internal(Frame, BodyA, BodyB);
		return Toolbox::f64(Relative.X) * AxisX + Toolbox::f64(Relative.Y) * AxisY;
	}
	// 単一Distance Jointの位置誤差を線形補正で解く。
	void CorrectDistanceJointPositions_Internal() noexcept
	{
		for (Toolbox::size_t Slot = 0; Slot < Joints.Size(); ++Slot)
		{
			FJointRecord2D& Joint = Joints[Slot];
			if (!Joint.bAlive)
			{
				continue;
			}
			FBodyRecord2D* BodyA = Find_Internal(Joint.BodyA);
			FBodyRecord2D* BodyB = Find_Internal(Joint.BodyB);
			if (BodyA == nullptr || BodyB == nullptr)
			{
				continue;
			}
			// 姿勢を変えるとAnchorとAxisが動くため、反復ごとに作り直す。
			for (Toolbox::uint32 Pass = 0; Pass < JointPositionIterations; ++Pass)
			{
				FJointFrame2D Frame;
				if (!BuildJointFrame_Internal(Joint, *BodyA, *BodyB, Frame))
				{
					break;
				}
				// 誤差0または縮退軸では位置補正しない。縮退軸は速度側で処理済み。
				if (Frame.Error == 0 || !Toolbox::IsFinite(Frame.Error))
				{
					break;
				}
				CorrectDistanceJointPositionPass_Internal(*BodyA, *BodyB, Frame);
			}
		}
	}
	// 1反復分の位置補正。Frameは呼び出し側で構築した現在値。
	static void CorrectDistanceJointPositionPass_Internal(FBodyRecord2D& BodyA, FBodyRecord2D& BodyB,
	                                                     const FJointFrame2D& Frame) noexcept
	{
		{
			const Toolbox::f64 InverseMassA = EffectiveInverseMass_Internal(BodyA);
			const Toolbox::f64 InverseMassB = EffectiveInverseMass_Internal(BodyB);
			const Toolbox::f64 InverseInertiaA = EffectiveInverseInertia_Internal(BodyA);
			const Toolbox::f64 InverseInertiaB = EffectiveInverseInertia_Internal(BodyB);
			const Toolbox::f64 AxisX = Toolbox::f64(Frame.Axis.X);
			const Toolbox::f64 AxisY = Toolbox::f64(Frame.Axis.Y);
			// C = Distance - Length。正なら伸びている。
			// 軸はA→Bなので、縮めるにはAを+n、Bを-nへ動かす。
			// 補正量は一方向へclipし、誤差を跨いで反対へ跳ね返さない。
			Toolbox::f64 Correction = JointPositionBeta * Frame.Error;
			Correction = Toolbox::Clamp(Correction, -JointMaxPositionCorrection, JointMaxPositionCorrection);
			// 位置補正の有効質量。位置段階の逆質量は速度段階と同じ規約。
			const Toolbox::f64 ArmAX = Toolbox::f64(Frame.ArmA.X);
			const Toolbox::f64 ArmAY = Toolbox::f64(Frame.ArmA.Y);
			const Toolbox::f64 ArmBX = Toolbox::f64(Frame.ArmB.X);
			const Toolbox::f64 ArmBY = Toolbox::f64(Frame.ArmB.Y);
			const Toolbox::f64 CrossA = ArmAX * AxisY - ArmAY * AxisX;
			const Toolbox::f64 CrossB = ArmBX * AxisY - ArmBY * AxisX;
			const Toolbox::f64 Mass = InverseMassA + InverseMassB + InverseInertiaA * CrossA * CrossA +
			                          InverseInertiaB * CrossB * CrossB;
			if (Mass <= 0 || !Toolbox::IsFinite(Mass))
			{
				return;
			}
			// Correctionを各Bodyへ逆質量比で分配する。A/B両方を動かす。
			const Toolbox::f64 TotalInverse = InverseMassA + InverseMassB;
			// 一方が無限質量（=0）なら他方が全量を受け持つ。
			Toolbox::f64 WeightA = 0;
			Toolbox::f64 WeightB = 0;
			if (TotalInverse > 0)
			{
				WeightA = InverseMassA / TotalInverse;
				WeightB = InverseMassB / TotalInverse;
			}
			// 速度拘束と同じ規約。Aは+n、Bは-nへ動かす。
			const Toolbox::f64 MoveX = AxisX * Correction;
			const Toolbox::f64 MoveY = AxisY * Correction;
			BodyA.Position += {static_cast<Toolbox::f32>(MoveX * WeightA), static_cast<Toolbox::f32>(MoveY * WeightA)};
			BodyB.Position += {static_cast<Toolbox::f32>(-MoveX * WeightB), static_cast<Toolbox::f32>(-MoveY * WeightB)};
			// 姿勢補正は行わない。off-centerの角成分は速度拘束が担当する。
			// 位置段階で姿勢まで動かすと、回転後のAnchorが動いた状態で
			// 次の反復が古い軸で計算し、誤差が拡大する（実測で確認）。
		}
	}
	// 拘束参照列の速度拘束を1反復内でContact→DistanceJoint順に解く。
	// 反復ごとに両種類を交互に解くため、Jointを最後にまとめて解かない。
	void SolveConstraints_Internal(Toolbox::TVector<FManifold2D>& Manifolds,
	                               const Toolbox::TVector<PhysicsPrivate::FPhysicsConstraintRef>& Constraints)
	{
		for (Toolbox::uint32 Iteration = 0; Iteration < Contact.VelocityIterations; ++Iteration)
		{
			for (Toolbox::size_t Slot = 0; Slot < Constraints.Size(); ++Slot)
			{
				const PhysicsPrivate::FPhysicsConstraintRef& Constraint = Constraints[Slot];
				// Island Managerが(Kind, Index)順に整列済みなので、種別で分岐するだけで固定順になる。
				if (Constraint.Kind == PhysicsPrivate::EPhysicsConstraintKind::DistanceJoint)
				{
					SolveDistanceJoint_Internal(Constraint.Index);
					continue;
				}
				FManifold2D& Manifold = Manifolds[Constraint.Index];
				FBodyRecord2D* BodyA = Find_Internal(Manifold.BodyA);
				FBodyRecord2D* BodyB = Find_Internal(Manifold.BodyB);
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
	// 多様体列と生存JointからIslandを構築する。
	// BodyIndicesはDynamic Bodyのみを保持するため、起床伝播と休止判定の
	// 対象列としてそのまま利用できる。Static/Kinematicは含まれない。
	void BuildIslands_Internal(const Toolbox::TVector<FManifold2D>& Manifolds,
	                           Toolbox::TVector<PhysicsPrivate::FPhysicsIsland>& Out)
	{
		Toolbox::TVector<PhysicsPrivate::FIslandEdge> Edges;
		Edges.Reserve(Manifolds.Size() + Joints.Size());
		// Contact辺。Manifoldの安定添字をそのまま拘束参照へ入れる。
		for (Toolbox::size_t Index = 0; Index < Manifolds.Size(); ++Index)
		{
			const FManifold2D& Manifold = Manifolds[Index];
			const FBodyRecord2D* BodyA = Find_Internal(Manifold.BodyA);
			const FBodyRecord2D* BodyB = Find_Internal(Manifold.BodyB);
			if (BodyA == nullptr || BodyB == nullptr)
			{
				continue;
			}
			PhysicsPrivate::FIslandEdge Edge;
			Edge.BodyA = Manifold.BodyA.Index;
			Edge.BodyB = Manifold.BodyB.Index;
			Edge.Constraint.Kind = PhysicsPrivate::EPhysicsConstraintKind::Contact;
			Edge.Constraint.Index = Index;
			Edge.bDynamicA = BodyA->Type == EBodyType::Dynamic;
			Edge.bDynamicB = BodyB->Type == EBodyType::Dynamic;
			Edges.PushBack(Edge);
		}
		// Joint辺。添字は再利用で変わらないWorld Joint slotを使う。
		// 死亡済みslotはIslandへ入れず、順序もslot昇順で投入する。
		for (Toolbox::size_t Slot = 0; Slot < Joints.Size(); ++Slot)
		{
			const FJointRecord2D& Joint = Joints[Slot];
			if (!Joint.bAlive)
			{
				continue;
			}
			const FBodyRecord2D* BodyA = Find_Internal(Joint.BodyA);
			const FBodyRecord2D* BodyB = Find_Internal(Joint.BodyB);
			if (BodyA == nullptr || BodyB == nullptr)
			{
				continue;
			}
			PhysicsPrivate::FIslandEdge Edge;
			Edge.BodyA = Joint.BodyA.Index;
			Edge.BodyB = Joint.BodyB.Index;
			Edge.Constraint.Kind = PhysicsPrivate::EPhysicsConstraintKind::DistanceJoint;
			Edge.Constraint.Index = Slot;
			Edge.bDynamicA = BodyA->Type == EBodyType::Dynamic;
			Edge.bDynamicB = BodyB->Type == EBodyType::Dynamic;
			Edges.PushBack(Edge);
		}
		PhysicsPrivate::FIslandManager::Build(Slots.Size(), Edges, Out);
	}
	// 構築済みのIsland列を逐次で解く。
	void SolveIslands_Serial_Internal(Toolbox::TVector<FManifold2D>& Manifolds,
	                                 const Toolbox::TVector<PhysicsPrivate::FPhysicsIsland>& Islands)
	{
		for (Toolbox::size_t IslandIndex = 0; IslandIndex < Islands.Size(); ++IslandIndex)
		{
			SolveConstraints_Internal(Manifolds, Islands[IslandIndex].Constraints);
		}
	}
	// 構築済みのIsland列を並列で解く。
	void SolveIslands_Parallel_Internal(Toolbox::TVector<FManifold2D>& Manifolds,
	                                    const Toolbox::TVector<PhysicsPrivate::FPhysicsIsland>& Islands)
	{
		if (Islands.IsEmpty())
		{
			return;
		}
		auto SolveOne = [&](Toolbox::size_t IslandIndex)
		{
			SolveConstraints_Internal(Manifolds, Islands[IslandIndex].Constraints);
		};
		if (!Toolbox::ParallelFor(*Execution.JobSystem, Islands.Size(), SolveOne, 1))
		{
			throw Toolbox::FException("Parallel 2D island solver failed");
		}
		ExecutionDiagnostics.SolverIslandCount += Islands.Size();
	}
	// 構築済みのIsland列で拘束を解く。並列可否をExecution設定で選ぶ。
	// IslandはDynamic Bodyを共有せず、Static/Kinematicへは書き込まない。
	void SolveIslands_Internal(Toolbox::TVector<FManifold2D>& Manifolds,
	                           const Toolbox::TVector<PhysicsPrivate::FPhysicsIsland>& Islands)
	{
		if (UseBorrowedJobs_Internal(Execution.bParallelIslandSolver))
		{
			SolveIslands_Parallel_Internal(Manifolds, Islands);
		}
		else
		{
			SolveIslands_Serial_Internal(Manifolds, Islands);
		}
	}
	// 解決結果を再利用記録へ保存する。
	void StoreCache_Internal(const Toolbox::TVector<FManifold2D>& Manifolds)
	{
		// どの多様体にも属さない組の記録は捨てる。分離後の再接触へ
		// 古いImpulseを持ち越さない。特徴点の出入りでは捨てない。
		for (Toolbox::size_t CacheIndex = 0; CacheIndex < Cache.Size();)
		{
			const FCachedImpulse2D& Cached = Cache[CacheIndex];
			bool bActive = false;
			for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size() && !bActive; ++ManifoldIndex)
			{
				const FManifold2D& Manifold = Manifolds[ManifoldIndex];
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
			const FManifold2D& Manifold = Manifolds[ManifoldIndex];
			const FBodyRecord2D* BodyA = Find_Internal(Manifold.BodyA);
			const FBodyRecord2D* BodyB = Find_Internal(Manifold.BodyB);
			if (BodyA == nullptr || BodyB == nullptr)
			{
				continue;
			}
			for (Toolbox::size_t PointIndex = 0; PointIndex < Manifold.Points.Size(); ++PointIndex)
			{
				const FSolvePoint2D& Point = Manifold.Points[PointIndex];
				bool bStored = false;
				for (Toolbox::size_t CacheIndex = 0; CacheIndex < Cache.Size(); ++CacheIndex)
				{
					FCachedImpulse2D& Cached = Cache[CacheIndex];
					const bool bSamePair =
					    Cached.ColliderA == Manifold.ColliderA && Cached.ColliderB == Manifold.ColliderB;
					if (bSamePair && Cached.FeatureId == Point.FeatureId)
					{
						Cached.BodyGenerationA = BodyA->Generation;
						Cached.BodyGenerationB = BodyB->Generation;
						Cached.Normal = Point.Normal;
						Cached.NormalImpulse = Point.NormalImpulse;
						Cached.TangentImpulse = Point.TangentImpulse;
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
					FCachedImpulse2D Cached;
					Cached.ColliderA = Manifold.ColliderA;
					Cached.ColliderB = Manifold.ColliderB;
					Cached.BodyGenerationA = BodyA->Generation;
					Cached.BodyGenerationB = BodyB->Generation;
					Cached.FeatureId = Point.FeatureId;
					Cached.Normal = Point.Normal;
					Cached.NormalImpulse = Point.NormalImpulse;
					Cached.TangentImpulse = Point.TangentImpulse;
					Cache.PushBack(Cached);
				}
			}
		}
	}
	// Jointの起床に必要な距離閾値。速度次元のSleep判定が1固定区間で
	// 移動できる距離を使い、超える誤差なら拘束解決を必要とする。
	// 軸判定の縮退閾値を下限にして、非有限や0除算を避ける。
	Toolbox::f64 JointWakeDistance_Internal(Toolbox::f64 Slice) const noexcept
	{
		const Toolbox::f64 Travel = Toolbox::f64(Sleep.LinearSpeedLimit) * Slice;
		return Travel > JointAxisEpsilon ? Travel : JointAxisEpsilon;
	}
	// Island内のDynamic Bodyを起床させる。Static/Kinematicは起こさない。
	// Island ManagerのBodyIndicesはDynamicのみなのでそのまま使える。
	void WakeIslandDynamics_Internal(const PhysicsPrivate::FPhysicsIsland& Island) noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Island.BodyIndices.Size(); ++Index)
		{
			const Toolbox::size_t Slot = Island.BodyIndices[Index];
			if (Slot >= Slots.Size())
			{
				continue;
			}
			FBodyRecord2D& Record = Slots[Slot];
			if (!Record.bAlive || Record.Type != EBodyType::Dynamic)
			{
				continue;
			}
			Wake_Internal(Record);
		}
	}
	// Island内に既に起きているDynamicがあるか。
	bool HasAwakeDynamic_Internal(const PhysicsPrivate::FPhysicsIsland& Island) const noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Island.BodyIndices.Size(); ++Index)
		{
			const Toolbox::size_t Slot = Island.BodyIndices[Index];
			if (Slot >= Slots.Size())
			{
				continue;
			}
			const FBodyRecord2D& Record = Slots[Slot];
			if (Record.bAlive && Record.Type == EBodyType::Dynamic && !Record.bSleeping)
			{
				return true;
			}
		}
		return false;
	}
	// Island内に休止中のDynamicがあるか。
	bool HasSleepingDynamic_Internal(const PhysicsPrivate::FPhysicsIsland& Island) const noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Island.BodyIndices.Size(); ++Index)
		{
			const Toolbox::size_t Slot = Island.BodyIndices[Index];
			if (Slot >= Slots.Size())
			{
				continue;
			}
			const FBodyRecord2D& Record = Slots[Slot];
			if (Record.bAlive && Record.Type == EBodyType::Dynamic && Record.bSleeping)
			{
				return true;
			}
		}
		return false;
	}
	// Island内のJointが起床閾値を超える距離誤差を持つか。
	bool HasJointErrorBeyond_Internal(const PhysicsPrivate::FPhysicsIsland& Island, Toolbox::f64 WakeDistance) const noexcept
	{
		for (Toolbox::size_t Slot = 0; Slot < Island.Constraints.Size(); ++Slot)
		{
			const PhysicsPrivate::FPhysicsConstraintRef& Constraint = Island.Constraints[Slot];
			if (Constraint.Kind != PhysicsPrivate::EPhysicsConstraintKind::DistanceJoint || Constraint.Index >= Joints.Size())
			{
				continue;
			}
			const FJointRecord2D& Joint = Joints[Constraint.Index];
			if (!Joint.bAlive)
			{
				continue;
			}
			const FBodyRecord2D* BodyA = Find_Internal(Joint.BodyA);
			const FBodyRecord2D* BodyB = Find_Internal(Joint.BodyB);
			if (BodyA == nullptr || BodyB == nullptr)
			{
				continue;
			}
			FJointFrame2D Frame;
			if (!BuildJointFrame_Internal(Joint, *BodyA, *BodyB, Frame))
			{
				continue;
			}
			const Toolbox::f64 Error = Frame.Error < 0 ? -Frame.Error : Frame.Error;
			if (Error > WakeDistance)
			{
				return true;
			}
		}
		return false;
	}
	// 拘束の運動が起床理由になるか。Contactは既存の接触点相対運動、
	// JointはAnchor全相対速度と距離誤差を使う。
	bool ShouldWakeForConstraintMotion_Internal(const Toolbox::TVector<FManifold2D>& Manifolds,
	                                            const PhysicsPrivate::FPhysicsIsland& Island, Toolbox::f64 Slice) const noexcept
	{
		const Toolbox::f64 WakeDistance = JointWakeDistance_Internal(Slice);
		for (Toolbox::size_t Slot = 0; Slot < Island.Constraints.Size(); ++Slot)
		{
			const PhysicsPrivate::FPhysicsConstraintRef& Constraint = Island.Constraints[Slot];
			if (Constraint.Kind == PhysicsPrivate::EPhysicsConstraintKind::Contact)
			{
				if (Constraint.Index >= Manifolds.Size())
				{
					continue;
				}
				const FManifold2D& Manifold = Manifolds[Constraint.Index];
				const FBodyRecord2D* BodyA = Find_Internal(Manifold.BodyA);
				const FBodyRecord2D* BodyB = Find_Internal(Manifold.BodyB);
				if (BodyA == nullptr || BodyB == nullptr || (BodyA->bSleeping && BodyB->bSleeping))
				{
					continue;
				}
				for (Toolbox::size_t PointIndex = 0; PointIndex < Manifold.Points.Size(); ++PointIndex)
				{
					if (ShouldWakeForMotion_Internal(*BodyA, *BodyB, Manifold.Points[PointIndex].Position))
					{
						return true;
					}
				}
				continue;
			}
			// DistanceJoint。Solverと同じ解決規則でslotを解決する。
			if (Constraint.Index >= Joints.Size())
			{
				continue;
			}
			const FJointRecord2D& Joint = Joints[Constraint.Index];
			if (!Joint.bAlive)
			{
				continue;
			}
			const FBodyRecord2D* BodyA = Find_Internal(Joint.BodyA);
			const FBodyRecord2D* BodyB = Find_Internal(Joint.BodyB);
			if (BodyA == nullptr || BodyB == nullptr || (!BodyA->bSleeping && !BodyB->bSleeping))
			{
				continue;
			}
			FJointFrame2D Frame;
			if (!BuildJointFrame_Internal(Joint, *BodyA, *BodyB, Frame))
			{
				// 軸が確定できない退化ケースは方向を捏造せず起こさない。
				continue;
			}
			// Anchor全相対速度の大きさ。軸成分だけだと直交運動を見落とすため、
			// 既存Contactの接触点判定と同じ速度次元の基準を使う。
			const Toolbox::FVector2 Relative = AnchorRelativeVelocity_Internal(Frame, *BodyA, *BodyB);
			const Toolbox::f64 Speed = Toolbox::Sqrt(Toolbox::f64(Relative.X) * Toolbox::f64(Relative.X) +
			                                        Toolbox::f64(Relative.Y) * Toolbox::f64(Relative.Y));
			if (Speed > Toolbox::f64(Sleep.LinearSpeedLimit))
			{
				return true;
			}
			// 拘束誤差が閾値を超えるなら速度0でも拘束解決を必要とする。
			// sleeping Dynamicは逆質量0なので、起こさないと補正できない。
			const Toolbox::f64 Error = Frame.Error < 0 ? -Frame.Error : Frame.Error;
			if (Error > WakeDistance)
			{
				return true;
			}
		}
		return false;
	}
	// Island単位で起床させる。既 awake、拘束運動、Kinematic Anchor運動、
	// Joint誤差のいずれかがあればIsland全体のDynamicを起こす。
	// WakeUpやImpulse適用は対象Bodyだけを起こす既存契約で、
	// Islandへの伝播はここ（Main側）で一度に完了する。
	void WakeIslands_Internal(const Toolbox::TVector<FManifold2D>& Manifolds,
	                          const Toolbox::TVector<PhysicsPrivate::FPhysicsIsland>& Islands, Toolbox::f64 Slice) noexcept
	{
		for (Toolbox::size_t IslandIndex = 0; IslandIndex < Islands.Size(); ++IslandIndex)
		{
			const PhysicsPrivate::FPhysicsIsland& Island = Islands[IslandIndex];
			// 全て起きているなら何もしない。Wake_InternalはSleepTimerを0へ戻すため、
			// 起床伝播の条件と休止中の蓄積を打ち消してしまう。
			if (!HasSleepingDynamic_Internal(Island))
			{
				continue;
			}
			if (HasAwakeDynamic_Internal(Island) || ShouldWakeForConstraintMotion_Internal(Manifolds, Island, Slice))
			{
				WakeIslandDynamics_Internal(Island);
			}
		}
	}
	// Contact WarmStartが新規接触で起こした起床を、Island全体へもう一度伝播する。
	// WarmStartはMain側で走っているため、並列Islandの競合は発生しない。
	void PropagateAwakeDynamics_Internal(const Toolbox::TVector<PhysicsPrivate::FPhysicsIsland>& Islands) noexcept
	{
		for (Toolbox::size_t IslandIndex = 0; IslandIndex < Islands.Size(); ++IslandIndex)
		{
			const PhysicsPrivate::FPhysicsIsland& Island = Islands[IslandIndex];
			// 全て起きている島は何もしない。SleepTimerの蓄積を打ち消さないため。
			if (HasSleepingDynamic_Internal(Island) && HasAwakeDynamic_Internal(Island))
			{
				WakeIslandDynamics_Internal(Island);
			}
		}
	}
	// IslandをSleep単位として評価する。bTouchedは「休止判定で支持となる
	// constraint（ContactまたはJoint）へ参加している」ことを表す。
	// SensorはSolver拘束ではないので支持に数えない。
	void UpdateSleep_Internal(const Toolbox::TVector<FManifold2D>& Manifolds,
	                          const Toolbox::TVector<PhysicsPrivate::FPhysicsIsland>& Islands, Toolbox::f64 Slice) noexcept
	{
		if (!Sleep.bEnabled)
		{
			return;
		}
		const Toolbox::f64 WakeDistance = JointWakeDistance_Internal(Slice);
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			Slots[Index].bTouched = false;
		}
		// Islandの拘束から支持を付ける。ContactとJointの両方が参加扱いです。
		for (Toolbox::size_t IslandIndex = 0; IslandIndex < Islands.Size(); ++IslandIndex)
		{
			const PhysicsPrivate::FPhysicsIsland& Island = Islands[IslandIndex];
			for (Toolbox::size_t ConstraintSlot = 0; ConstraintSlot < Island.Constraints.Size(); ++ConstraintSlot)
			{
				const PhysicsPrivate::FPhysicsConstraintRef& Constraint = Island.Constraints[ConstraintSlot];
				if (Constraint.Kind == PhysicsPrivate::EPhysicsConstraintKind::DistanceJoint)
				{
					if (Constraint.Index >= Joints.Size())
					{
						continue;
					}
					const FJointRecord2D& Joint = Joints[Constraint.Index];
					if (!Joint.bAlive)
					{
						continue;
					}
					if (Joint.BodyA.Index < Slots.Size())
					{
						Slots[Joint.BodyA.Index].bTouched = true;
					}
					if (Joint.BodyB.Index < Slots.Size())
					{
						Slots[Joint.BodyB.Index].bTouched = true;
					}
					continue;
				}
				if (Constraint.Index >= Manifolds.Size())
				{
					continue;
				}
				const FManifold2D& Manifold = Manifolds[Constraint.Index];
				if (Manifold.BodyA.Index < Slots.Size())
				{
					Slots[Manifold.BodyA.Index].bTouched = true;
				}
				if (Manifold.BodyB.Index < Slots.Size())
				{
					Slots[Manifold.BodyB.Index].bTouched = true;
				}
			}
		}
		// IslandをSleep単位として評価する。一つでも条件を外れたらIsland全体を止める。
		for (Toolbox::size_t IslandIndex = 0; IslandIndex < Islands.Size(); ++IslandIndex)
		{
			const PhysicsPrivate::FPhysicsIsland& Island = Islands[IslandIndex];
			if (Island.BodyIndices.IsEmpty())
			{
				continue;
			}
			bool bCanSleep = true;
			for (Toolbox::size_t Index = 0; Index < Island.BodyIndices.Size(); ++Index)
			{
				const Toolbox::size_t Slot = Island.BodyIndices[Index];
				if (Slot >= Slots.Size())
				{
					bCanSleep = false;
					break;
				}
				const FBodyRecord2D& Record = Slots[Slot];
				// 速度の大きさと回転の大きさ。
				const Toolbox::f64 Speed = Toolbox::Sqrt(Toolbox::f64(Record.Velocity.X) * Toolbox::f64(Record.Velocity.X) +
				                                        Toolbox::f64(Record.Velocity.Y) * Toolbox::f64(Record.Velocity.Y));
				const Toolbox::f64 Spin =
				    Record.AngularVelocity < 0 ? -Toolbox::f64(Record.AngularVelocity) : Toolbox::f64(Record.AngularVelocity);
				// 休止禁止、拘束未参加、速度超過のいずれかならIsland全体を止める。
				// bAllowSleep=falseが1体でもあればAだけsleepさせることはしない。
				if (!Record.bAlive || Record.Type != EBodyType::Dynamic || !Record.bAllowSleep || !Record.bTouched ||
				    Speed > Toolbox::f64(Sleep.LinearSpeedLimit) || Spin > Toolbox::f64(Sleep.AngularSpeedLimit))
				{
					bCanSleep = false;
					break;
				}
			}
			// 大きな拘束誤差を抱えたまま速度0だからsleep、は禁止する。
			if (bCanSleep && HasJointErrorBeyond_Internal(Island, WakeDistance))
			{
				bCanSleep = false;
			}
			if (!bCanSleep)
			{
				for (Toolbox::size_t Index = 0; Index < Island.BodyIndices.Size(); ++Index)
				{
					const Toolbox::size_t Slot = Island.BodyIndices[Index];
					if (Slot < Slots.Size())
					{
						Slots[Slot].SleepTimer = 0;
					}
				}
				continue;
			}
			// 全Dynamicが候補なので同じSliceだけ進める。
			bool bAllReady = true;
			for (Toolbox::size_t Index = 0; Index < Island.BodyIndices.Size(); ++Index)
			{
				const Toolbox::size_t Slot = Island.BodyIndices[Index];
				if (Slot >= Slots.Size())
				{
					continue;
				}
				FBodyRecord2D& Record = Slots[Slot];
				Record.SleepTimer = static_cast<Toolbox::f32>(Toolbox::f64(Record.SleepTimer) + Slice);
				if (Toolbox::f64(Record.SleepTimer) < Toolbox::f64(Sleep.TimeoutSeconds))
				{
					bAllReady = false;
				}
			}
			if (!bAllReady)
			{
				continue;
			}
			// Island内のDynamicを同じStepで同時に休止させる。ズレを作らない。
			for (Toolbox::size_t Index = 0; Index < Island.BodyIndices.Size(); ++Index)
			{
				const Toolbox::size_t Slot = Island.BodyIndices[Index];
				if (Slot >= Slots.Size())
				{
					continue;
				}
				FBodyRecord2D& Record = Slots[Slot];
				Record.bSleeping = true;
				Record.Velocity = {};
				Record.AngularVelocity = 0;
			}
		}
		// Islandに属さないDynamicは支持を失ったとして起こす。
		// 拘束を破棄されて宙に浮いたsleeping Bodyが落下を再開する経路。
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			FBodyRecord2D& Record = Slots[Index];
			if (!Record.bAlive || Record.Type != EBodyType::Dynamic)
			{
				continue;
			}
			if (!Record.bTouched)
			{
				Record.SleepTimer = 0;
				Record.bSleeping = false;
			}
			Record.bTouched = false;
		}
	}
	// 許容幅を超える貫通を位置で補正する。運動エネルギーは注入しない。
	void CorrectPositions_Internal(const Toolbox::TVector<FManifold2D>& Manifolds) noexcept
	{
		for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
		{
			const FManifold2D& Manifold = Manifolds[ManifoldIndex];
			FBodyRecord2D* BodyA = Find_Internal(Manifold.BodyA);
			FBodyRecord2D* BodyB = Find_Internal(Manifold.BodyB);
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
				const FSolvePoint2D& Point = Manifold.Points[PointIndex];
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
				                    static_cast<Toolbox::f32>(Point.Normal.Y * Correction * WeightA)};
				BodyB->Position += {static_cast<Toolbox::f32>(-Point.Normal.X * Correction * WeightB),
				                    static_cast<Toolbox::f32>(-Point.Normal.Y * Correction * WeightB)};
			}
		}
	}
	// 移動区間の解決を行う剛体があるかを調べる。
	bool HasContinuousBody_Internal() const noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			const FBodyRecord2D& Record = Slots[Index];
			if (Record.bAlive && Record.Type == EBodyType::Dynamic && Record.bUseContinuous)
			{
				return true;
			}
		}
		return false;
	}
	// コライダー組の線形CCD対応を調べる。
	EContinuousSupport ClassifyPair_Internal(const FBodyRecord2D& BodyA, const FColliderRecord2D& RecordA,
	                                         const FBodyRecord2D& BodyB, const FColliderRecord2D& RecordB) const
	{
		const Toolbox::size_t IndexA = RecordA.Shape.Index();
		const Toolbox::size_t IndexB = RecordB.Shape.Index();
		// カプセルを含む組と矩形同士は線形CCDの対象外（離散の接触で解く）。
		if ((IndexA == 1 && IndexB == 1) || IndexA == 2 || IndexB == 2)
		{
			return EContinuousSupport::UnsupportedPair;
		}
		if (IndexA == 1 && !IsAxisAligned_Internal(ToWorld_Internal(BodyA, RecordA.Shape.Get<1>()).Angle))
		{
			return EContinuousSupport::UnsupportedRotation;
		}
		if (IndexB == 1 && !IsAxisAligned_Internal(ToWorld_Internal(BodyB, RecordB.Shape.Get<1>()).Angle))
		{
			return EContinuousSupport::UnsupportedRotation;
		}
		return EContinuousSupport::Supported;
	}
	// コライダーの移動境界を求める。
	FSweptBounds2D SweptBoundsOf_Internal(const FBodyRecord2D& Body, const FColliderRecord2D& Record,
	                                      Toolbox::FVector2 Displacement) const
	{
		if (Record.Shape.Index() == 0)
		{
			return SweptCircle_Internal(ToWorld_Internal(Body, Record.Shape.Get<0>()), Displacement);
		}
		if (Record.Shape.Index() == 2)
		{
			return SweptCapsule_Internal(ToWorld_Internal(Body, Record.Shape.Get<2>()), Displacement);
		}
		return SweptBox_Internal(ToWorld_Internal(Body, Record.Shape.Get<1>()), Displacement);
	}
	// 全剛体を位置だけ進める。力の積分は繰り返さない。休止中は動かさない。
	void AdvanceAll_Internal(Toolbox::f64 Slice) noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
		{
			FBodyRecord2D& Record = Slots[Index];
			if (!Record.bAlive || Record.bSleeping)
			{
				continue;
			}
			if (Record.Type == EBodyType::Dynamic)
			{
				IntegratePosition_Internal(Record, Slice);
			}
			else if (Record.Type == EBodyType::Kinematic)
			{
				IntegrateKinematic_Internal(Record, Slice);
			}
		}
	}
	// 世界箱を軸平行境界へ変換する。
	static Toolbox::FAABB2D ToBounds_Internal(const Toolbox::FOrientedBox2D& Box) noexcept
	{
		Toolbox::FVector2 U;
		Toolbox::FVector2 V;
		BoxAxes_Internal(Box, U, V);
		Toolbox::FAABB2D Bounds;
		bool bFirst = true;
		for (Toolbox::int32 Corner = 0; Corner < 4; ++Corner)
		{
			const Toolbox::f64 SignX = (Corner & 1) == 0 ? -1 : 1;
			const Toolbox::f64 SignY = (Corner & 2) == 0 ? -1 : 1;
			const Toolbox::f64 X = Toolbox::f64(Box.Center.X) + SignX * Box.HalfExtents.X * U.X + SignY * Box.HalfExtents.Y * V.X;
			const Toolbox::f64 Y = Toolbox::f64(Box.Center.Y) + SignX * Box.HalfExtents.X * U.Y + SignY * Box.HalfExtents.Y * V.Y;
			if (bFirst)
			{
				Bounds.Min = {static_cast<Toolbox::f32>(X), static_cast<Toolbox::f32>(Y)};
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
				if (X > Bounds.Max.X)
				{
					Bounds.Max.X = static_cast<Toolbox::f32>(X);
				}
				if (Y > Bounds.Max.Y)
				{
					Bounds.Max.Y = static_cast<Toolbox::f32>(Y);
				}
			}
		}
		return Bounds;
	}
	// 現在位置の拘束をその場で解く。時刻は進めない。
	void SolveNow_Internal()
	{
		Toolbox::TVector<FManifold2D> Manifolds;
		GenerateManifolds_Internal(Manifolds);
		for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
		{
			WarmStart_Internal(Manifolds[ManifoldIndex]);
		}
		// 時刻を進めないため起床伝播と休止更新は行わない。構築したIslandで解く。
		Toolbox::TVector<PhysicsPrivate::FPhysicsIsland> Islands;
		BuildIslands_Internal(Manifolds, Islands);
		SolveIslands_Internal(Manifolds, Islands);
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
				const FColliderRecord2D& RecordA = Colliders[First];
				if (!RecordA.bAlive)
				{
					continue;
				}
				const FBodyRecord2D* BodyA = Find_Internal(RecordA.Body);
				if (BodyA == nullptr)
				{
					continue;
				}
				for (Toolbox::size_t Second = First + 1; Second < Colliders.Size(); ++Second)
				{
					const FColliderRecord2D& RecordB = Colliders[Second];
					if (!RecordB.bAlive)
					{
						continue;
					}
					const FBodyRecord2D* BodyB = Find_Internal(RecordB.Body);
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
					// Sensorを含む組と、衝突フィルターが許さない組は連続衝突で止めない。
					if (!RespondsTogether_Internal(RecordA, RecordB))
					{
						continue;
					}
					// 残り時間の変位。
					const bool bMoverA = BodyA->Type == EBodyType::Dynamic && BodyA->bUseContinuous;
					const bool bMoverB = BodyB->Type == EBodyType::Dynamic && BodyB->bUseContinuous;
					Toolbox::FVector2 DisplacementA;
					Toolbox::FVector2 DisplacementB;
					if (BodyA->Type != EBodyType::Static)
					{
						DisplacementA = {static_cast<Toolbox::f32>(Toolbox::f64(BodyA->Velocity.X) * Remaining),
						                 static_cast<Toolbox::f32>(Toolbox::f64(BodyA->Velocity.Y) * Remaining)};
					}
					if (BodyB->Type != EBodyType::Static)
					{
						DisplacementB = {static_cast<Toolbox::f32>(Toolbox::f64(BodyB->Velocity.X) * Remaining),
						                 static_cast<Toolbox::f32>(Toolbox::f64(BodyB->Velocity.Y) * Remaining)};
					}
				// CCD対象自身が止まっていても相手の移動で交差するため、要求と相対運動を分けて判定する。
				const bool bRequireCCD = bMoverA || bMoverB;
				if (!bRequireCCD)
				{
					continue;
				}
				const Toolbox::f64 RelativeX = Toolbox::f64(DisplacementA.X) - DisplacementB.X;
				const Toolbox::f64 RelativeY = Toolbox::f64(DisplacementA.Y) - DisplacementB.Y;
				const Toolbox::f64 RelativeLength = Toolbox::Sqrt(RelativeX * RelativeX + RelativeY * RelativeY);
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
					// 正準順序のまま形状と変位を渡す。
					const Toolbox::size_t IndexA = RecordA.Shape.Index();
					const Toolbox::size_t IndexB = RecordB.Shape.Index();
					Toolbox::FSweepHit2D Hit;
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
						    (Toolbox::f64(BodyA->Velocity.Y) - BodyB->Velocity.Y) * Hit.Normal.Y;
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
// Dynamicの速度だけを更新する。位置は呼び出し元が進める。
static void IntegrateVelocity_Internal(FBodyRecord2D& Record, Toolbox::FVector2 Gravity, Toolbox::f64 StepSeconds)
{
	// 減衰後の速度へ加速度を足す半陰的Euler。
	const Toolbox::f64 DampLinear = 1.0 / (1.0 + static_cast<Toolbox::f64>(Record.LinearDamping) * StepSeconds);
	const Toolbox::f64 DampAngular = 1.0 / (1.0 + static_cast<Toolbox::f64>(Record.AngularDamping) * StepSeconds);
	// 速度の各成分。
	Toolbox::f64 VelocityX = static_cast<Toolbox::f64>(Record.Velocity.X) * DampLinear;
	Toolbox::f64 VelocityY = static_cast<Toolbox::f64>(Record.Velocity.Y) * DampLinear;
	// 重力と蓄積力による加速度。
	const Toolbox::f64 GravityX = static_cast<Toolbox::f64>(Gravity.X) * Record.GravityScale;
	const Toolbox::f64 GravityY = static_cast<Toolbox::f64>(Gravity.Y) * Record.GravityScale;
	const Toolbox::f64 ForceX = static_cast<Toolbox::f64>(Record.Force.X) * Record.InverseMass;
	const Toolbox::f64 ForceY = static_cast<Toolbox::f64>(Record.Force.Y) * Record.InverseMass;
	VelocityX += (GravityX + ForceX) * StepSeconds;
	VelocityY += (GravityY + ForceY) * StepSeconds;
	// 角速度。
	Toolbox::f64 Angular = static_cast<Toolbox::f64>(Record.AngularVelocity) * DampAngular;
	Angular += static_cast<Toolbox::f64>(Record.Torque) * Record.InverseInertia * StepSeconds;
	Record.Velocity = {static_cast<Toolbox::f32>(VelocityX), static_cast<Toolbox::f32>(VelocityY)};
	Record.AngularVelocity = static_cast<Toolbox::f32>(Angular);
}
// 更新後の速度で位置と姿勢を進める。
static void IntegratePosition_Internal(FBodyRecord2D& Record, Toolbox::f64 StepSeconds) noexcept
{
	Record.PrevVelocity = Record.Velocity;
	Record.PrevAngularVelocity = Record.AngularVelocity;
	const Toolbox::f32 MovedX = static_cast<Toolbox::f32>(Toolbox::f64(Record.Velocity.X) * StepSeconds);
	const Toolbox::f32 MovedY = static_cast<Toolbox::f32>(Toolbox::f64(Record.Velocity.Y) * StepSeconds);
	Record.Position += {MovedX, MovedY};
	// 指定角速度で姿勢を進める。
	const Toolbox::f64 Turned = static_cast<Toolbox::f64>(Record.AngularVelocity) * StepSeconds;
	Record.Angle = static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Angle) + Turned);
}
// 指定速度どおりに運動させる。外力と減衰は適用しない。
static void IntegrateKinematic_Internal(FBodyRecord2D& Record, Toolbox::f64 StepSeconds) noexcept
{
	Record.PrevVelocity = Record.Velocity;
	Record.PrevAngularVelocity = Record.AngularVelocity;
	Record.Position += {static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Velocity.X) * StepSeconds),
	                    static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Velocity.Y) * StepSeconds)};
	// 指定角速度で姿勢を進める。
	const Toolbox::f64 Turned = static_cast<Toolbox::f64>(Record.AngularVelocity) * StepSeconds;
	Record.Angle = static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.Angle) + Turned);
}
// 取り付ける形状を検査する。不正な値は例外。
static void ValidateColliderShape_Internal(const decltype(FColliderDescription2D::Shape)& Shape)
{
	if (Shape.Index() == 0)
	{
		if (!Toolbox::IsValid(Shape.Get<0>()))
		{
			throw Toolbox::FException("Invalid 2D circle collider");
		}
	}
	else if (Shape.Index() == 1)
	{
		if (!Toolbox::IsValid(Shape.Get<1>()))
		{
			throw Toolbox::FException("Invalid 2D box collider");
		}
	}
	else if (!Toolbox::IsValid(Shape.Get<2>()))
	{
		throw Toolbox::FException("Invalid 2D capsule collider");
	}
}
// 問い合わせ形状の中心（円は中心、カプセルは中心線の中点）。
static Toolbox::FVector2 QueryCenter_Internal(const Toolbox::FCircle2D& Shape) noexcept
{
	return Shape.Center;
}
static Toolbox::FVector2 QueryCenter_Internal(const Toolbox::FCapsule2D& Shape) noexcept
{
	return Toolbox::CapsuleCenter(Shape);
}
// 問い合わせ形状を中心から覆う距離（円は半径、カプセルは中心線の半分の長さと半径の和）。
static Toolbox::f64 QueryReach_Internal(const Toolbox::FCircle2D& Shape) noexcept
{
	return Shape.Radius;
}
static Toolbox::f64 QueryReach_Internal(const Toolbox::FCapsule2D& Shape) noexcept
{
	const Toolbox::f64 X = Toolbox::f64(Shape.End.X) - Shape.Start.X;
	const Toolbox::f64 Y = Toolbox::f64(Shape.End.Y) - Shape.Start.Y;
	// 中点の丸め分の余裕を足す。
	const Toolbox::f64 Half = Toolbox::Sqrt(X * X + Y * Y) * 0.5;
	return Half + Shape.Radius + Half * 1e-6;
}
// 範囲の形状と対象の重なり（接触を含む）。
template <typename TShape> static bool AreaOverlaps_Internal(const Toolbox::FCircle2D& Area, const TShape& Shape)
{
	return Toolbox::Intersects(Area, Shape, 0.0f);
}
template <typename TShape> static bool AreaOverlaps_Internal(const Toolbox::FCapsule2D& Area, const TShape& Shape)
{
	return Toolbox::FindShapeContact(Area, Shape).Separation <= 0;
}
// 移動量がf32で表現できるかを検査する。
static void RequireRepresentableMove_Internal(Toolbox::FVector2 Start, Toolbox::FVector2 End)
{
	if (Toolbox::Abs(Toolbox::f64(End.X) - Start.X) > Toolbox::f64(Toolbox::TNumericLimits<Toolbox::f32>::Max()) ||
	    Toolbox::Abs(Toolbox::f64(End.Y) - Start.Y) > Toolbox::f64(Toolbox::TNumericLimits<Toolbox::f32>::Max()))
	{
		throw Toolbox::FException("Unrepresentable 2D world sweep movement");
	}
}
FPhysicsWorld2D::FPhysicsWorld2D() : m_pImpl(Toolbox::MakeUnique<FImpl>())
{
}
FPhysicsWorld2D::~FPhysicsWorld2D() = default;
FBodyId2D FPhysicsWorld2D::CreateBody(const FBodyDescription2D& Description)
{
	if (!Description.Position.IsValid() || !Description.Velocity.IsValid())
	{
		throw Toolbox::FException("Invalid 2D body position or velocity");
	}
	if (!Toolbox::IsFinite(Description.Angle) || !Toolbox::IsFinite(Description.AngularVelocity))
	{
		throw Toolbox::FException("Invalid 2D body angle or angular velocity");
	}
	if (!Toolbox::IsFinite(Description.LinearDamping) || Description.LinearDamping < 0)
	{
		throw Toolbox::FException("Invalid 2D body linear damping");
	}
	if (!Toolbox::IsFinite(Description.AngularDamping) || Description.AngularDamping < 0)
	{
		throw Toolbox::FException("Invalid 2D body angular damping");
	}
	if (!Toolbox::IsFinite(Description.GravityScale))
	{
		throw Toolbox::FException("Invalid 2D body gravity scale");
	}
	// 新しい登録の初期状態。
	FBodyRecord2D Record;
	Record.Type = Description.Type;
	Record.Position = Description.Position;
	Record.Angle = Description.Angle;
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
			throw Toolbox::FException("Invalid 2D body mass");
		}
		if (!Toolbox::IsFinite(Description.Inertia) || Description.Inertia <= 0)
		{
			throw Toolbox::FException("Invalid 2D body inertia");
		}
		Record.InverseMass = 1.0f / Description.Mass;
		Record.InverseInertia = 1.0f / Description.Inertia;
	}
	else
	{
		Record.InverseMass = 0;
		Record.InverseInertia = 0;
	}
	// 問い合わせ索引のBody一覧を先に予約する（失敗しても状態は変わらない）。
	m_pImpl->QueryIndex.ReserveBodies(m_pImpl->Slots.Size() + 1);
	if (m_pImpl->IndexedPoses.Size() < m_pImpl->Slots.Size() + 1)
	{
		m_pImpl->IndexedPoses.Resize(
		    Toolbox::Max<Toolbox::size_t>(m_pImpl->Slots.Size() + 1, m_pImpl->IndexedPoses.Size() * 2));
	}
	// 空きスロットの再使用または末尾への追加。
	Toolbox::size_t Index = 0;
	if (!m_pImpl->Free.IsEmpty())
	{
		Index = m_pImpl->Free.Back();
		m_pImpl->Free.PopBack();
		FBodyRecord2D& Slot = m_pImpl->Slots[Index];
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
	m_pImpl->QueryIndex.ResetBody(Index);
	m_pImpl->IndexedPoses[Index] = {};
	return {m_pImpl->World, Index, m_pImpl->Slots[Index].Generation};
}
bool FPhysicsWorld2D::DestroyBody(FBodyId2D Id) noexcept
{
	FBodyRecord2D* Record = m_pImpl->Find_Internal(Id);
	if (Record == nullptr)
	{
		return false;
	}
	Record->bAlive = false;
	Record->Generation += 1;
	Record->Force = {};
	Record->Torque = 0;
	m_pImpl->Free.PushBack(Id.Index);
	// このBodyに接続される距離拘束は残さない（利用者にDestroyJointの順序を要求しない）。
	for (Toolbox::size_t Index = 0; Index < m_pImpl->Joints.Size(); ++Index)
	{
		FJointRecord2D& Joint = m_pImpl->Joints[Index];
		if (!Joint.bAlive || (Joint.BodyA != Id && Joint.BodyB != Id))
		{
			continue;
		}
		Joint.bAlive = false;
		Joint.Generation += 1;
		m_pImpl->JointFree.PushBack(Index);
	}
	// 取り付け済みのコライダーも、スロット昇順で失効させて索引から外す（Bodyごとの一覧をたどる）。
	Toolbox::int32 Current = m_pImpl->QueryIndex.GetFirstCollider(Id.Index);
	while (Current != PhysicsPrivate::TQueryIndex<2>::None)
	{
		const Toolbox::size_t Index = static_cast<Toolbox::size_t>(Current);
		Current = m_pImpl->QueryIndex.GetNextCollider(Index);
		FColliderRecord2D& Collider = m_pImpl->Colliders[Index];
		Collider.bAlive = false;
		Collider.Generation += 1;
		m_pImpl->ColliderFree.PushBack(Index);
		m_pImpl->QueryIndex.Detach(Index);
		--m_pImpl->AliveColliders;
	}
	// 古い接触記録を使い回さない。
	m_pImpl->Cache.Clear();
	return true;
}
bool FPhysicsWorld2D::IsAlive(FBodyId2D Id) const noexcept
{
	return m_pImpl->Find_Internal(Id) != nullptr;
}
FJointId2D FPhysicsWorld2D::CreateDistanceJoint(FBodyId2D BodyA, FBodyId2D BodyB,
                                               const FDistanceJointDescription2D& Description)
{
	const FBodyRecord2D* RecordA = m_pImpl->Find_Internal(BodyA);
	const FBodyRecord2D* RecordB = m_pImpl->Find_Internal(BodyB);
	if (RecordA == nullptr || RecordB == nullptr)
	{
		throw Toolbox::FException("Invalid 2D joint body");
	}
	if (BodyA == BodyB)
	{
		throw Toolbox::FException("A 2D joint needs two different bodies");
	}
	if (RecordA->Type != EBodyType::Dynamic && RecordB->Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("A 2D joint needs at least one dynamic body");
	}
	if (!Description.LocalAnchorA.IsValid() || !Description.LocalAnchorB.IsValid() ||
	    !Toolbox::IsFinite(Description.Length) || Description.Length < 0)
	{
		throw Toolbox::FException("Invalid 2D distance joint");
	}
	FJointRecord2D Record;
	Record.bAlive = true;
	Record.BodyA = BodyA;
	Record.BodyB = BodyB;
	Record.LocalAnchorA = Description.LocalAnchorA;
	Record.LocalAnchorB = Description.LocalAnchorB;
	Record.Length = Description.Length;
	// Solver cacheは明示的に初期化する。slot再利用で前のJointのImpulseや
	// 保存軸が混ざらないことを保証する。
	Record.AccumulatedImpulse = 0;
	Record.LastValidAxis = {1, 0};
	Record.bHasLastValidAxis = false;
	// 空きスロットの再使用または末尾への追加。破棄時に進めた世代を引き継ぐ。
	Toolbox::size_t Index = 0;
	if (!m_pImpl->JointFree.IsEmpty())
	{
		Index = m_pImpl->JointFree.Back();
		m_pImpl->JointFree.PopBack();
		FJointRecord2D& Slot = m_pImpl->Joints[Index];
		const Toolbox::uint64 NextGeneration = Slot.Generation + 1;
		Slot = Record;
		Slot.Generation = NextGeneration;
	}
	else
	{
		Index = m_pImpl->Joints.Size();
		Record.Generation = 1;
		m_pImpl->Joints.PushBack(Record);
	}
	return {m_pImpl->World, Index, m_pImpl->Joints[Index].Generation};
}
bool FPhysicsWorld2D::DestroyJoint(FJointId2D Id) noexcept
{
	FJointRecord2D* Record = m_pImpl->FindJoint_Internal(Id);
	if (Record == nullptr)
	{
		return false;
	}
	Record->bAlive = false;
	Record->Generation += 1;
	m_pImpl->JointFree.PushBack(Id.Index);
	return true;
}
bool FPhysicsWorld2D::IsJointAlive(FJointId2D Id) const noexcept
{
	return m_pImpl->FindJoint_Internal(Id) != nullptr;
}
FDistanceJointState2D FPhysicsWorld2D::GetDistanceJoint(FJointId2D Id) const
{
	const FJointRecord2D& Record = m_pImpl->ResolveJoint_Internal(Id);
	const FBodyRecord2D& BodyA = m_pImpl->Resolve_Internal(Record.BodyA);
	const FBodyRecord2D& BodyB = m_pImpl->Resolve_Internal(Record.BodyB);
	const Toolbox::FVector2 AnchorA = m_pImpl->AnchorWorld_Internal(BodyA, Record.LocalAnchorA);
	const Toolbox::FVector2 AnchorB = m_pImpl->AnchorWorld_Internal(BodyB, Record.LocalAnchorB);
	const Toolbox::f64 Dx = Toolbox::f64(AnchorB.X) - Toolbox::f64(AnchorA.X);
	const Toolbox::f64 Dy = Toolbox::f64(AnchorB.Y) - Toolbox::f64(AnchorA.Y);
	FDistanceJointState2D State;
	State.CurrentLength = Toolbox::Sqrt(Dx * Dx + Dy * Dy);
	State.Error = State.CurrentLength - Record.Length;
	return State;
}
Toolbox::FVector2 FPhysicsWorld2D::GetPosition(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Position;
}
Toolbox::f32 FPhysicsWorld2D::GetAngle(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Angle;
}
Toolbox::FVector2 FPhysicsWorld2D::GetVelocity(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Velocity;
}
Toolbox::f32 FPhysicsWorld2D::GetAngularVelocity(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).AngularVelocity;
}
Toolbox::f32 FPhysicsWorld2D::GetAngularMomentum(FBodyId2D Id) const
{
	// 非Dynamicの運動量は追跡しない。
	const FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		return 0;
	}
	return Record.AngularVelocity / Record.InverseInertia;
}
void FPhysicsWorld2D::SetVelocity(FBodyId2D Id, Toolbox::FVector2 Velocity)
{
	if (!Velocity.IsValid())
	{
		throw Toolbox::FException("Invalid 2D body velocity");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type == EBodyType::Static)
	{
		throw Toolbox::FException("Static 2D body has no velocity");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.Velocity = Velocity;
}
void FPhysicsWorld2D::SetAngularVelocity(FBodyId2D Id, Toolbox::f32 AngularVelocity)
{
	if (!Toolbox::IsFinite(AngularVelocity))
	{
		throw Toolbox::FException("Invalid 2D body angular velocity");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type == EBodyType::Static)
	{
		throw Toolbox::FException("Static 2D body has no angular velocity");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.AngularVelocity = AngularVelocity;
}
void FPhysicsWorld2D::ApplyForce(FBodyId2D Id, Toolbox::FVector2 Force)
{
	if (!Force.IsValid())
	{
		throw Toolbox::FException("Invalid 2D body force");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 2D bodies accept forces");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.Force += Force;
}
void FPhysicsWorld2D::ApplyTorque(FBodyId2D Id, Toolbox::f32 Torque)
{
	if (!Toolbox::IsFinite(Torque))
	{
		throw Toolbox::FException("Invalid 2D body torque");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 2D bodies accept torques");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.Torque += Torque;
}
void FPhysicsWorld2D::ApplyLinearImpulse(FBodyId2D Id, Toolbox::FVector2 Impulse)
{
	if (!Impulse.IsValid())
	{
		throw Toolbox::FException("Invalid 2D body impulse");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 2D bodies accept impulses");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	// 力積は速度へ即時反映し、分割数に依存しない。
	Record.Velocity += {Impulse.X * Record.InverseMass, Impulse.Y * Record.InverseMass};
}
void FPhysicsWorld2D::ApplyAngularImpulse(FBodyId2D Id, Toolbox::f32 Impulse)
{
	if (!Toolbox::IsFinite(Impulse))
	{
		throw Toolbox::FException("Invalid 2D body angular impulse");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 2D bodies accept impulses");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.AngularVelocity += Impulse * Record.InverseInertia;
}
void FPhysicsWorld2D::ApplyImpulseAtPoint(FBodyId2D Id, Toolbox::FVector2 Impulse, Toolbox::FVector2 WorldPoint)
{
	if (!Impulse.IsValid() || !WorldPoint.IsValid())
	{
		throw Toolbox::FException("Invalid 2D body impulse point");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (Record.Type != EBodyType::Dynamic)
	{
		throw Toolbox::FException("Only dynamic 2D bodies accept impulses");
	}
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	// 重心からの腕と力積の外積が回転を生む。
	const Toolbox::f64 ArmX = static_cast<Toolbox::f64>(WorldPoint.X) - Record.Position.X;
	const Toolbox::f64 ArmY = static_cast<Toolbox::f64>(WorldPoint.Y) - Record.Position.Y;
	Record.Velocity += {Impulse.X * Record.InverseMass, Impulse.Y * Record.InverseMass};
	Record.AngularVelocity = static_cast<Toolbox::f32>(static_cast<Toolbox::f64>(Record.AngularVelocity) +
	                                                   (ArmX * Impulse.Y - ArmY * Impulse.X) * Record.InverseInertia);
}
void FPhysicsWorld2D::SetGravity(Toolbox::FVector2 Gravity)
{
	if (!Gravity.IsValid())
	{
		throw Toolbox::FException("Invalid 2D gravity");
	}
	// 重力の変化は休止島へ影響するため起こす。同じ値は起こさない。
	if (!(m_pImpl->Gravity == Gravity))
	{
		m_pImpl->Gravity = Gravity;
		m_pImpl->WakeAll_Internal();
	}
}
Toolbox::FVector2 FPhysicsWorld2D::GetGravity() const noexcept
{
	return m_pImpl->Gravity;
}
FColliderId2D FPhysicsWorld2D::AttachCollider(FBodyId2D Body, const FColliderDescription2D& Description)
{
	FBodyRecord2D& Target = m_pImpl->Resolve_Internal(Body);
	ValidateColliderShape_Internal(Description.Shape);
	if (!Toolbox::IsFinite(Description.Friction) || Description.Friction < 0)
	{
		throw Toolbox::FException("Invalid 2D collider friction");
	}
	if (!Toolbox::IsFinite(Description.Restitution) || Description.Restitution < 0 || Description.Restitution > 1)
	{
		throw Toolbox::FException("Invalid 2D collider restitution");
	}
	if (Description.Response != EColliderResponse::Solid && Description.Response != EColliderResponse::Sensor)
	{
		throw Toolbox::FException("Invalid 2D collider response");
	}
	// 新しい登録の初期状態。
	FColliderRecord2D Record;
	Record.Body = Body;
	Record.Shape = Description.Shape;
	Record.Friction = Description.Friction;
	Record.Restitution = Description.Restitution;
	Record.QueryCategory = Description.QueryCategory;
	Record.Response = Description.Response;
	Record.Collision = Description.Collision;
	// 現在の姿勢での索引用の境界と、索引の領域を先に用意する（失敗しても状態は変わらない）。
	decltype(FColliderRecord2D::Shape) QueryWorld;
	const auto QueryBounds = FImpl::ColliderQueryBounds_Internal(Target, Record, QueryWorld);
	m_pImpl->QueryIndex.ReserveColliders(m_pImpl->Colliders.Size() + 1, m_pImpl->AliveColliders + 1);
	if (m_pImpl->QueryWorldShapes.Size() < m_pImpl->Colliders.Size() + 1)
	{
		m_pImpl->QueryWorldShapes.Resize(
		    Toolbox::Max<Toolbox::size_t>(m_pImpl->Colliders.Size() + 1, m_pImpl->QueryWorldShapes.Size() * 2));
	}
	// イベント境界列も登録時に確保し、次のStepへ確保を持ち越さない。
	if (m_pImpl->Events.IsEnabled())
	{
		m_pImpl->EventEntries.Reserve(m_pImpl->QueryWorldShapes.Size());
	}
	// 空きスロットの再使用または末尾への追加。
	Toolbox::size_t Index = 0;
	if (!m_pImpl->ColliderFree.IsEmpty())
	{
		Index = m_pImpl->ColliderFree.Back();
		m_pImpl->ColliderFree.PopBack();
		FColliderRecord2D& Slot = m_pImpl->Colliders[Index];
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
	m_pImpl->QueryWorldShapes[Index] = QueryWorld;
	m_pImpl->QueryIndex.Attach(Index, Body.Index, QueryBounds);
	++m_pImpl->AliveColliders;
	return {Body, Index, m_pImpl->Colliders[Index].Generation};
}
bool FPhysicsWorld2D::DetachCollider(FColliderId2D Id) noexcept
{
	FColliderRecord2D* Record = m_pImpl->FindCollider_Internal(Id);
	if (Record == nullptr)
	{
		return false;
	}
	Record->bAlive = false;
	Record->Generation += 1;
	m_pImpl->ColliderFree.PushBack(Id.Index);
	m_pImpl->QueryIndex.Detach(Id.Index);
	--m_pImpl->AliveColliders;
	// 古い接触記録を使い回さない。
	m_pImpl->Cache.Clear();
	return true;
}
void FPhysicsWorld2D::SetColliderShape(FColliderId2D Id, const decltype(FColliderDescription2D::Shape)& Shape)
{
	// 状態・ID・形状の検査がすべて成功してから書き換える。
	(void)m_pImpl->ResolveQueryCollider_Internal(Id);
	ValidateColliderShape_Internal(Shape);
	FColliderRecord2D& Record = m_pImpl->Colliders[Id.Index];
	FBodyRecord2D& Body = m_pImpl->Resolve_Internal(Record.Body);
	Record.Shape = Shape;
	// 索引へ反映した姿勢のまま、この形状の境界と問い合わせ用のWorld形状だけを合わせ直す。
	m_pImpl->QueryIndex.Refresh(Id.Index,
	                            FImpl::ColliderQueryBounds_Internal(Body, Record, m_pImpl->QueryWorldShapes[Id.Index]));
	// 古い接触の記録を使い回さず、支えが変わる剛体を起こす。
	m_pImpl->Cache.Clear();
	if (Body.Type == EBodyType::Dynamic)
	{
		Body.bSleeping = false;
		Body.SleepTimer = 0;
	}
}
decltype(FColliderDescription2D::Shape) FPhysicsWorld2D::GetColliderShape(FColliderId2D Id) const
{
	const FColliderRecord2D* Record = m_pImpl->FindCollider_Internal(Id);
	if (Record == nullptr)
	{
		throw Toolbox::FException("Invalid 2D collider id");
	}
	return Record->Shape;
}
void FPhysicsWorld2D::SetContactSettings(const FContactSettings2D& Settings)
{
	if (!Toolbox::IsFinite(Settings.ContactSlop) || Settings.ContactSlop < 0)
	{
		throw Toolbox::FException("Invalid 2D contact slop");
	}
	if (!Toolbox::IsFinite(Settings.BaumgarteBeta) || Settings.BaumgarteBeta < 0 || Settings.BaumgarteBeta > 1)
	{
		throw Toolbox::FException("Invalid 2D contact beta");
	}
	if (!Toolbox::IsFinite(Settings.MaxCorrection) || Settings.MaxCorrection <= 0)
	{
		throw Toolbox::FException("Invalid 2D contact correction");
	}
	if (!Toolbox::IsFinite(Settings.RestitutionThreshold) || Settings.RestitutionThreshold < 0)
	{
		throw Toolbox::FException("Invalid 2D restitution threshold");
	}
	if (Settings.VelocityIterations < 1 || Settings.VelocityIterations > 64)
	{
		throw Toolbox::FException("Invalid 2D solver iterations");
	}
	m_pImpl->Contact = Settings;
}
FContactSettings2D FPhysicsWorld2D::GetContactSettings() const noexcept
{
	return m_pImpl->Contact;
}
void FPhysicsWorld2D::SetContinuousSettings(const FContinuousSettings2D& Settings)
{
	if (Settings.MaxIterations < 1 || Settings.MaxIterations > 32)
	{
		throw Toolbox::FException("Invalid 2D continuous iterations");
	}
	if (!Toolbox::IsFinite(Settings.MinAdvanceSeconds) || Settings.MinAdvanceSeconds < 0)
	{
		throw Toolbox::FException("Invalid 2D continuous progress");
	}
	m_pImpl->Continuous = Settings;
}
FContinuousSettings2D FPhysicsWorld2D::GetContinuousSettings() const noexcept
{
	return m_pImpl->Continuous;
}
void FPhysicsWorld2D::SetContinuous(FBodyId2D Id, bool bEnabled)
{
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	Record.bUseContinuous = bEnabled;
}
bool FPhysicsWorld2D::IsContinuous(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).bUseContinuous;
}
EContinuousSupport FPhysicsWorld2D::QueryContinuousSupport(FColliderId2D A, FColliderId2D B) const
{
	const FColliderRecord2D* RecordA = m_pImpl->FindCollider_Internal(A);
	const FColliderRecord2D* RecordB = m_pImpl->FindCollider_Internal(B);
	if (RecordA == nullptr || RecordB == nullptr)
	{
		throw Toolbox::FException("Invalid 2D collider id");
	}
	const FBodyRecord2D* BodyA = m_pImpl->Find_Internal(RecordA->Body);
	const FBodyRecord2D* BodyB = m_pImpl->Find_Internal(RecordB->Body);
	if (BodyA == nullptr || BodyB == nullptr)
	{
		throw Toolbox::FException("Invalid 2D collider body");
	}
	return m_pImpl->ClassifyPair_Internal(*BodyA, *RecordA, *BodyB, *RecordB);
}
FContinuousDiagnostics2D FPhysicsWorld2D::GetContinuousDiagnostics() const noexcept
{
	return m_pImpl->Diagnostics;
}
// Step内部で借用するJob Systemと並列化の指定を変更する。
// @param Settings Step中だけ使う並列実行設定。
void FPhysicsWorld2D::SetExecutionSettings(const FPhysicsExecutionSettings& Settings) noexcept
{
	m_pImpl->Execution = Settings;
}
// Step内部で使う並列実行設定を返す。
FPhysicsExecutionSettings FPhysicsWorld2D::GetExecutionSettings() const noexcept
{
	return m_pImpl->Execution;
}
// 直近更新の並列実行診断を返す。
FPhysicsExecutionDiagnostics FPhysicsWorld2D::GetExecutionDiagnostics() const noexcept
{
	return m_pImpl->ExecutionDiagnostics;
}
void FPhysicsWorld2D::SetSleepSettings(const FSleepSettings2D& Settings)
{
	if (!Toolbox::IsFinite(Settings.TimeoutSeconds) || Settings.TimeoutSeconds <= 0)
	{
		throw Toolbox::FException("Invalid 2D sleep timeout");
	}
	if (!Toolbox::IsFinite(Settings.LinearSpeedLimit) || Settings.LinearSpeedLimit < 0)
	{
		throw Toolbox::FException("Invalid 2D sleep speed");
	}
	if (!Toolbox::IsFinite(Settings.AngularSpeedLimit) || Settings.AngularSpeedLimit < 0)
	{
		throw Toolbox::FException("Invalid 2D sleep spin");
	}
	// 無効化で凍結した剛体を残さない。
	const bool bWasEnabled = m_pImpl->Sleep.bEnabled;
	m_pImpl->Sleep = Settings;
	if (bWasEnabled && !Settings.bEnabled)
	{
		m_pImpl->WakeAll_Internal();
	}
}
FSleepSettings2D FPhysicsWorld2D::GetSleepSettings() const noexcept
{
	return m_pImpl->Sleep;
}
bool FPhysicsWorld2D::IsSleeping(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).bSleeping;
}
bool FPhysicsWorld2D::WakeUp(FBodyId2D Id) noexcept
{
	FBodyRecord2D* Record = m_pImpl->Find_Internal(Id);
	if (Record == nullptr || Record->Type != EBodyType::Dynamic)
	{
		return false;
	}
	Record->bSleeping = false;
	Record->SleepTimer = 0;
	return true;
}
void FPhysicsWorld2D::SetBodyTransform(FBodyId2D Id, Toolbox::FVector2 Position, Toolbox::f32 Angle)
{
	if (!Position.IsValid() || !Toolbox::IsFinite(Angle))
	{
		throw Toolbox::FException("Invalid 2D body transform");
	}
	FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	// 外力は休止中の剛体を起こす。
	Record.bSleeping = false;
	Record.SleepTimer = 0;
	Record.Position = Position;
	Record.Angle = Angle;
	// Stepを待たず、次の問い合わせへ新しい姿勢を反映する。
	m_pImpl->RefreshBodyColliders_Internal(Id.Index);
}
void FPhysicsWorld2D::ClearContactCache() noexcept
{
	m_pImpl->Cache.Clear();
}
bool FPhysicsWorld2D::IsColliderAlive(FColliderId2D Id) const noexcept
{
	return m_pImpl->FindCollider_Internal(Id) != nullptr;
}
// 登録配列から直接採取する。外部の観察登録一覧は使用しない。
// 全ビットのフィルターへ委譲する。走査と交点計算は4引数版だけに置く。
Toolbox::TOptional<FWorldSegmentHit2D> FPhysicsWorld2D::RaycastClosest(Toolbox::FVector2 Start, Toolbox::FVector2 End,
                                                                       Toolbox::TOptional<FBodyId2D> ExcludedBody) const
{
	return RaycastClosest(Start, End, ExcludedBody, FWorldQueryFilter{});
}
// 索引（または総当たり）で候補を絞り、状態を変更せず、対象カテゴリの中で最短の交差を返す。
Toolbox::TOptional<FWorldSegmentHit2D> FPhysicsWorld2D::RaycastClosest(Toolbox::FVector2 Start, Toolbox::FVector2 End,
                                                                       Toolbox::TOptional<FBodyId2D> ExcludedBody,
                                                                       const FWorldQueryFilter& Filter) const
{
	// 空Worldでも入力を先に検査する。既存円交差の変位表現に合わせる。
	if (!Start.IsValid() || !End.IsValid() || Start == End || !(End - Start).IsValid())
	{
		throw Toolbox::FException("Invalid or unrepresentable 2D world query segment");
	}
	// 読み取り専用の内部状態。
	const FImpl& Impl = *m_pImpl;
	Impl.RequireQueryState_Internal();
	if (ExcludedBody)
	{
		(void)Impl.Resolve_Internal(*ExcludedBody);
	}
	// 候補を絞る線分（f64）と、問い合わせの座標の規模。
	const Toolbox::f64 StartXY[2] = {Start.X, Start.Y};
	const Toolbox::f64 EndXY[2] = {End.X, End.Y};
	const Toolbox::f64 QueryMaxAbs = Toolbox::Max(Toolbox::Max(Toolbox::Abs(StartXY[0]), Toolbox::Abs(StartXY[1])),
	                                              Toolbox::Max(Toolbox::Abs(EndXY[0]), Toolbox::Abs(EndXY[1])));
	PhysicsPrivate::TQuerySegment<2> Segment;
	Segment.Radius = PhysicsPrivate::QueryInflation_Internal(Impl.QueryIndex, QueryMaxAbs);
	for (Toolbox::int32 Axis = 0; Axis < 2; ++Axis)
	{
		Segment.Start[Axis] = StartXY[Axis];
		Segment.Delta[Axis] = EndXY[Axis] - StartXY[Axis];
		Segment.Bounds.Min[Axis] = Toolbox::Min(StartXY[Axis], EndXY[Axis]) - Segment.Radius;
		Segment.Bounds.Max[Axis] = Toolbox::Max(StartXY[Axis], EndXY[Axis]) + Segment.Radius;
	}
	// 最短候補。候補0でも後続形状の計算は省略しない。
	Toolbox::TOptional<FWorldSegmentHit2D> Best;
	auto Consider = [&](Toolbox::size_t Index, bool bIndexed)
	{
		// 現在のColliderスロットと所有Bodyの位置・角度。
		const auto& Record = Impl.Colliders[Index];
		// 既存の形状変換（Body角度＋Collider角度）と有限線分交差による割合。
		const auto Hit = Impl.QueryWorldShape_Internal(Index, bIndexed)
		                     .Visit(
		                         [&](const auto& WorldShape)
		                         {
			                         return Toolbox::IntersectSegment(Start, End, WorldShape);
		                         });
		if (!Hit)
		{
			return;
		}
		if (!Toolbox::IsFinite(*Hit) || *Hit < 0 || *Hit > 1)
		{
			throw Toolbox::FException("Invalid 2D world query fraction");
		}
		if (!PhysicsPrivate::IsCloserHit_Internal(Best.HasValue(), Best ? Best->Fraction : 0,
		                                          Best ? Best->Collider.Index : 0, *Hit, Index))
		{
			return;
		}
		// 最短候補として保持する非所有の値。
		FWorldSegmentHit2D Result;
		Result.Collider = {Record.Body, Index, Record.Generation};
		Result.Fraction = *Hit;
		// f32の差が大きい場合も、交点を倍精度の凸結合から作る。
		const Toolbox::f64 X = (1 - *Hit) * Start.X + *Hit * End.X;
		const Toolbox::f64 Y = (1 - *Hit) * Start.Y + *Hit * End.Y;
		if (!Toolbox::IsFinite(X) || !Toolbox::IsFinite(Y) ||
		    Toolbox::Abs(X) > Toolbox::TNumericLimits<Toolbox::f32>::Max() ||
		    Toolbox::Abs(Y) > Toolbox::TNumericLimits<Toolbox::f32>::Max())
		{
			throw Toolbox::FException("Unrepresentable 2D world query point");
		}
		Result.Position = {static_cast<Toolbox::f32>(X), static_cast<Toolbox::f32>(Y)};
		Best = Result;
	};
	PhysicsPrivate::FQueryVisitCounters Visit;
	Toolbox::uint64 NarrowTests = 0;
	const bool bFallback = PhysicsPrivate::VisitQueryCandidates_Internal(
	    Impl.QuerySource_Internal(), QueryMaxAbs, Filter, ExcludedBody,
	    [&](const PhysicsPrivate::TQueryBounds<2>& Bounds)
	    {
		    return PhysicsPrivate::SegmentOverlaps_Internal(Bounds, Segment);
	    },
	    Consider, Visit, NarrowTests);
	PhysicsPrivate::CommitQueryCounters_Internal(Impl.bQueryDiagnostics, Impl.QueryTotals.Raycast, Visit, NarrowTests,
	                                             bFallback);
	return Best;
}
// 半径0かつ移動ありはRaycastClosestへ委譲し、それ以外は索引（または総当たり）で候補を絞って最初の接触を返す。
Toolbox::TOptional<FWorldSweepHit2D> FPhysicsWorld2D::SweepClosest(const Toolbox::FCircle2D& StartShape,
                                                                   Toolbox::FVector2 EndCenter,
                                                                   Toolbox::TOptional<FBodyId2D> ExcludedBody,
                                                                   const FWorldQueryFilter& Filter) const
{
	// 空Worldやマスク0でも、形状・終点・f32で表現できる移動量を先に検査する。差はf64で求める。
	if (!Toolbox::IsValid(StartShape) || !EndCenter.IsValid())
	{
		throw Toolbox::FException("Invalid 2D world sweep shape or end center");
	}
	const Toolbox::f64 XStart = StartShape.Center.X;
	const Toolbox::f64 XEnd = EndCenter.X;
	const Toolbox::f64 YStart = StartShape.Center.Y;
	const Toolbox::f64 YEnd = EndCenter.Y;
	if (Toolbox::Abs(XEnd - XStart) > Toolbox::f64(Toolbox::TNumericLimits<Toolbox::f32>::Max()) ||
	    Toolbox::Abs(YEnd - YStart) > Toolbox::f64(Toolbox::TNumericLimits<Toolbox::f32>::Max()))
	{
		throw Toolbox::FException("Unrepresentable 2D world sweep movement");
	}
	if (StartShape.Radius == 0 && !(StartShape.Center == EndCenter))
	{
		// 同じ条件の線分問い合わせと同じ結果にする。走査は一度だけで、結果型だけを変える。
		const auto Ray = RaycastClosest(StartShape.Center, EndCenter, ExcludedBody, Filter);
		if (!Ray)
		{
			return {};
		}
		FWorldSweepHit2D Result;
		Result.Collider = Ray->Collider;
		Result.Fraction = Ray->Fraction;
		Result.CenterAtHit = Ray->Position;
		Result.bInitialContact = Ray->Fraction == 0;
		return Result;
	}
	return SweepColliders_Internal(StartShape, EndCenter, ExcludedBody, Filter, false);
}
// カプセルの移動。形状・終点・移動量を検査してから走査する。
Toolbox::TOptional<FWorldSweepHit2D> FPhysicsWorld2D::SweepCapsuleClosest(const Toolbox::FCapsule2D& StartShape,
                                                                          Toolbox::FVector2 EndCenter,
                                                                          Toolbox::TOptional<FBodyId2D> ExcludedBody,
                                                                          const FWorldQueryFilter& Filter) const
{
	if (!Toolbox::IsValid(StartShape) || !EndCenter.IsValid())
	{
		throw Toolbox::FException("Invalid 2D world sweep shape or end center");
	}
	RequireRepresentableMove_Internal(QueryCenter_Internal(StartShape), EndCenter);
	return SweepColliders_Internal(StartShape, EndCenter, ExcludedBody, Filter, false);
}
// SweepClosestの走査部分。bSkipInitialContactsなら開始時に接触しているColliderを候補から除く。
template <typename TShape>
Toolbox::TOptional<FWorldSweepHit2D> FPhysicsWorld2D::SweepColliders_Internal(
    const TShape& StartShape, Toolbox::FVector2 EndCenter, const Toolbox::TOptional<FBodyId2D>& ExcludedBody,
    const FWorldQueryFilter& Filter, bool bSkipInitialContacts) const
{
	// 移動する形状の中心（円は中心、カプセルは中心線の中点）と、中心から形状を覆う距離。
	const Toolbox::FVector2 StartCenter = QueryCenter_Internal(StartShape);
	const Toolbox::f64 Reach = QueryReach_Internal(StartShape);
	const Toolbox::f64 XStart = StartCenter.X;
	const Toolbox::f64 XEnd = EndCenter.X;
	const Toolbox::f64 YStart = StartCenter.Y;
	const Toolbox::f64 YEnd = EndCenter.Y;
	// 読み取り専用の内部状態。
	const FImpl& Impl = *m_pImpl;
	Impl.RequireQueryState_Internal();
	if (ExcludedBody)
	{
		(void)Impl.Resolve_Internal(*ExcludedBody);
	}
	// 候補を絞る、半径で広げた線分（f64）と、問い合わせの座標の規模。
	const Toolbox::f64 StartXY[2] = {XStart, YStart};
	const Toolbox::f64 EndXY[2] = {XEnd, YEnd};
	const Toolbox::f64 QueryMaxAbs = Toolbox::Max(Toolbox::Max(Toolbox::Abs(XStart), Toolbox::Abs(YStart)),
	                                              Toolbox::Max(Toolbox::Abs(XEnd), Toolbox::Abs(YEnd))) +
	                                 Reach;
	PhysicsPrivate::TQuerySegment<2> Segment;
	Segment.Radius = Reach + PhysicsPrivate::QueryInflation_Internal(Impl.QueryIndex, QueryMaxAbs);
	for (Toolbox::int32 Axis = 0; Axis < 2; ++Axis)
	{
		Segment.Start[Axis] = StartXY[Axis];
		Segment.Delta[Axis] = EndXY[Axis] - StartXY[Axis];
		Segment.Bounds.Min[Axis] = Toolbox::Min(StartXY[Axis], EndXY[Axis]) - Segment.Radius;
		Segment.Bounds.Max[Axis] = Toolbox::Max(StartXY[Axis], EndXY[Axis]) + Segment.Radius;
	}
	// 最短候補。割合0の候補があっても後続の対象は計算する。
	Toolbox::TOptional<FWorldSweepHit2D> Best;
	auto Consider = [&](Toolbox::size_t Index, bool bIndexed)
	{
		// 既存の形状変換で現在の姿勢へ移し、許容距離0の移動判定を行う。
		const auto& Record = Impl.Colliders[Index];
		const auto Hit = Impl.QueryWorldShape_Internal(Index, bIndexed)
		                     .Visit(
		                         [&](const auto& WorldShape)
		                         {
			                         return Toolbox::SweepToCenter(StartShape, EndCenter, WorldShape);
		                         });
		if (!Hit)
		{
			return;
		}
		// 開始時に接触しているColliderを除く問い合わせでは、初期接触を候補にしない。
		if (bSkipInitialContacts && Hit->bInitialContact)
		{
			return;
		}
		if (!Toolbox::IsFinite(Hit->Time) || Hit->Time < 0 || Hit->Time > 1)
		{
			throw Toolbox::FException("Invalid 2D world sweep fraction");
		}
		if (!PhysicsPrivate::IsCloserHit_Internal(Best.HasValue(), Best ? Best->Fraction : 0,
		                                          Best ? Best->Collider.Index : 0, Hit->Time, Index))
		{
			return;
		}
		// 接触時の中心を倍精度の凸結合から作る。
		const Toolbox::f64 XAt = (1 - Hit->Time) * XStart + Hit->Time * XEnd;
		const Toolbox::f64 YAt = (1 - Hit->Time) * YStart + Hit->Time * YEnd;
		if (!Toolbox::IsFinite(XAt) || Toolbox::Abs(XAt) > Toolbox::f64(Toolbox::TNumericLimits<Toolbox::f32>::Max()) ||
		    !Toolbox::IsFinite(YAt) || Toolbox::Abs(YAt) > Toolbox::f64(Toolbox::TNumericLimits<Toolbox::f32>::Max()))
		{
			throw Toolbox::FException("Unrepresentable 2D world sweep center");
		}
		FWorldSweepHit2D Result;
		Result.Collider = {Record.Body, Index, Record.Generation};
		Result.Fraction = Hit->Time;
		Result.CenterAtHit = {static_cast<Toolbox::f32>(XAt), static_cast<Toolbox::f32>(YAt)};
		Result.bInitialContact = Hit->bInitialContact;
		// 法線は形状計算がf64の相対値から求めた方向をそのまま使う（f32のCenterAtHitから引き直さない）。
		Result.Normal = Hit->Normal;
		Best = Result;
	};
	PhysicsPrivate::FQueryVisitCounters Visit;
	Toolbox::uint64 NarrowTests = 0;
	const bool bFallback = PhysicsPrivate::VisitQueryCandidates_Internal(
	    Impl.QuerySource_Internal(), QueryMaxAbs, Filter, ExcludedBody,
	    [&](const PhysicsPrivate::TQueryBounds<2>& Bounds)
	    {
		    return PhysicsPrivate::SegmentOverlaps_Internal(Bounds, Segment);
	    },
	    Consider, Visit, NarrowTests);
	PhysicsPrivate::CommitQueryCounters_Internal(Impl.bQueryDiagnostics, Impl.QueryTotals.Sweep, Visit, NarrowTests,
	                                             bFallback);
	return Best;
}
// 開始時に接触しているColliderを除いて、移動中に最初に接触するColliderを返す。
Toolbox::TOptional<FWorldSweepHit2D> FPhysicsWorld2D::SweepClosestIgnoringInitialContacts(
    const Toolbox::FCircle2D& StartShape, Toolbox::FVector2 EndCenter, Toolbox::TOptional<FBodyId2D> ExcludedBody,
    const FWorldQueryFilter& Filter) const
{
	// 半径は正（点の問い合わせはRaycastClosestの規則になるため提供しない）。その他の検査はSweepClosestと同じ。
	if (!(StartShape.Center.IsValid() && Toolbox::IsFinite(StartShape.Radius) && StartShape.Radius > 0) ||
	    !EndCenter.IsValid())
	{
		throw Toolbox::FException("Invalid 2D world sweep shape or end center");
	}
	Toolbox::f64 Start[3] = {StartShape.Center.X, StartShape.Center.Y, 0};
	Toolbox::f64 End[3] = {EndCenter.X, EndCenter.Y, 0};
	for (Toolbox::int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (Toolbox::Abs(End[Axis] - Start[Axis]) > Toolbox::f64(Toolbox::TNumericLimits<Toolbox::f32>::Max()))
		{
			throw Toolbox::FException("Unrepresentable 2D world sweep movement");
		}
	}
	return SweepColliders_Internal(StartShape, EndCenter, ExcludedBody, Filter, true);
}
// カプセルの移動で、開始時に接触しているColliderを除く。半径は正。
Toolbox::TOptional<FWorldSweepHit2D> FPhysicsWorld2D::SweepCapsuleClosestIgnoringInitialContacts(
    const Toolbox::FCapsule2D& StartShape, Toolbox::FVector2 EndCenter, Toolbox::TOptional<FBodyId2D> ExcludedBody,
    const FWorldQueryFilter& Filter) const
{
	if (!Toolbox::IsValid(StartShape) || !(StartShape.Radius > 0) || !EndCenter.IsValid())
	{
		throw Toolbox::FException("Invalid 2D world sweep shape or end center");
	}
	RequireRepresentableMove_Internal(QueryCenter_Internal(StartShape), EndCenter);
	return SweepColliders_Internal(StartShape, EndCenter, ExcludedBody, Filter, true);
}
// 索引（または総当たり）で候補を絞り、Margin以下の符号付き距離のColliderを固定容量の結果へ集める。
FWorldContactSet2D FPhysicsWorld2D::QueryContacts(const Toolbox::FCircle2D& Shape, Toolbox::f64 Margin,
                                                  Toolbox::TOptional<FBodyId2D> ExcludedBody,
                                                  const FWorldQueryFilter& Filter) const
{
	// マスクや空Worldでも入力・状態・除外IDの検査は省略しない。
	if (!(Toolbox::IsValid(Shape)) || !Toolbox::IsFinite(Margin) || Margin < 0)
	{
		throw Toolbox::FException("Invalid 2D world contact query");
	}
	return QueryContacts_Internal(Shape, Margin, ExcludedBody, Filter);
}
// カプセルの接触の問い合わせ。形状とMarginを検査してから走査する。
FWorldContactSet2D FPhysicsWorld2D::QueryCapsuleContacts(const Toolbox::FCapsule2D& Shape, Toolbox::f64 Margin,
                                                         Toolbox::TOptional<FBodyId2D> ExcludedBody,
                                                         const FWorldQueryFilter& Filter) const
{
	if (!Toolbox::IsValid(Shape) || !Toolbox::IsFinite(Margin) || Margin < 0)
	{
		throw Toolbox::FException("Invalid 2D world contact query");
	}
	return QueryContacts_Internal(Shape, Margin, ExcludedBody, Filter);
}
// QueryContactsの走査部分。
template <typename TShape>
FWorldContactSet2D FPhysicsWorld2D::QueryContacts_Internal(const TShape& Shape, Toolbox::f64 Margin,
                                                           const Toolbox::TOptional<FBodyId2D>& ExcludedBody,
                                                           const FWorldQueryFilter& Filter) const
{
	const FImpl& Impl = *m_pImpl;
	Impl.RequireQueryState_Internal();
	if (ExcludedBody)
	{
		(void)Impl.Resolve_Internal(*ExcludedBody);
	}
	// 候補を絞る範囲（中心から形状を覆う距離＋Marginまで）と、問い合わせの座標の規模。
	const Toolbox::FVector2 ShapeCenter = QueryCenter_Internal(Shape);
	const Toolbox::f64 Center[2] = {ShapeCenter.X, ShapeCenter.Y};
	const Toolbox::f64 QueryMaxAbs =
	    Toolbox::Max(Toolbox::Abs(Center[0]), Toolbox::Abs(Center[1])) + QueryReach_Internal(Shape) + Margin;
	const Toolbox::f64 Reach =
	    QueryReach_Internal(Shape) + Margin + PhysicsPrivate::QueryInflation_Internal(Impl.QueryIndex, QueryMaxAbs);
	PhysicsPrivate::TQueryBounds<2> Area;
	for (Toolbox::int32 Axis = 0; Axis < 2; ++Axis)
	{
		Area.Min[Axis] = Center[Axis] - Reach;
		Area.Max[Axis] = Center[Axis] + Reach;
	}
	// 呼出しごとのローカルな結果。例外時は破棄され、部分結果は外へ出ない。
	FWorldContactSet2D Result;
	auto Consider = [&](Toolbox::size_t Index, bool bIndexed)
	{
		// 既存の形状変換で現在の姿勢へ移し、符号付き距離を求める。
		const auto& Record = Impl.Colliders[Index];
		const auto Contact = Impl.QueryWorldShape_Internal(Index, bIndexed)
		                         .Visit(
		                             [&](const auto& WorldShape)
		                             {
			                             return Toolbox::FindShapeContact(Shape, WorldShape);
		                             });
		if (!Toolbox::IsFinite(Contact.Separation))
		{
			throw Toolbox::FException("Invalid 2D world contact separation");
		}
		if (Contact.Separation > Margin)
		{
			return;
		}
		FWorldContact2D Item;
		Item.Collider = {Record.Body, Index, Record.Generation};
		Item.Separation = Contact.Separation;
		Item.Normal = Contact.Normal;
		PhysicsPrivate::InsertContactBySlot_Internal(Result, Item);
	};
	PhysicsPrivate::FQueryVisitCounters Visit;
	Toolbox::uint64 NarrowTests = 0;
	const bool bFallback = PhysicsPrivate::VisitQueryCandidates_Internal(
	    Impl.QuerySource_Internal(), QueryMaxAbs, Filter, ExcludedBody,
	    [&](const PhysicsPrivate::TQueryBounds<2>& Bounds)
	    {
		    return PhysicsPrivate::Overlaps_Internal(Bounds, Area);
	    },
	    Consider, Visit, NarrowTests);
	PhysicsPrivate::CommitQueryCounters_Internal(Impl.bQueryDiagnostics, Impl.QueryTotals.Contacts, Visit, NarrowTests,
	                                             bFallback);
	return Result;
}
// 索引（または総当たり）で候補を絞り、範囲と重なる対象Colliderの完全なIDをスロット昇順で集める。
Toolbox::TVector<FColliderId2D> FPhysicsWorld2D::OverlapAll(const Toolbox::FCircle2D& Area,
                                                            Toolbox::TOptional<FBodyId2D> ExcludedBody,
                                                            const FWorldQueryFilter& Filter) const
{
	// マスクや空Worldでも入力・状態・除外IDの検査は省略しない。
	if (!Toolbox::IsValid(Area))
	{
		throw Toolbox::FException("Invalid 2D world overlap area");
	}
	return OverlapAll_Internal(Area, ExcludedBody, Filter);
}
// カプセルの範囲の重なりの問い合わせ。範囲を検査してから走査する。
Toolbox::TVector<FColliderId2D> FPhysicsWorld2D::OverlapCapsuleAll(const Toolbox::FCapsule2D& Area,
                                                                   Toolbox::TOptional<FBodyId2D> ExcludedBody,
                                                                   const FWorldQueryFilter& Filter) const
{
	if (!Toolbox::IsValid(Area))
	{
		throw Toolbox::FException("Invalid 2D world overlap area");
	}
	return OverlapAll_Internal(Area, ExcludedBody, Filter);
}
// OverlapAllの走査部分。
template <typename TShape>
Toolbox::TVector<FColliderId2D> FPhysicsWorld2D::OverlapAll_Internal(const TShape& Area,
                                                                     const Toolbox::TOptional<FBodyId2D>& ExcludedBody,
                                                                     const FWorldQueryFilter& Filter) const
{
	const FImpl& Impl = *m_pImpl;
	Impl.RequireQueryState_Internal();
	if (ExcludedBody)
	{
		(void)Impl.Resolve_Internal(*ExcludedBody);
	}
	// 候補を絞る範囲と、問い合わせの座標の規模。
	const Toolbox::FVector2 AreaCenter = QueryCenter_Internal(Area);
	const Toolbox::f64 Center[2] = {AreaCenter.X, AreaCenter.Y};
	const Toolbox::f64 QueryMaxAbs =
	    Toolbox::Max(Toolbox::Abs(Center[0]), Toolbox::Abs(Center[1])) + QueryReach_Internal(Area);
	const Toolbox::f64 Reach =
	    QueryReach_Internal(Area) + PhysicsPrivate::QueryInflation_Internal(Impl.QueryIndex, QueryMaxAbs);
	PhysicsPrivate::TQueryBounds<2> Bounds;
	for (Toolbox::int32 Axis = 0; Axis < 2; ++Axis)
	{
		Bounds.Min[Axis] = Center[Axis] - Reach;
		Bounds.Max[Axis] = Center[Axis] + Reach;
	}
	// 呼出しごとのローカルな結果。一致しなければ確保しない。例外時は破棄され、部分結果は外へ出ない。
	Toolbox::TVector<FColliderId2D> Result;
	auto Consider = [&](Toolbox::size_t Index, bool bIndexed)
	{
		// 既存の形状変換で現在の姿勢へ移し、許容距離0で重なりを判定する。
		const auto& Record = Impl.Colliders[Index];
		const bool bOverlaps = Impl.QueryWorldShape_Internal(Index, bIndexed)
		                           .Visit(
		                               [&](const auto& WorldShape)
		                               {
			                               return AreaOverlaps_Internal(Area, WorldShape);
		                               });
		if (bOverlaps)
		{
			Result.PushBack({Record.Body, Index, Record.Generation});
		}
	};
	PhysicsPrivate::FQueryVisitCounters Visit;
	Toolbox::uint64 NarrowTests = 0;
	const bool bFallback = PhysicsPrivate::VisitQueryCandidates_Internal(
	    Impl.QuerySource_Internal(), QueryMaxAbs, Filter, ExcludedBody,
	    [&](const PhysicsPrivate::TQueryBounds<2>& Node)
	    {
		    return PhysicsPrivate::Overlaps_Internal(Node, Bounds);
	    },
	    Consider, Visit, NarrowTests);
	if (!bFallback)
	{
		// 索引の訪問順をスロット昇順へ並べ替える（確保しない）。
		PhysicsPrivate::HeapSort_Internal(Result.Data(), Result.Size(),
		                                  [](const FColliderId2D& A, const FColliderId2D& B)
		                                  {
			                                  return A.Index < B.Index;
		                                  });
	}
	PhysicsPrivate::CommitQueryCounters_Internal(Impl.bQueryDiagnostics, Impl.QueryTotals.Overlap, Visit, NarrowTests,
	                                             bFallback);
	return Result;
}
// Colliderの問い合わせカテゴリを変更する。問い合わせの候補だけに影響し、他の状態は変えない。
void FPhysicsWorld2D::SetColliderQueryCategory(FColliderId2D Id, Toolbox::uint32 Categories)
{
	// 状態とIDの検査がすべて成功してから、値だけを書き換える。
	(void)m_pImpl->ResolveQueryCollider_Internal(Id);
	m_pImpl->Colliders[Id.Index].QueryCategory = Categories;
}
// Colliderの問い合わせカテゴリを返す。
Toolbox::uint32 FPhysicsWorld2D::GetColliderQueryCategory(FColliderId2D Id) const
{
	return m_pImpl->ResolveQueryCollider_Internal(Id).QueryCategory;
}
// ColliderのSolid／Sensorの区分を変更する。
void FPhysicsWorld2D::SetColliderResponse(FColliderId2D Id, EColliderResponse Response)
{
	// 状態・ID・値の検査がすべて成功してから書き換える。
	(void)m_pImpl->ResolveQueryCollider_Internal(Id);
	if (Response != EColliderResponse::Solid && Response != EColliderResponse::Sensor)
	{
		throw Toolbox::FException("Invalid 2D collider response");
	}
	m_pImpl->Colliders[Id.Index].Response = Response;
	m_pImpl->AfterResponseChange_Internal();
}
// 接触・Triggerのイベントの生成を設定する。
void FPhysicsWorld2D::SetEventSettings(const FWorldEventSettings& Settings)
{
	if (m_pImpl->SnapshotState.bInStep)
	{
		throw Toolbox::FException("2D world event settings cannot change during Step");
	}
	// 設定とバッチを変更する前に、イベント境界と詳細判定の作業領域を全て用意する。
	m_pImpl->Events.Validate(Settings);
	if (Settings.bEnabled)
	{
		m_pImpl->EventEntries.Reserve(m_pImpl->QueryWorldShapes.Size());
		m_pImpl->EventHits.Reserve(8);
	}
	m_pImpl->Events.Configure(Settings);
}
// Bodyの運動区分を返す。
EBodyType FPhysicsWorld2D::GetBodyType(FBodyId2D Id) const
{
	return m_pImpl->Resolve_Internal(Id).Type;
}
// Bodyに固定した点の、次のStepの後の位置を返す。
Toolbox::FVector2 FPhysicsWorld2D::PredictBodyPoint(FBodyId2D Id, Toolbox::FVector2 WorldPoint,
                                                    Toolbox::f64 DeltaSeconds) const
{
	m_pImpl->RequireQueryState_Internal();
	const FBodyRecord2D& Record = m_pImpl->Resolve_Internal(Id);
	if (!WorldPoint.IsValid() || !Toolbox::IsFinite(DeltaSeconds) || DeltaSeconds <= 0)
	{
		throw Toolbox::FException("Invalid 2D body point prediction");
	}
	if (Record.Type == EBodyType::Dynamic)
	{
		throw Toolbox::FException("Dynamic 2D body motion is not predictable before Step");
	}
	if (Record.Type == EBodyType::Static)
	{
		return WorldPoint;
	}
	// 現在の姿勢でのローカル位置（Stepの前後で同じ）。
	const Toolbox::f64 Cosine = Toolbox::Cos(Toolbox::f64(Record.Angle));
	const Toolbox::f64 Sine = Toolbox::Sin(Toolbox::f64(Record.Angle));
	const Toolbox::f64 DeltaX = Toolbox::f64(WorldPoint.X) - Record.Position.X;
	const Toolbox::f64 DeltaY = Toolbox::f64(WorldPoint.Y) - Record.Position.Y;
	const Toolbox::f64 LocalX = Cosine * DeltaX + Sine * DeltaY;
	const Toolbox::f64 LocalY = -Sine * DeltaX + Cosine * DeltaY;
	// Stepと同じ積分（位置と角度）で次の姿勢を作る。
	FBodyRecord2D Next = Record;
	IntegratePosition_Internal(Next, DeltaSeconds);
	const Toolbox::f64 NextCosine = Toolbox::Cos(Toolbox::f64(Next.Angle));
	const Toolbox::f64 NextSine = Toolbox::Sin(Toolbox::f64(Next.Angle));
	return {static_cast<Toolbox::f32>(Toolbox::f64(Next.Position.X) + NextCosine * LocalX - NextSine * LocalY),
	        static_cast<Toolbox::f32>(Toolbox::f64(Next.Position.Y) + NextSine * LocalX + NextCosine * LocalY)};
}
// 接触・Triggerのイベントの設定を返す。
FWorldEventSettings FPhysicsWorld2D::GetEventSettings() const noexcept
{
	return m_pImpl->Events.GetSettings();
}
// 直前に成功したStepのイベントのバッチを返す。
const FWorldEventBatch2D& FPhysicsWorld2D::GetEventBatch() const noexcept
{
	return m_pImpl->Events.GetBatch();
}
// ColliderのSolid／Sensorの区分を返す。
EColliderResponse FPhysicsWorld2D::GetColliderResponse(FColliderId2D Id) const
{
	return m_pImpl->ResolveQueryCollider_Internal(Id).Response;
}
// Colliderの衝突カテゴリとマスクを変更する。
void FPhysicsWorld2D::SetColliderCollisionFilter(FColliderId2D Id, const FColliderCollisionFilter& Filter)
{
	(void)m_pImpl->ResolveQueryCollider_Internal(Id);
	m_pImpl->Colliders[Id.Index].Collision = Filter;
	m_pImpl->AfterResponseChange_Internal();
}
// Colliderの衝突カテゴリとマスクを返す。
FColliderCollisionFilter FPhysicsWorld2D::GetColliderCollisionFilter(FColliderId2D Id) const
{
	return m_pImpl->ResolveQueryCollider_Internal(Id).Collision;
}
// 問い合わせの集計を有効／無効にする。
void FPhysicsWorld2D::SetQueryDiagnosticsEnabled(bool bEnabled) noexcept
{
	m_pImpl->bQueryDiagnostics = bEnabled;
}
// 索引の状態と累計を返す。
FWorldQueryDiagnostics FPhysicsWorld2D::GetQueryDiagnostics() const noexcept
{
	FWorldQueryDiagnostics Result = m_pImpl->QueryTotals;
	m_pImpl->QueryIndex.Describe(Result, m_pImpl->Colliders.Size(), m_pImpl->AliveColliders);
	return Result;
}
// 累計を0へ戻す。
void FPhysicsWorld2D::ResetQueryDiagnostics() noexcept
{
	m_pImpl->QueryTotals = {};
	m_pImpl->QueryIndex.ResetCounters();
}
// 検証用に、問い合わせを総当たりの参照経路へ切り替える。
void FPhysicsWorld2D::SetQueryIndexEnabled_Internal(bool bEnabled) noexcept
{
	m_pImpl->bQueryIndexEnabled = bEnabled;
}
// 検証用に、Solverとイベントの組の候補を総当たりの参照経路へ切り替える。
void FPhysicsWorld2D::SetSolverBroadPhaseEnabled_Internal(bool bEnabled) noexcept
{
	m_pImpl->bSolverIndexEnabled = bEnabled;
}
FPhysicsSnapshot2D FPhysicsWorld2D::CaptureSnapshot(const FPhysicsSnapshotLimits& Limits) const
{
	return PhysicsPrivate::CaptureSnapshot_Internal<FPhysicsSnapshot2D>(
	    m_pImpl->World, m_pImpl->SnapshotState, m_pImpl->Slots, m_pImpl->Colliders, Limits,
	    [](const FBodyRecord2D& Body, FPhysicsSnapshot2D::FBody& Item)
	    {
		    Item.Rotation = Body.Angle;
	    });
}
void FPhysicsWorld2D::Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps)
{
	if (!Toolbox::IsFinite(DeltaSeconds) || DeltaSeconds <= 0)
	{
		throw Toolbox::FException("Invalid 2D step seconds");
	}
	if (SubSteps < 1 || SubSteps > 1024)
	{
		throw Toolbox::FException("Invalid 2D sub step count");
	}
	// 引数検証後に観測を開始し、更新途中の例外では採取を禁止する。
	PhysicsPrivate::FSnapshotStepGuard SnapshotStep(m_pImpl->SnapshotState, DeltaSeconds, SubSteps);
	// 前回のバッチを未発行にする（途中で失敗したStepの後に、古いバッチを今回のものに見せない）。
	m_pImpl->Events.BeginStep();
	// 一回の更新を等分割し、蓄積力は全分割で保持する。
	const Toolbox::f64 Slice = DeltaSeconds / static_cast<Toolbox::f64>(SubSteps);
	// 診断は更新ごとに作り直す。
	m_pImpl->Diagnostics = {};
	m_pImpl->ExecutionDiagnostics = {};
	m_pImpl->ExecutionDiagnostics.ExecutionThreadCount = m_pImpl->ExecutionLanes_Internal();
	// 移動区間の解決を行うか。
	const bool bContinuous = m_pImpl->Continuous.bEnabled && m_pImpl->HasContinuousBody_Internal();
	for (Toolbox::uint32 SliceIndex = 0; SliceIndex < SubSteps; ++SliceIndex)
	{
		// 力と重力を速度へ反映する。
		if (m_pImpl->UseBorrowedJobs_Internal(m_pImpl->Execution.bParallelIntegration))
		{
			Toolbox::FJobSystem& Jobs = *m_pImpl->Execution.JobSystem;
			auto IntegrateOne = [&](Toolbox::size_t Index)
			{
				FBodyRecord2D& Record = m_pImpl->Slots[Index];
				if (!Record.bAlive || Record.Type != EBodyType::Dynamic || Record.bSleeping)
				{
					return;
				}
				IntegrateVelocity_Internal(Record, m_pImpl->Gravity, Slice);
			};
			if (!Toolbox::ParallelFor(Jobs, m_pImpl->Slots.Size(), IntegrateOne, 32))
			{
				throw Toolbox::FException("Parallel 2D velocity integration failed");
			}
		}
		else
		{
			for (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)
			{
				FBodyRecord2D& Record = m_pImpl->Slots[Index];
				if (!Record.bAlive || Record.Type != EBodyType::Dynamic || Record.bSleeping)
				{
					continue;
				}
				IntegrateVelocity_Internal(Record, m_pImpl->Gravity, Slice);
			}
		}
		// 現在位置の接触を集めて速度拘束を解く。
		Toolbox::TVector<FManifold2D> Manifolds;
		m_pImpl->GenerateStepManifolds_Internal(Manifolds);
		// 接触とJointからIslandを一度だけ構築し、起床・求解・休止判定で共有する。
		// 睡眠判定のためにUnion-Findを作り直さない。
		Toolbox::TVector<PhysicsPrivate::FPhysicsIsland> Islands;
		m_pImpl->BuildIslands_Internal(Manifolds, Islands);
		m_pImpl->ExecutionDiagnostics.IslandCount = Islands.Size();
		// Solver前に必要な起床をMain側で完了させる。並列Workerからは起こさない。
		m_pImpl->WakeIslands_Internal(Manifolds, Islands, Slice);
		for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
		{
			m_pImpl->WarmStart_Internal(Manifolds[ManifoldIndex]);
		}
		// 新規接触で起きたBodyをIsland全体へ伝播する。既存Contact起床は保持する。
		m_pImpl->PropagateAwakeDynamics_Internal(Islands);
		// JointのWarm StartはMain側で全生存Jointへ適用する。
		// WorkerがJoint Recordへ触る構造はJ4の並列commitまで作らない。
		m_pImpl->WarmStartJoints_Internal();
		// IslandはDynamicを共有しないため逐次でも並列でも結果は同じ。
		if (m_pImpl->UseBorrowedJobs_Internal(m_pImpl->Execution.bParallelIslandSolver))
		{
			m_pImpl->SolveIslands_Parallel_Internal(Manifolds, Islands);
		}
		else
		{
			m_pImpl->SolveIslands_Serial_Internal(Manifolds, Islands);
		}
		m_pImpl->StoreCache_Internal(Manifolds);
		if (bContinuous)
		{
			// 最初接触まで進めて残りを解決する。
			m_pImpl->AdvanceContinuous_Internal(Slice);
			// 移動後の分離で貫通を補正する。
			Toolbox::TVector<FManifold2D> Touched;
			m_pImpl->GenerateStepManifolds_Internal(Touched);
			m_pImpl->CorrectPositions_Internal(Touched);
			// 移動後もJointの距離誤差を位置で補正する。
			m_pImpl->CorrectDistanceJointPositions_Internal();
			// 移動後の接触からIslandを作り直し、休止判定に使う。
			// 連続衝突の区間だけ再構築する。通常経路は一度で済ませる。
			Toolbox::TVector<PhysicsPrivate::FPhysicsIsland> MovedIslands;
			m_pImpl->BuildIslands_Internal(Touched, MovedIslands);
			m_pImpl->UpdateSleep_Internal(Touched, MovedIslands, Slice);
			continue;
		}
		// 更新後の速度で位置と姿勢を進める。
		if (m_pImpl->UseBorrowedJobs_Internal(m_pImpl->Execution.bParallelIntegration))
		{
			Toolbox::FJobSystem& Jobs = *m_pImpl->Execution.JobSystem;
			auto IntegrateOne = [&](Toolbox::size_t Index)
			{
				FBodyRecord2D& Record = m_pImpl->Slots[Index];
				if (!Record.bAlive)
				{
					return;
				}
				if (Record.Type == EBodyType::Dynamic)
				{
					IntegratePosition_Internal(Record, Slice);
				}
				else if (Record.Type == EBodyType::Kinematic)
				{
					IntegrateKinematic_Internal(Record, Slice);
				}
			};
			if (!Toolbox::ParallelFor(Jobs, m_pImpl->Slots.Size(), IntegrateOne, 32))
			{
				throw Toolbox::FException("Parallel 2D position integration failed");
			}
		}
		else
		{
			for (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)
			{
				FBodyRecord2D& Record = m_pImpl->Slots[Index];
				if (!Record.bAlive)
				{
					continue;
				}
				if (Record.Type == EBodyType::Dynamic)
				{
					IntegratePosition_Internal(Record, Slice);
				}
				else if (Record.Type == EBodyType::Kinematic)
				{
					IntegrateKinematic_Internal(Record, Slice);
				}
			}
		}
		// 許容幅を超える貫通を位置で補正する。
		m_pImpl->CorrectPositions_Internal(Manifolds);
		// Jointの距離誤差も位置で補正する。並進中心の補正。
		m_pImpl->CorrectDistanceJointPositions_Internal();
		// 同じIsland列で休止を評価する。拘束の支持は接触点数ではなくContactとJointの参加で決まる。
		m_pImpl->UpdateSleep_Internal(Manifolds, Islands, Slice);
	}
	// 蓄積した力とトルクを一度だけ消去する。
	for (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)
	{
		FBodyRecord2D& Record = m_pImpl->Slots[Index];
		Record.Force = {};
		Record.Torque = 0;
	}
	// 積分・接触補正・連続衝突を含む最終姿勢へ問い合わせの索引を合わせてから、問い合わせを受け付ける状態へ戻す。
	// 途中で失敗したStepでは合わせないが、問い合わせは拒否され、次の正常なStepの完了時に全員を合わせ直す。
	m_pImpl->RefreshMovingColliders_Internal();
	// 最終姿勢で接触・Triggerの組を確定し、前回との差を発行する（このStepが成功として完了する直前）。
	if (m_pImpl->Events.IsEnabled())
	{
		m_pImpl->CollectEvents_Internal();
		const FImpl& Impl = *m_pImpl;
		m_pImpl->Events.Publish(m_pImpl->SnapshotState.StepIndex + 1,
		                        [&Impl](const auto& Pair)
		                        {
			                        return Impl.EndReason_Internal(Pair);
		                        });
	}
	SnapshotStep.Complete();
}
} // namespace Dxf
