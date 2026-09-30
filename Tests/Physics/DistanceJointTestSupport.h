// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_DISTANCE_JOINT_TEST_SUPPORT_H
#define DXF_DISTANCE_JOINT_TEST_SUPPORT_H
#include "QueryIndexTestSupport.h"
#include "Dxf/PhysicsExecution.h"
namespace PhysicsTest::JointTest
{
/**
 * 2DのJoint試験で使う型と姿勢操作。
 */
struct F2D : QueryIndexTest::F2D
{
	/**
	 * Z成分を持たない次元。
	 */
	static constexpr bool b3D = false;
	/**
	 * 距離拘束の登録設定。
	 */
	using FJointDescription = Dxf::FDistanceJointDescription2D;
	/**
	 * 距離拘束の識別子。
	 */
	using FJoint = Dxf::FJointId2D;
	/**
	 * 接触の反復設定。
	 */
	using FContact = Dxf::FContactSettings2D;
	/**
	 * 連続衝突の設定。
	 */
	using FContinuous = Dxf::FContinuousSettings2D;
	/**
	 * 姿勢と角速度を値列へ追加する。各成分を個別に比較し、構造体の余白を比較しない。
	 * @param World 対象World。
	 * @param Body 採取するBody。
	 * @param Values 比較用の列。
	 */
	static void AppendRotation(const FWorld& World, FBody Body, Toolbox::TVector<Toolbox::f64>& Values)
	{
		Values.PushBack(World.GetAngle(Body));
		Values.PushBack(World.GetAngularVelocity(Body));
	}
	/**
	 * 生成姿勢と角速度へ戻す。Jointの記録には触らない。
	 * @param World 対象World。
	 * @param Body 戻すBody。
	 * @param Position 生成位置。
	 */
	static void Reset(FWorld& World, FBody Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, 0);
		World.SetVelocity(Body, {});
		World.SetAngularVelocity(Body, 0);
	}
};
/**
 * 3DのJoint試験で使う型と姿勢操作。
 */
struct F3D : QueryIndexTest::F3D
{
	/**
	 * Z成分を持つ次元。
	 */
	static constexpr bool b3D = true;
	/**
	 * 距離拘束の登録設定。
	 */
	using FJointDescription = Dxf::FDistanceJointDescription3D;
	/**
	 * 距離拘束の識別子。
	 */
	using FJoint = Dxf::FJointId3D;
	/**
	 * 接触の反復設定。
	 */
	using FContact = Dxf::FContactSettings3D;
	/**
	 * 連続衝突の設定。
	 */
	using FContinuous = Dxf::FContinuousSettings3D;
	/**
	 * 姿勢と角速度を成分ごとに値列へ追加する。
	 * @param World 対象World。
	 * @param Body 採取するBody。
	 * @param Values 比較用の列。
	 */
	static void AppendRotation(const FWorld& World, FBody Body, Toolbox::TVector<Toolbox::f64>& Values)
	{
		// 比較対象BodyのQuaternion全成分。
		const auto Rotation = World.GetOrientation(Body);
		Values.PushBack(Rotation.X);
		Values.PushBack(Rotation.Y);
		Values.PushBack(Rotation.Z);
		Values.PushBack(Rotation.W);
		// 比較対象Bodyの角速度全成分。
		const auto Angular = World.GetAngularVelocity(Body);
		Values.PushBack(Angular.X);
		Values.PushBack(Angular.Y);
		Values.PushBack(Angular.Z);
	}
	/**
	 * 生成姿勢と角速度へ戻す。Jointの記録には触らない。
	 * @param World 対象World。
	 * @param Body 戻すBody。
	 * @param Position 生成位置。
	 */
	static void Reset(FWorld& World, FBody Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, {});
		World.SetVelocity(Body, {});
		World.SetAngularVelocity(Body, {});
	}
};
/**
 * Bodyの観測値を比較用の列へ追加する。休止状態も含む。
 * @param World 採取するWorld。
 * @param Body 採取するBody。
 * @param Values 比較用の列。
 */
template <typename T>
void AppendBody(const typename T::FWorld& World, typename T::FBody Body, Toolbox::TVector<Toolbox::f64>& Values)
{
	// 比較対象Bodyの現在位置。
	const auto Position = World.GetPosition(Body);
	// 比較対象Bodyの現在速度。
	const auto Velocity = World.GetVelocity(Body);
	Values.PushBack(Position.X);
	Values.PushBack(Position.Y);
	Values.PushBack(Velocity.X);
	Values.PushBack(Velocity.Y);
	if constexpr (T::b3D)
	{
		Values.PushBack(Position.Z);
		Values.PushBack(Velocity.Z);
	}
	T::AppendRotation(World, Body, Values);
	Values.PushBack(World.IsSleeping(Body) ? 1.0 : 0.0);
}
/**
 * 値列の浮動小数点表現が全て同一かを検査する。
 * @param A 比較元。
 * @param B 比較先。
 */
inline void RequireBits(const Toolbox::TVector<Toolbox::f64>& A, const Toolbox::TVector<Toolbox::f64>& B)
{
	PHYSICS_REQUIRE(A.Size() == B.Size());
	for (Toolbox::size_t Index = 0; Index < A.Size(); ++Index)
	{
		PHYSICS_REQUIRE(memcmp(&A[Index], &B[Index], sizeof(Toolbox::f64)) == 0);
	}
}
} // namespace PhysicsTest::JointTest
#endif
