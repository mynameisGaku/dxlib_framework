// SPDX-License-Identifier: NOASSERTION
// 回転する支持点を、製品の姿勢変換関数を使わない解析式で全成分比較する。
#include "Support/Test.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
namespace
{
// 一回の固定更新の秒数。
constexpr Toolbox::f64 StepSeconds = 1.0 / 60.0;
// 角速度の積分と公開f32座標の丸めを含む、位置の許容誤差。
constexpr Toolbox::f64 PositionTolerance = 3e-6;

// 期待した全成分との差を確認する。
void RequireVector_Internal(Toolbox::FVector2 Actual, Toolbox::f64 X, Toolbox::f64 Y)
{
	REQUIRE(Toolbox::Abs(static_cast<Toolbox::f64>(Actual.X) - X) < PositionTolerance);
	REQUIRE(Toolbox::Abs(static_cast<Toolbox::f64>(Actual.Y) - Y) < PositionTolerance);
}

// 3Dでは回転面のZも必ず確認する。
void RequireVector_Internal(Toolbox::FVector3 Actual, Toolbox::f64 X, Toolbox::f64 Y, Toolbox::f64 Z)
{
	REQUIRE(Toolbox::Abs(static_cast<Toolbox::f64>(Actual.X) - X) < PositionTolerance);
	REQUIRE(Toolbox::Abs(static_cast<Toolbox::f64>(Actual.Y) - Y) < PositionTolerance);
	REQUIRE(Toolbox::Abs(static_cast<Toolbox::f64>(Actual.Z) - Z) < PositionTolerance);
}
} // namespace

// 既知の並進と回転を持つ床に、中心から離れて乗った円の候補位置を解析式と比較する。
TEST("2D moving support matches independent sine cosine coordinates")
{
	// 接触計算と運動情報を提供する実World。
	Dxf::FPhysicsWorld2D World;
	// 原点から離れた回転中心。姿勢の初期角は0。
	Dxf::FBodyDescription2D Body;
	Body.Type = Dxf::EBodyType::Kinematic;
	Body.Position = {2, 0};
	Body.Velocity = {0.6f, 0.3f};
	Body.AngularVelocity = 0.4f;
	// 後でWorldの非変更と完全な支持IDも照合する。
	const auto BodyId = World.CreateBody(Body);
	// 上面がy=0.25の床。
	Dxf::FColliderDescription2D Shape;
	Shape.Shape = Toolbox::FOrientedBox2D{{0, 0}, {4, 0.25f}, 0};
	const auto Collider = World.AttachCollider(BodyId, Shape);
	// 半径0.5と接触余裕0.02を含む床上の中心。
	Dxf::FCharacterState2D State;
	State.Center = {3.5f, 0.77f};
	// 既定の追従契約を使う。
	Dxf::FCharacterMoveSettings2D Settings;
	// 歩行・ジャンプを要求せず、床の運動だけを測る。
	const Dxf::FCharacterMoveInput2D Input;
	const auto Result = Dxf::StepCharacter(World, Settings, State, Input, StepSeconds);
	// 期待値は既知の回転中心・角速度・初期相対位置から直接作る。
	const Toolbox::f64 Angle = static_cast<Toolbox::f64>(Body.AngularVelocity) * StepSeconds;
	const Toolbox::f64 C = Toolbox::Cos(Angle);
	const Toolbox::f64 S = Toolbox::Sin(Angle);
	const Toolbox::f64 LocalX = static_cast<Toolbox::f64>(State.Center.X) - Body.Position.X;
	const Toolbox::f64 LocalY = static_cast<Toolbox::f64>(State.Center.Y) - Body.Position.Y;
	const Toolbox::f64 ExpectedX = Body.Position.X + Body.Velocity.X * StepSeconds + C * LocalX - S * LocalY;
	const Toolbox::f64 ExpectedY = Body.Position.Y + Body.Velocity.Y * StepSeconds + S * LocalX + C * LocalY;
	REQUIRE(Result.bCarried && !Result.bCarryBlocked && Result.Carrier && *Result.Carrier == Collider);
	RequireVector_Internal(Result.State.Center, ExpectedX, ExpectedY);
	RequireVector_Internal(Result.CarryRequested, ExpectedX - State.Center.X, ExpectedY - State.Center.Y);
	REQUIRE(World.GetPosition(BodyId) == Body.Position && World.GetAngle(BodyId) == Body.Angle);
}

// 3Dの鉛直軸回りの回転はXとZの両方へ現れ、符号を逆にしたZも検出する。
TEST("3D moving support matches independent sine cosine in all axes")
{
	// 接触計算と運動情報を提供する実World。
	Dxf::FPhysicsWorld3D World;
	// 原点から離れた回転中心。姿勢の初期回転は単位四元数。
	Dxf::FBodyDescription3D Body;
	Body.Type = Dxf::EBodyType::Kinematic;
	Body.Position = {2, 0, -3};
	Body.Velocity = {0.6f, 0.3f, -0.2f};
	Body.AngularVelocity = {0, 0.4f, 0};
	// 後でWorldの非変更と完全な支持IDも照合する。
	const auto BodyId = World.CreateBody(Body);
	// XZの両方に幅がある床。
	Dxf::FColliderDescription3D Shape;
	Shape.Shape = Toolbox::FOBB{{0, 0, 0}, {4, 0.25f, 4}};
	const auto Collider = World.AttachCollider(BodyId, Shape);
	// 回転中心からXとZの両方へ離れた位置に立つ球。
	Dxf::FCharacterState3D State;
	State.Center = {3.5f, 0.77f, -2.25f};
	// 既定の追従契約を使う。
	Dxf::FCharacterMoveSettings3D Settings;
	// 歩行・ジャンプを要求せず、床の運動だけを測る。
	const Dxf::FCharacterMoveInput3D Input;
	const auto Result = Dxf::StepCharacter(World, Settings, State, Input, StepSeconds);
	// Y軸回転の独立式。Quaternion::RotateやPredictBodyPointで期待値を作らない。
	const Toolbox::f64 Angle = static_cast<Toolbox::f64>(Body.AngularVelocity.Y) * StepSeconds;
	const Toolbox::f64 C = Toolbox::Cos(Angle);
	const Toolbox::f64 S = Toolbox::Sin(Angle);
	const Toolbox::f64 LocalX = static_cast<Toolbox::f64>(State.Center.X) - Body.Position.X;
	const Toolbox::f64 LocalZ = static_cast<Toolbox::f64>(State.Center.Z) - Body.Position.Z;
	const Toolbox::f64 ExpectedX = Body.Position.X + Body.Velocity.X * StepSeconds + C * LocalX + S * LocalZ;
	const Toolbox::f64 ExpectedY = State.Center.Y + Body.Velocity.Y * StepSeconds;
	const Toolbox::f64 ExpectedZ = Body.Position.Z + Body.Velocity.Z * StepSeconds - S * LocalX + C * LocalZ;
	REQUIRE(Result.bCarried && !Result.bCarryBlocked && Result.Carrier && *Result.Carrier == Collider);
	RequireVector_Internal(Result.State.Center, ExpectedX, ExpectedY, ExpectedZ);
	RequireVector_Internal(Result.CarryRequested, ExpectedX - State.Center.X, ExpectedY - State.Center.Y,
	                       ExpectedZ - State.Center.Z);
	REQUIRE(World.GetPosition(BodyId) == Body.Position);
}
