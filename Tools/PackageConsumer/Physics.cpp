// SPDX-License-Identifier: NOASSERTION
// Physicsだけを使う外部の利用者（dxf::physics）。World問い合わせ・移動候補・キャラクター移動・索引の診断を確かめる。終了コード0が成功。
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Dxf/WorldSlideMove2D.h"
#include "Dxf/WorldSlideMove3D.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
#include "Dxf/WorldQueryDiagnostics.h"
// 円スイープ: 中心線の射線は外れ、半径のある移動は当たる。除外・カテゴリ・静止・削除後の失効。
// 接触法線: 壁の左下の角(4.5,0.4)から中心(4.2,0)へ向く(-0.6,-0.8)。初期接触・半径0では空。
static int Sweep2D()
{
	Dxf::FPhysicsWorld2D World;
	Dxf::FBodyDescription2D Fixed;
	Fixed.Type = Dxf::EBodyType::Static;
	const auto Self = World.CreateBody(Fixed);
	Dxf::FColliderDescription2D Mine;
	Mine.Shape = Toolbox::FCircle2D{{0, 0}, 0.5f};
	World.AttachCollider(Self, Mine);
	Fixed.Position = {5, 0.9f};
	const auto WallBody = World.CreateBody(Fixed);
	Dxf::FColliderDescription2D Wall;
	Wall.Shape = Toolbox::FOrientedBox2D{{0, 0}, {0.5f, 0.5f}, 0};
	Wall.QueryCategory = 1u;
	const auto WallId = World.AttachCollider(WallBody, Wall);
	Fixed.Position = {3, -0.3f};
	Dxf::FColliderDescription2D Pickup;
	Pickup.Shape = Toolbox::FCircle2D{{0, 0}, 0.2f};
	Pickup.QueryCategory = 4u;
	World.AttachCollider(World.CreateBody(Fixed), Pickup);
	Dxf::FWorldQueryFilter Obstacles;
	Obstacles.IncludeCategories = 1u;
	const Toolbox::FCircle2D Probe{{0, 0}, 0.5f};
	if (World.RaycastClosest({0, 0}, {10, 0}, Self, Obstacles))
	{
		return 101;
	}
	const auto Hit = World.SweepClosest(Probe, {10, 0}, Self, Obstacles);
	if (!Hit || Hit->Collider != WallId || Hit->bInitialContact || Toolbox::Abs(Hit->CenterAtHit.X - 4.2f) > 1e-4f ||
	    Hit->CenterAtHit.Y != 0)
	{
		return 102;
	}
	const auto Any = World.SweepClosest(Probe, {10, 0});
	if (!Any || !Any->bInitialContact || Any->Fraction != 0 || Any->Collider.Body != Self)
	{
		return 103;
	}
	if (!Hit->Normal || Toolbox::Abs(Hit->Normal->X + 0.6f) > 1e-5f || Toolbox::Abs(Hit->Normal->Y + 0.8f) > 1e-5f ||
	    Any->Normal)
	{
		return 107;
	}
	const auto Ray = World.RaycastClosest({0, 0.9f}, {10, 0.9f}, Self, Obstacles);
	const auto Thin = World.SweepClosest(Toolbox::FCircle2D{{0, 0.9f}, 0}, {10, 0.9f}, Self, Obstacles);
	if (!Ray || !Thin || Thin->Fraction != Ray->Fraction || Thin->Normal)
	{
		return 108;
	}
	World.SetColliderQueryCategory(WallId, 0u);
	if (World.SweepClosest(Probe, {10, 0}, Self, Obstacles))
	{
		return 104;
	}
	World.SetColliderQueryCategory(WallId, 1u);
	const auto Still = World.SweepClosest(Toolbox::FCircle2D{{5, 0.9f}, 0.1f}, {5, 0.9f}, Self, Obstacles);
	if (!Still || !Still->bInitialContact || Still->Fraction != 0 || Still->Collider != WallId || Still->Normal)
	{
		return 105;
	}
	World.DestroyBody(WallBody);
	if (World.IsColliderAlive(Hit->Collider) || World.SweepClosest(Probe, {10, 0}, Self, Obstacles))
	{
		return 106;
	}
	return 0;
}
// 球スイープ（カメラ位置候補の近似）: 回転したOBBの壁で同じ流れを確認する。
// 接触法線は有限の単位方向で、移動と逆向きの成分を持つ。初期接触では空。
static int Sweep3D()
{
	Dxf::FPhysicsWorld3D World;
	Dxf::FBodyDescription3D Fixed;
	Fixed.Type = Dxf::EBodyType::Static;
	const auto Self = World.CreateBody(Fixed);
	Dxf::FColliderDescription3D Mine;
	Mine.Shape = Toolbox::FSphere{{0, 0, 0}, 0.5f};
	World.AttachCollider(Self, Mine);
	Fixed.Position = {5, 0.9f, 0};
	Fixed.Orientation = Toolbox::FQuaternion::FromAxisAngle({1, 0, 0}, 0.4f);
	const auto WallBody = World.CreateBody(Fixed);
	Dxf::FColliderDescription3D Wall;
	Wall.Shape = Toolbox::FOBB{{0, 0, 0}, {0.5f, 0.5f, 0.5f}};
	Wall.QueryCategory = 1u;
	const auto WallId = World.AttachCollider(WallBody, Wall);
	Dxf::FWorldQueryFilter Obstacles;
	Obstacles.IncludeCategories = 1u;
	const Toolbox::FSphere Probe{{0, 0, 0}, 0.5f};
	if (World.RaycastClosest({0, 0, 0}, {10, 0, 0}, Self, Obstacles))
	{
		return 111;
	}
	const auto Hit = World.SweepClosest(Probe, {10, 0, 0}, Self, Obstacles);
	if (!Hit || Hit->Collider != WallId || Hit->bInitialContact || Hit->Fraction <= 0 || Hit->Fraction >= 1 ||
	    Hit->CenterAtHit.Y != 0 || Hit->CenterAtHit.Z != 0)
	{
		return 112;
	}
	if (!Hit->Normal || !(Hit->Normal->X < 0))
	{
		return 116;
	}
	const Toolbox::f32 LengthSquared =
	    Hit->Normal->X * Hit->Normal->X + Hit->Normal->Y * Hit->Normal->Y + Hit->Normal->Z * Hit->Normal->Z;
	if (Toolbox::Abs(LengthSquared - 1.0f) > 1e-5f)
	{
		return 117;
	}
	World.SetColliderQueryCategory(WallId, 0u);
	if (World.SweepClosest(Probe, {10, 0, 0}, Self, Obstacles))
	{
		return 113;
	}
	World.SetColliderQueryCategory(WallId, 1u);
	const auto Still = World.SweepClosest(Toolbox::FSphere{{5, 0.9f, 0}, 0.1f}, {5, 0.9f, 0}, Self, Obstacles);
	if (!Still || !Still->bInitialContact || Still->Collider != WallId || Still->Normal)
	{
		return 114;
	}
	World.DestroyBody(WallBody);
	if (World.IsColliderAlive(Hit->Collider) || World.SweepClosest(Probe, {10, 0, 0}, Self, Obstacles))
	{
		return 115;
	}
	return 0;
}
// 円の移動候補: 正面の壁で停止、斜めに当たって1回滑り途中の板で停止、板を外すと滑り切る、初期接触で停止。
static int Slide2D()
{
	Dxf::FPhysicsWorld2D World;
	const auto Level = World.CreateBody({});
	Dxf::FColliderDescription2D Wall;
	Wall.Shape = Toolbox::FOrientedBox2D{{6, 0}, {1, 50}, 0};
	World.AttachCollider(Level, Wall);
	Dxf::FColliderDescription2D Plate;
	Plate.Shape = Toolbox::FOrientedBox2D{{4, 3}, {0.9f, 0}, 0};
	const auto PlateId = World.AttachCollider(Level, Plate);
	const Toolbox::FCircle2D Probe{{0, 0}, 0.5f};
	const auto Head = Dxf::ComputeSlideMove(World, Probe, {10, 0}, 0.01);
	if (Head.Stop != Dxf::EWorldSlideStop::Blocked || Toolbox::Abs(Head.EndCenter.X - 4.49f) > 1e-5f)
	{
		return 121;
	}
	const auto Stopped = Dxf::ComputeSlideMove(World, Probe, {10, 4}, 0.01);
	if (Stopped.Stop != Dxf::EWorldSlideStop::Blocked || !Stopped.SlideHit || Stopped.SlideHit->Collider != PlateId ||
	    Toolbox::Abs(Stopped.EndCenter.Y - 2.49f) > 1e-5f)
	{
		return 122;
	}
	World.DetachCollider(PlateId);
	const auto Slid = Dxf::ComputeSlideMove(World, Probe, {10, 4}, 0.01);
	if (Slid.Stop != Dxf::EWorldSlideStop::SlideCompleted || Slid.EndCenter.Y != 4 || Slid.EndCenter.X >= 4.5f)
	{
		return 123;
	}
	const auto Touching = Dxf::ComputeSlideMove(World, Toolbox::FCircle2D{{4.8f, 0}, 0.5f}, {10, 4}, 0.01);
	if (Touching.Stop != Dxf::EWorldSlideStop::InitialContact || Touching.EndCenter.X != 4.8f)
	{
		return 124;
	}
	return 0;
}
// 球の移動候補: 2Dと同じ配置をZ=0の平面に置く（奥行きの大きいOBB）。
static int Slide3D()
{
	Dxf::FPhysicsWorld3D World;
	const auto Level = World.CreateBody({});
	Dxf::FColliderDescription3D Wall;
	Wall.Shape = Toolbox::FOBB{{6, 0, 0}, {1, 50, 50}};
	World.AttachCollider(Level, Wall);
	Dxf::FColliderDescription3D Plate;
	Plate.Shape = Toolbox::FOBB{{4, 3, 0}, {0.9f, 0, 50}};
	const auto PlateId = World.AttachCollider(Level, Plate);
	const Toolbox::FSphere Probe{{0, 0, 0}, 0.5f};
	const auto Head = Dxf::ComputeSlideMove(World, Probe, {10, 0, 0}, 0.01);
	if (Head.Stop != Dxf::EWorldSlideStop::Blocked || Toolbox::Abs(Head.EndCenter.X - 4.49f) > 1e-5f)
	{
		return 131;
	}
	const auto Stopped = Dxf::ComputeSlideMove(World, Probe, {10, 4, 0}, 0.01);
	if (Stopped.Stop != Dxf::EWorldSlideStop::Blocked || !Stopped.SlideHit || Stopped.SlideHit->Collider != PlateId ||
	    Toolbox::Abs(Stopped.EndCenter.Y - 2.49f) > 1e-5f)
	{
		return 132;
	}
	World.DetachCollider(PlateId);
	const auto Slid = Dxf::ComputeSlideMove(World, Probe, {10, 4, 0}, 0.01);
	if (Slid.Stop != Dxf::EWorldSlideStop::SlideCompleted || Slid.EndCenter.Y != 4 || Slid.EndCenter.X >= 4.5f)
	{
		return 133;
	}
	const auto Touching = Dxf::ComputeSlideMove(World, Toolbox::FSphere{{4.8f, 0, 0}, 0.5f}, {10, 4, 0}, 0.01);
	if (Touching.Stop != Dxf::EWorldSlideStop::InitialContact || Touching.EndCenter.X != 4.8f)
	{
		return 134;
	}
	return 0;
}
// キャラクター移動: 上面y=0の床とx∈[5,7]の壁。床の上で接地、壁の手前で止まる（接触余裕0.02）。
// 240回の固定更新で右へ歩いて壁で止まり、最初の固定更新だけ跳ぶ。3Dは同じ配置をZ方向に厚みを持たせて置く。
template <typename TWorld, typename TSettings, typename TState, typename TInput, typename TVector, typename TShape>
int MoveCharacter(TWorld& World, TVector Start, TVector Right, TShape Probe, int Code)
{
	const TSettings Settings;
	const auto Ground = Dxf::ProbeCharacterGround(World, Start, Settings);
	if (Ground.State != Dxf::ECharacterGroundState::Walkable || !Ground.Collider)
	{
		return Code;
	}
	const auto Move = Dxf::MoveAndSlide(World, Start, Right * 10.0f, Settings);
	if (Move.Stop == Dxf::ECharacterMoveStop::Completed || Toolbox::Abs(Move.EndCenter.X - 4.48f) > 1e-4f ||
	    Toolbox::Abs(Move.EndCenter.Y - 0.52f) > 1e-4f)
	{
		return Code + 1;
	}
	TState State;
	State.Center = Start;
	State.Ground = Ground;
	TInput Input;
	Input.Move = Right;
	Input.bJump = true;
	int Jumps = 0;
	for (int Step = 0; Step < 240; ++Step)
	{
		const auto Result = Dxf::StepCharacter(World, Settings, State, Input, 1.0 / 60.0);
		Jumps += Result.bJumped ? 1 : 0;
		Input.bJump = false;
		State = Result.State;
	}
	if (Jumps != 1 || State.Ground.State != Dxf::ECharacterGroundState::Walkable ||
	    Toolbox::Abs(State.Center.X - 4.48f) > 1e-3f)
	{
		return Code + 2;
	}
	if (!World.QueryContacts(Probe, 0.1f).Count)
	{
		return Code + 3;
	}
	return 0;
}
static int CharacterMove2D()
{
	Dxf::FPhysicsWorld2D World;
	const auto Level = World.CreateBody({});
	Dxf::FColliderDescription2D Box;
	Box.Shape = Toolbox::FOrientedBox2D{{0, -1}, {50, 1}, 0};
	World.AttachCollider(Level, Box);
	Box.Shape = Toolbox::FOrientedBox2D{{6, 5}, {1, 5}, 0};
	World.AttachCollider(Level, Box);
	return MoveCharacter<Dxf::FPhysicsWorld2D, Dxf::FCharacterMoveSettings2D, Dxf::FCharacterState2D,
	                     Dxf::FCharacterMoveInput2D>(World, Toolbox::FVector2{0, 0.52f}, Toolbox::FVector2{1, 0},
	                                                 Toolbox::FCircle2D{{0, 0.52f}, 0.5f}, 141);
}
static int CharacterMove3D()
{
	Dxf::FPhysicsWorld3D World;
	const auto Level = World.CreateBody({});
	Dxf::FColliderDescription3D Box;
	Box.Shape = Toolbox::FOBB{{0, -1, 0}, {50, 1, 50}};
	World.AttachCollider(Level, Box);
	Box.Shape = Toolbox::FOBB{{6, 5, 0}, {1, 5, 50}};
	World.AttachCollider(Level, Box);
	return MoveCharacter<Dxf::FPhysicsWorld3D, Dxf::FCharacterMoveSettings3D, Dxf::FCharacterState3D,
	                     Dxf::FCharacterMoveInput3D>(World, Toolbox::FVector3{0, 0.52f, 0}, Toolbox::FVector3{1, 0, 0},
	                                                 Toolbox::FSphere{{0, 0.52f, 0}, 0.5f}, 151);
}
// 索引の診断: 一列に並べた多数のColliderへ短い射線を撃ち、詳細判定が近くの数件に限られ、総当たりへ切り替えないこと。
template <typename TWorld, typename TBody, typename TCollider, typename TShape, typename TPoint>
static int QueryIndex(TShape (*MakeShape)(float), TPoint (*MakePoint)(float), int Code)
{
	TWorld World;
	TBody Fixed;
	Fixed.Type = Dxf::EBodyType::Static;
	const auto Level = World.CreateBody(Fixed);
	for (int Index = 0; Index < 64; ++Index)
	{
		TCollider Description;
		Description.Shape = MakeShape(3.0f * static_cast<float>(Index));
		World.AttachCollider(Level, Description);
	}
	World.SetQueryDiagnosticsEnabled(true);
	World.ResetQueryDiagnostics();
	const auto Hit = World.RaycastClosest(MakePoint(-1.5f), MakePoint(1.5f));
	const Dxf::FWorldQueryDiagnostics Diagnostics = World.GetQueryDiagnostics();
	if (!Hit || Diagnostics.AliveColliders != 64 || Diagnostics.IndexedColliders != 64 ||
	    Diagnostics.Raycast.Queries != 1 || Diagnostics.Raycast.NarrowTests == 0 ||
	    Diagnostics.Raycast.NarrowTests > 4 || Diagnostics.Raycast.FallbackQueries != 0)
	{
		return Code;
	}
	return 0;
}
static Toolbox::FCircle2D Circle2DAt(float X)
{
	return {{X, 0}, 0.5f};
}
static Toolbox::FVector2 Point2DAt(float X)
{
	return {X, 0};
}
static Toolbox::FSphere SphereAt(float X)
{
	return {{X, 0, 0}, 0.5f};
}
static Toolbox::FVector3 Point3DAt(float X)
{
	return {X, 0, 0};
}
int main()
{
	Dxf::FPhysicsWorld3D World;
	const auto Body = World.CreateBody({});
	Dxf::FColliderDescription3D Description;
	Description.Shape = Toolbox::FSphere{{3, 0, 0}, 1};
	const auto Collider = World.AttachCollider(Body, Description);
	const auto Hit = World.RaycastClosest({0, 0, 0}, {10, 0, 0});
	if (!Hit || Hit->Collider != Collider || Toolbox::Abs(Hit->Fraction - .2) > 1e-12)
	{
		return 1;
	}
	if (World.RaycastClosest({0, 0, 0}, {10, 0, 0}, Body))
	{
		return 2;
	}
	World.DestroyBody(Body);
	if (World.IsColliderAlive(Hit->Collider) || World.RaycastClosest({0, 0, 0}, {10, 0, 0}))
	{
		return 3;
	}
	Dxf::FPhysicsWorld2D World2D;
	const auto Body2D = World2D.CreateBody({});
	Dxf::FColliderDescription2D Circle;
	Circle.Shape = Toolbox::FCircle2D{{0, 0}, 1};
	const auto Collider2D = World2D.AttachCollider(Body2D, Circle);
	Dxf::FBodyDescription2D WallBody;
	WallBody.Type = Dxf::EBodyType::Static;
	WallBody.Position = {5, 0};
	const auto Wall = World2D.CreateBody(WallBody);
	Dxf::FColliderDescription2D Box;
	Box.Shape = Toolbox::FOrientedBox2D{{0, 0}, {0.5f, 2}, 0};
	const auto WallCollider = World2D.AttachCollider(Wall, Box);
	const auto Hit2D = World2D.RaycastClosest({-2, 0}, {2, 0});
	if (!Hit2D || Hit2D->Collider != Collider2D || Toolbox::Abs(Hit2D->Fraction - .25) > 1e-12 ||
	    !(Hit2D->Position == Toolbox::FVector2(-1, 0)))
	{
		return 4;
	}
	const auto Other2D = World2D.RaycastClosest({0, 0}, {10, 0}, Body2D);
	if (!Other2D || Other2D->Collider != WallCollider || Toolbox::Abs(Other2D->Fraction - .45) > 1e-12)
	{
		return 5;
	}
	World2D.DestroyBody(Wall);
	if (World2D.IsColliderAlive(WallCollider) || World2D.RaycastClosest({0, 0}, {10, 0}, Body2D))
	{
		return 6;
	}
	// 問い合わせカテゴリ: 手前の不一致を飛ばし、自己除外と併用し、Stepなしの変更と失効を確認する。
	Dxf::FWorldQueryFilter Sight;
	Sight.IncludeCategories = 2u;
	Dxf::FBodyDescription2D Fixed2D;
	Fixed2D.Type = Dxf::EBodyType::Static;
	Dxf::FColliderDescription2D Pickup2D;
	Pickup2D.Shape = Toolbox::FCircle2D{{3, 0}, 0.5f};
	Pickup2D.QueryCategory = 4u;
	const auto PickupCollider2D = World2D.AttachCollider(World2D.CreateBody(Fixed2D), Pickup2D);
	Dxf::FColliderDescription2D Target2D;
	Target2D.Shape = Toolbox::FCircle2D{{6, 0}, 0.5f};
	Target2D.QueryCategory = 2u;
	const auto TargetCollider2D = World2D.AttachCollider(World2D.CreateBody(Fixed2D), Target2D);
	const auto Seen2D = World2D.RaycastClosest({0, 0}, {10, 0}, Body2D, Sight);
	if (!Seen2D || Seen2D->Collider != TargetCollider2D ||
	    World2D.RaycastClosest({0, 0}, {10, 0}, Body2D)->Collider != PickupCollider2D)
	{
		return 7;
	}
	World2D.SetColliderQueryCategory(TargetCollider2D, 0u);
	if (World2D.GetColliderQueryCategory(TargetCollider2D) != 0u || World2D.RaycastClosest({0, 0}, {10, 0}, {}, Sight))
	{
		return 8;
	}
	Dxf::FPhysicsWorld3D World3D;
	const auto Self3D = World3D.CreateBody({});
	Dxf::FColliderDescription3D Near3D;
	Near3D.Shape = Toolbox::FSphere{{3, 0, 0}, 0.5f};
	Near3D.QueryCategory = 4u;
	World3D.AttachCollider(Self3D, Near3D);
	Dxf::FColliderDescription3D Far3D;
	Far3D.Shape = Toolbox::FSphere{{6, 0, 0}, 0.5f};
	Far3D.QueryCategory = 2u;
	const auto FarBody3D = World3D.CreateBody({});
	const auto FarCollider3D = World3D.AttachCollider(FarBody3D, Far3D);
	const auto Seen3D = World3D.RaycastClosest({0, 0, 0}, {10, 0, 0}, {}, Sight);
	if (!Seen3D || Seen3D->Collider != FarCollider3D || Toolbox::Abs(Seen3D->Fraction - .55) > 1e-12)
	{
		return 9;
	}
	if (World3D.RaycastClosest({0, 0, 0}, {10, 0, 0}, FarBody3D, Sight))
	{
		return 10;
	}
	World3D.SetColliderQueryCategory(FarCollider3D, 6u);
	World3D.DestroyBody(FarBody3D);
	if (World3D.IsColliderAlive(FarCollider3D) || World3D.RaycastClosest({0, 0, 0}, {10, 0, 0}, {}, Sight))
	{
		return 11;
	}
	// 範囲問い合わせ: 候補を集め、障害物込みの射線で遮られていない候補を選ぶ（2D）。
	Dxf::FWorldQueryFilter Characters;
	Characters.IncludeCategories = 2u;
	Dxf::FWorldQueryFilter LineOfSight;
	LineOfSight.IncludeCategories = 1u | 2u;
	Dxf::FPhysicsWorld2D Area2D;
	Dxf::FBodyDescription2D At2D;
	At2D.Type = Dxf::EBodyType::Static;
	const auto Eye2D = Area2D.CreateBody(At2D);
	Dxf::FColliderDescription2D Character2D;
	Character2D.Shape = Toolbox::FCircle2D{{0, 0}, 0.5f};
	Character2D.QueryCategory = 2u;
	Area2D.AttachCollider(Eye2D, Character2D);
	At2D.Position = {0, 4};
	const auto Visible2D = Area2D.CreateBody(At2D);
	const auto VisibleCollider2D = Area2D.AttachCollider(Visible2D, Character2D);
	At2D.Position = {8, 0};
	const auto Hidden2D = Area2D.CreateBody(At2D);
	const auto HiddenCollider2D = Area2D.AttachCollider(Hidden2D, Character2D);
	At2D.Position = {5, 0};
	const auto WallBody2D = Area2D.CreateBody(At2D);
	Dxf::FColliderDescription2D Wall2D;
	Wall2D.Shape = Toolbox::FOrientedBox2D{{0, 0}, {0.5f, 2}, 0.3f};
	Wall2D.QueryCategory = 1u;
	const auto WallCollider2D = Area2D.AttachCollider(WallBody2D, Wall2D);
	const auto Candidates2D = Area2D.OverlapAll(Toolbox::FCircle2D{{0, 0}, 10}, Eye2D, Characters);
	if (Candidates2D.Size() != 2 || Candidates2D[0] != VisibleCollider2D || Candidates2D[1] != HiddenCollider2D)
	{
		return 12;
	}
	const auto See2D = Area2D.RaycastClosest({0, 0}, Area2D.GetPosition(Visible2D), Eye2D, LineOfSight);
	const auto Block2D = Area2D.RaycastClosest({0, 0}, Area2D.GetPosition(Hidden2D), Eye2D, LineOfSight);
	if (!See2D || See2D->Collider.Body != Visible2D || !Block2D || Block2D->Collider != WallCollider2D)
	{
		return 13;
	}
	if (Area2D.OverlapAll(Toolbox::FCircle2D{{5, 0}, 0.1f}).Size() != 1)
	{
		return 14;
	}
	Area2D.SetColliderQueryCategory(WallCollider2D, 0u);
	Area2D.SetBodyTransform(Visible2D, {0, 20}, 0);
	const auto After2D = Area2D.OverlapAll(Toolbox::FCircle2D{{0, 0}, 10}, Eye2D, Characters);
	if (After2D.Size() != 1 || After2D[0] != HiddenCollider2D ||
	    !Area2D.OverlapAll(Toolbox::FCircle2D{{5, 0}, 0.1f}).IsEmpty())
	{
		return 15;
	}
	Area2D.DestroyBody(Hidden2D);
	const auto Reused2D = Area2D.CreateBody(At2D);
	if (Reused2D.Index != Hidden2D.Index || Area2D.IsColliderAlive(HiddenCollider2D) ||
	    !Area2D.OverlapAll(Toolbox::FCircle2D{{0, 0}, 10}, Eye2D, Characters).IsEmpty())
	{
		return 16;
	}
	// 同じ流れを3Dで（回転OBBの壁）。
	Dxf::FPhysicsWorld3D Area3D;
	Dxf::FBodyDescription3D At3D;
	At3D.Type = Dxf::EBodyType::Static;
	const auto Eye3D = Area3D.CreateBody(At3D);
	Dxf::FColliderDescription3D Character3D;
	Character3D.Shape = Toolbox::FSphere{{0, 0, 0}, 0.5f};
	Character3D.QueryCategory = 2u;
	Area3D.AttachCollider(Eye3D, Character3D);
	At3D.Position = {0, 4, 0};
	const auto Visible3D = Area3D.CreateBody(At3D);
	const auto VisibleCollider3D = Area3D.AttachCollider(Visible3D, Character3D);
	At3D.Position = {8, 0, 0};
	const auto Hidden3D = Area3D.CreateBody(At3D);
	const auto HiddenCollider3D = Area3D.AttachCollider(Hidden3D, Character3D);
	At3D.Position = {5, 0, 0};
	At3D.Orientation = Toolbox::FQuaternion::FromAxisAngle({0, 1, 0}, 0.3f);
	const auto WallBody3D = Area3D.CreateBody(At3D);
	Dxf::FColliderDescription3D Wall3D;
	Wall3D.Shape = Toolbox::FOBB{{0, 0, 0}, {0.5f, 2, 2}};
	Wall3D.QueryCategory = 1u;
	const auto WallCollider3D = Area3D.AttachCollider(WallBody3D, Wall3D);
	const auto Candidates3D = Area3D.OverlapAll(Toolbox::FSphere{{0, 0, 0}, 10}, Eye3D, Characters);
	if (Candidates3D.Size() != 2 || Candidates3D[0] != VisibleCollider3D || Candidates3D[1] != HiddenCollider3D)
	{
		return 17;
	}
	const auto See3D = Area3D.RaycastClosest({0, 0, 0}, Area3D.GetPosition(Visible3D), Eye3D, LineOfSight);
	const auto Block3D = Area3D.RaycastClosest({0, 0, 0}, Area3D.GetPosition(Hidden3D), Eye3D, LineOfSight);
	if (!See3D || See3D->Collider.Body != Visible3D || !Block3D || Block3D->Collider != WallCollider3D)
	{
		return 18;
	}
	Area3D.SetColliderQueryCategory(WallCollider3D, 0u);
	const auto Open3D = Area3D.RaycastClosest({0, 0, 0}, Area3D.GetPosition(Hidden3D), Eye3D, LineOfSight);
	if (!Open3D || Open3D->Collider != HiddenCollider3D)
	{
		return 19;
	}
	Area3D.DestroyBody(Hidden3D);
	if (Area3D.IsColliderAlive(HiddenCollider3D) ||
	    Area3D.OverlapAll(Toolbox::FSphere{{0, 0, 0}, 10}, Eye3D, Characters).Size() != 1)
	{
		return 20;
	}
	const int Sweep2DCode = Sweep2D();
	if (Sweep2DCode != 0)
	{
		return Sweep2DCode;
	}
	const int Sweep3DCode = Sweep3D();
	if (Sweep3DCode != 0)
	{
		return Sweep3DCode;
	}
	const int Slide2DCode = Slide2D();
	if (Slide2DCode != 0)
	{
		return Slide2DCode;
	}
	const int Slide3DCode = Slide3D();
	if (Slide3DCode != 0)
	{
		return Slide3DCode;
	}
	const int Character2DCode = CharacterMove2D();
	if (Character2DCode != 0)
	{
		return Character2DCode;
	}
	const int Character3DCode = CharacterMove3D();
	if (Character3DCode != 0)
	{
		return Character3DCode;
	}
	const int Index2DCode = QueryIndex<Dxf::FPhysicsWorld2D, Dxf::FBodyDescription2D, Dxf::FColliderDescription2D>(
	    Circle2DAt, Point2DAt, 40);
	if (Index2DCode != 0)
	{
		return Index2DCode;
	}
	const int Index3DCode =
	    QueryIndex<Dxf::FPhysicsWorld3D, Dxf::FBodyDescription3D, Dxf::FColliderDescription3D>(SphereAt, Point3DAt, 41);
	if (Index3DCode != 0)
	{
		return Index3DCode;
	}
	return 0;
}
