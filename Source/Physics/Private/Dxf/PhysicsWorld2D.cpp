// SPDX-License-Identifier: NOASSERTION
#include "Dxf/RigidBody2D.h"
#include "ParallelPhysicsCore.h"
#include "Toolbox/ContinuousCollision.h"
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
	// 今回分割で接触したか。
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
	Toolbox::TVariant<Toolbox::FCircle2D, Toolbox::FOrientedBox2D> Shape;
	// 摩擦係数。
	Toolbox::f32 Friction = 0.5f;
	// 反発係数。
	Toolbox::f32 Restitution = 0;
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
	// 休止の条件。
	FSleepSettings2D Sleep;
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
		const FBodyRecord2D& Record = Slots[Id.Index];
		return (Record.bAlive && Record.Generation == Id.Generation) ? &Record : nullptr;
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
	// 全コライダー組から多様体列を作る。
	void GenerateManifolds_Internal(Toolbox::TVector<FManifold2D>& Out)
	{
		for (Toolbox::size_t First = 0; First < Colliders.Size(); ++First)
		{
			FColliderRecord2D& RecordA = Colliders[First];
			if (!RecordA.bAlive)
			{
				continue;
			}
			FBodyRecord2D* BodyA = Find_Internal(RecordA.Body);
			if (BodyA == nullptr)
			{
				continue;
			}
			for (Toolbox::size_t Second = First + 1; Second < Colliders.Size(); ++Second)
			{
				FColliderRecord2D& RecordB = Colliders[Second];
				if (!RecordB.bAlive)
				{
					continue;
				}
				FBodyRecord2D* BodyB = Find_Internal(RecordB.Body);
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
					Out.PushBack(Manifold);
				}
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
	// 多様体列の速度拘束を反復して解く。
	void SolveVelocities_Internal(Toolbox::TVector<FManifold2D>& Manifolds)
	{
		// 全制約の安定添字。
		Toolbox::TVector<Toolbox::size_t> All;
		All.Reserve(Manifolds.Size());
		for (Toolbox::size_t Index = 0; Index < Manifolds.Size(); ++Index)
		{
			All.PushBack(Index);
		}
		SolveConstraints_Internal(Manifolds, All);
	}
	// 制約添字列の速度拘束を添字順に反復して解く。
	void SolveConstraints_Internal(Toolbox::TVector<FManifold2D>& Manifolds,
	                               const Toolbox::TVector<Toolbox::size_t>& Constraints)
	{
		for (Toolbox::uint32 Iteration = 0; Iteration < Contact.VelocityIterations; ++Iteration)
		{
			for (Toolbox::size_t Slot = 0; Slot < Constraints.Size(); ++Slot)
			{
				FManifold2D& Manifold = Manifolds[Constraints[Slot]];
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
	// 多様体列からIslandを構築し、独立Islandごとに拘束を解く。
	// IslandはDynamic Bodyを共有せず、Static/Kinematicへは書き込まない。
	// WarmStartは呼び出し側で全多様体へ済ませておく。
	void SolveIslands_Internal(Toolbox::TVector<FManifold2D>& Manifolds)
	{
		Toolbox::TVector<PhysicsPrivate::FIslandEdge> Edges;
		Edges.Reserve(Manifolds.Size());
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
			Edge.ConstraintIndex = Index;
			Edge.bDynamicA = BodyA->Type == EBodyType::Dynamic;
			Edge.bDynamicB = BodyB->Type == EBodyType::Dynamic;
			Edges.PushBack(Edge);
		}
		Toolbox::TVector<PhysicsPrivate::FPhysicsIsland> Islands;
		PhysicsPrivate::FIslandManager::Build(Slots.Size(), Edges, Islands);
		ExecutionDiagnostics.IslandCount = Islands.Size();
		if (Islands.IsEmpty())
		{
			return;
		}
		auto SolveOne = [&](Toolbox::size_t IslandIndex)
		{
			SolveConstraints_Internal(Manifolds, Islands[IslandIndex].ConstraintIndices);
		};
		if (UseBorrowedJobs_Internal(Execution.bParallelIslandSolver))
		{
			if (!Toolbox::ParallelFor(*Execution.JobSystem, Islands.Size(), SolveOne, 1))
			{
				throw Toolbox::FException("Parallel 2D island solver failed");
			}
			ExecutionDiagnostics.SolverIslandCount += Islands.Size();
		}
		else
		{
			for (Toolbox::size_t IslandIndex = 0; IslandIndex < Islands.Size(); ++IslandIndex)
			{
				SolveOne(IslandIndex);
			}
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
	// 低速接触の継続で休止し、支持を失ったら起こす。
	void UpdateSleep_Internal(const Toolbox::TVector<FManifold2D>& Manifolds, Toolbox::f64 Slice) noexcept
	{
		if (!Sleep.bEnabled)
		{
			return;
		}
		for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
		{
			const FManifold2D& Manifold = Manifolds[ManifoldIndex];
			FBodyRecord2D* BodyA = Find_Internal(Manifold.BodyA);
			FBodyRecord2D* BodyB = Find_Internal(Manifold.BodyB);
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
			FBodyRecord2D& Record = Slots[Index];
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
			                                         Toolbox::f64(Record.Velocity.Y) * Record.Velocity.Y);
			const Toolbox::f64 Spin =
			    Record.AngularVelocity < 0 ? -Toolbox::f64(Record.AngularVelocity) : Record.AngularVelocity;
			if (Speed <= Sleep.LinearSpeedLimit && Spin <= Sleep.AngularSpeedLimit)
			{
				Record.SleepTimer = static_cast<Toolbox::f32>(Toolbox::f64(Record.SleepTimer) + Slice);
				if (Record.SleepTimer >= Sleep.TimeoutSeconds)
				{
					Record.bSleeping = true;
					Record.Velocity = {};
					Record.AngularVelocity = 0;
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
		if (IndexA == 1 && IndexB == 1)
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
	// 取り付け済みのコライダーも失効させる。
	for (Toolbox::size_t Index = 0; Index < m_pImpl->Colliders.Size(); ++Index)
	{
		FColliderRecord2D& Collider = m_pImpl->Colliders[Index];
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
bool FPhysicsWorld2D::IsAlive(FBodyId2D Id) const noexcept
{
	return m_pImpl->Find_Internal(Id) != nullptr;
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
	(void)Target;
	if (Description.Shape.Index() == 0)
	{
		if (!Toolbox::IsValid(Description.Shape.Get<0>()))
		{
			throw Toolbox::FException("Invalid 2D circle collider");
		}
	}
	else
	{
		if (!Toolbox::IsValid(Description.Shape.Get<1>()))
		{
			throw Toolbox::FException("Invalid 2D box collider");
		}
	}
	if (!Toolbox::IsFinite(Description.Friction) || Description.Friction < 0)
	{
		throw Toolbox::FException("Invalid 2D collider friction");
	}
	if (!Toolbox::IsFinite(Description.Restitution) || Description.Restitution < 0 || Description.Restitution > 1)
	{
		throw Toolbox::FException("Invalid 2D collider restitution");
	}
	// 新しい登録の初期状態。
	FColliderRecord2D Record;
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
	// 古い接触記録を使い回さない。
	m_pImpl->Cache.Clear();
	return true;
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
}
void FPhysicsWorld2D::ClearContactCache() noexcept
{
	m_pImpl->Cache.Clear();
}
bool FPhysicsWorld2D::IsColliderAlive(FColliderId2D Id) const noexcept
{
	return m_pImpl->FindCollider_Internal(Id) != nullptr;
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
		for (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)
		{
			m_pImpl->WarmStart_Internal(Manifolds[ManifoldIndex]);
		}
		if (m_pImpl->Execution.JobSystem == nullptr)
		{
			m_pImpl->SolveVelocities_Internal(Manifolds);
		}
		else
		{
			m_pImpl->SolveIslands_Internal(Manifolds);
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
			m_pImpl->UpdateSleep_Internal(Touched, Slice);
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
		m_pImpl->UpdateSleep_Internal(Manifolds, Slice);
	}
	// 蓄積した力とトルクを一度だけ消去する。
	for (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)
	{
		FBodyRecord2D& Record = m_pImpl->Slots[Index];
		Record.Force = {};
		Record.Torque = 0;
	}
}
} // namespace Dxf
