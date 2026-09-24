"""Build, install, relocate, and consume the package without SDK/network access."""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from ValidationSupport import project_version, run_logged, validation_report

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, default=ROOT / 'Build' / 'PackageValidation')
    parser.add_argument('--logs', type=Path, default=ROOT / 'Docs' / 'Validation' / 'Package')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    args.work.mkdir(parents=True, exist_ok=True)
    args.logs.mkdir(parents=True, exist_ok=True)
    # Each run is clean; never accept an old install as evidence for a failed build.
    work = Path(tempfile.mkdtemp(prefix='run-', dir=args.work.resolve()))

    summary: dict[str, object] = {'real_dxlib_sdk': False}

    def run(name: str, command: list[str]) -> None:
        run_logged(args.logs, name, command, cwd=ROOT, timeout=240)

    with validation_report(args.logs, summary):
        summary['version'] = project_version(ROOT)
        run('build-configure', ['cmake', '-S', str(ROOT), '-B', str(work / 'Build'), '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Debug', '-DDXF_BUILD_TESTS=OFF', '-DDXF_BUILD_NATIVE=OFF', '-DDXF_INSTALL=ON'])
        run('build', ['cmake', '--build', str(work / 'Build'), '--parallel', str(args.jobs)])
        run('install', ['cmake', '--install', str(work / 'Build'), '--prefix', str(work / 'Original')])
        relocated = work / 'Relocated package'
        shutil.move(str(work / 'Original'), str(relocated))
        consumer = work / 'Consumer'
        consumer.mkdir()
        (consumer / 'Main.cpp').write_text('''#include "Dxf/GameScene.h"
#include "Dxf/PhysicsDebugPicking3D.h"
#include "Dxf/RenderQueue2D.h"
int main()
{
    Dxf::FPhysicsWorld3D World;
    const auto Body=World.CreateBody({});
    Dxf::FColliderDescription3D Collider;
    Collider.Shape=Toolbox::FSphere{{0,0,0},1};
    const auto Id=World.AttachCollider(Body,Collider);
    const auto Snapshot=Dxf::CapturePhysicsDebugSnapshot3D(World,0);
    if (!Snapshot) { return 6; }
    const auto Pick=Dxf::PickPhysicsDebugSnapshot3D(Snapshot.Value(),{{0,0,-5},{0,0,5}});
    if (!Pick || !Pick.Value() || Pick.Value()->Collider!=Id) { return 7; }
    Dxf::DGameScene Scene;
    Scene.Shutdown_Internal();
    Dxf::FRenderQueue2D Queue;
    return Queue.Submit(Dxf::FRectangleCommand{}) ? 1 : 0;
}
''', encoding='utf-8')
        (consumer / 'Support.cpp').write_text('''#include "Dxf/RenderQueue2D.h"
#include "Dxf/ModelImport.h"
#include "Dxf/ViewCoordinates.h"
#include "Toolbox/SegmentIntersection.h"
int main()
{
    if (Dxf::ImportFbxModel(nullptr, 0)) { return 2; }
    Dxf::FRenderView3D View;
    const auto Point = Dxf::ProjectWorldToScreen(View,640,480,{0,0,0});
    const auto Ray = Dxf::MakeViewPickSegment(View,640,480,{320,240});
    if (!Point || !Point.Value().bInsideView || Point.Value().Screen.X != 320 || !Ray || !Ray.Value()) { return 3; }
    if (!Toolbox::IntersectSegment(Ray.Value()->Start,Ray.Value()->End,Toolbox::FSphere{{0,0,0},1})) { return 4; }
    if (!Toolbox::IntersectSegment(Ray.Value()->Start,Ray.Value()->End,Toolbox::FOBB{{0,0,0},{1,1,1}})) { return 5; }
    Dxf::FRenderQueue2D Queue;
    return Queue.Submit(Dxf::FRectangleCommand{}) ? 1 : 0;
}
''', encoding='utf-8')
        (consumer / 'Physics.cpp').write_text('''#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Dxf/WorldSlideMove2D.h"
#include "Dxf/WorldSlideMove3D.h"
// 円スイープ: 中心線の射線は外れ、半径のある移動は当たる。除外・カテゴリ・静止・削除後の失効。
// 接触法線: 壁の左下の角(4.5,0.4)から中心(4.2,0)へ向く(-0.6,-0.8)。初期接触・半径0では空。
static int Sweep2D()
{
    Dxf::FPhysicsWorld2D World;
    Dxf::FBodyDescription2D Fixed;
    Fixed.Type = Dxf::EBodyType::Static;
    const auto Self = World.CreateBody(Fixed);
    Dxf::FColliderDescription2D Mine;
    Mine.Shape = Toolbox::FCircle2D{{0,0},0.5f};
    World.AttachCollider(Self, Mine);
    Fixed.Position = {5,0.9f};
    const auto WallBody = World.CreateBody(Fixed);
    Dxf::FColliderDescription2D Wall;
    Wall.Shape = Toolbox::FOrientedBox2D{{0,0},{0.5f,0.5f},0};
    Wall.QueryCategory = 1u;
    const auto WallId = World.AttachCollider(WallBody, Wall);
    Fixed.Position = {3,-0.3f};
    Dxf::FColliderDescription2D Pickup;
    Pickup.Shape = Toolbox::FCircle2D{{0,0},0.2f};
    Pickup.QueryCategory = 4u;
    World.AttachCollider(World.CreateBody(Fixed), Pickup);
    Dxf::FWorldQueryFilter Obstacles;
    Obstacles.IncludeCategories = 1u;
    const Toolbox::FCircle2D Probe{{0,0},0.5f};
    if (World.RaycastClosest({0,0},{10,0},Self,Obstacles)) { return 101; }
    const auto Hit = World.SweepClosest(Probe, {10,0}, Self, Obstacles);
    if (!Hit || Hit->Collider != WallId || Hit->bInitialContact || Toolbox::Abs(Hit->CenterAtHit.X - 4.2f) > 1e-4f || Hit->CenterAtHit.Y != 0) { return 102; }
    const auto Any = World.SweepClosest(Probe, {10,0});
    if (!Any || !Any->bInitialContact || Any->Fraction != 0 || Any->Collider.Body != Self) { return 103; }
    if (!Hit->Normal || Toolbox::Abs(Hit->Normal->X + 0.6f) > 1e-5f || Toolbox::Abs(Hit->Normal->Y + 0.8f) > 1e-5f || Any->Normal) { return 107; }
    const auto Ray = World.RaycastClosest({0,0.9f},{10,0.9f},Self,Obstacles);
    const auto Thin = World.SweepClosest(Toolbox::FCircle2D{{0,0.9f},0}, {10,0.9f}, Self, Obstacles);
    if (!Ray || !Thin || Thin->Fraction != Ray->Fraction || Thin->Normal) { return 108; }
    World.SetColliderQueryCategory(WallId, 0u);
    if (World.SweepClosest(Probe, {10,0}, Self, Obstacles)) { return 104; }
    World.SetColliderQueryCategory(WallId, 1u);
    const auto Still = World.SweepClosest(Toolbox::FCircle2D{{5,0.9f},0.1f}, {5,0.9f}, Self, Obstacles);
    if (!Still || !Still->bInitialContact || Still->Fraction != 0 || Still->Collider != WallId || Still->Normal) { return 105; }
    World.DestroyBody(WallBody);
    if (World.IsColliderAlive(Hit->Collider) || World.SweepClosest(Probe, {10,0}, Self, Obstacles)) { return 106; }
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
    Mine.Shape = Toolbox::FSphere{{0,0,0},0.5f};
    World.AttachCollider(Self, Mine);
    Fixed.Position = {5,0.9f,0};
    Fixed.Orientation = Toolbox::FQuaternion::FromAxisAngle({1,0,0}, 0.4f);
    const auto WallBody = World.CreateBody(Fixed);
    Dxf::FColliderDescription3D Wall;
    Wall.Shape = Toolbox::FOBB{{0,0,0},{0.5f,0.5f,0.5f}};
    Wall.QueryCategory = 1u;
    const auto WallId = World.AttachCollider(WallBody, Wall);
    Dxf::FWorldQueryFilter Obstacles;
    Obstacles.IncludeCategories = 1u;
    const Toolbox::FSphere Probe{{0,0,0},0.5f};
    if (World.RaycastClosest({0,0,0},{10,0,0},Self,Obstacles)) { return 111; }
    const auto Hit = World.SweepClosest(Probe, {10,0,0}, Self, Obstacles);
    if (!Hit || Hit->Collider != WallId || Hit->bInitialContact || Hit->Fraction <= 0 || Hit->Fraction >= 1 || Hit->CenterAtHit.Y != 0 || Hit->CenterAtHit.Z != 0) { return 112; }
    if (!Hit->Normal || !(Hit->Normal->X < 0)) { return 116; }
    const Toolbox::f32 LengthSquared = Hit->Normal->X * Hit->Normal->X + Hit->Normal->Y * Hit->Normal->Y + Hit->Normal->Z * Hit->Normal->Z;
    if (Toolbox::Abs(LengthSquared - 1.0f) > 1e-5f) { return 117; }
    World.SetColliderQueryCategory(WallId, 0u);
    if (World.SweepClosest(Probe, {10,0,0}, Self, Obstacles)) { return 113; }
    World.SetColliderQueryCategory(WallId, 1u);
    const auto Still = World.SweepClosest(Toolbox::FSphere{{5,0.9f,0},0.1f}, {5,0.9f,0}, Self, Obstacles);
    if (!Still || !Still->bInitialContact || Still->Collider != WallId || Still->Normal) { return 114; }
    World.DestroyBody(WallBody);
    if (World.IsColliderAlive(Hit->Collider) || World.SweepClosest(Probe, {10,0,0}, Self, Obstacles)) { return 115; }
    return 0;
}
// 円の移動候補: 正面の壁で停止、斜めに当たって1回滑り途中の板で停止、板を外すと滑り切る、初期接触で停止。
static int Slide2D()
{
    Dxf::FPhysicsWorld2D World;
    const auto Level = World.CreateBody({});
    Dxf::FColliderDescription2D Wall;
    Wall.Shape = Toolbox::FOrientedBox2D{{6,0},{1,50},0};
    World.AttachCollider(Level, Wall);
    Dxf::FColliderDescription2D Plate;
    Plate.Shape = Toolbox::FOrientedBox2D{{4,3},{0.9f,0},0};
    const auto PlateId = World.AttachCollider(Level, Plate);
    const Toolbox::FCircle2D Probe{{0,0},0.5f};
    const auto Head = Dxf::ComputeSlideMove(World, Probe, {10,0}, 0.01);
    if (Head.Stop != Dxf::EWorldSlideStop::Blocked || Toolbox::Abs(Head.EndCenter.X - 4.49f) > 1e-5f) { return 121; }
    const auto Stopped = Dxf::ComputeSlideMove(World, Probe, {10,4}, 0.01);
    if (Stopped.Stop != Dxf::EWorldSlideStop::Blocked || !Stopped.SlideHit || Stopped.SlideHit->Collider != PlateId || Toolbox::Abs(Stopped.EndCenter.Y - 2.49f) > 1e-5f) { return 122; }
    World.DetachCollider(PlateId);
    const auto Slid = Dxf::ComputeSlideMove(World, Probe, {10,4}, 0.01);
    if (Slid.Stop != Dxf::EWorldSlideStop::SlideCompleted || Slid.EndCenter.Y != 4 || Slid.EndCenter.X >= 4.5f) { return 123; }
    const auto Touching = Dxf::ComputeSlideMove(World, Toolbox::FCircle2D{{4.8f,0},0.5f}, {10,4}, 0.01);
    if (Touching.Stop != Dxf::EWorldSlideStop::InitialContact || Touching.EndCenter.X != 4.8f) { return 124; }
    return 0;
}
// 球の移動候補: 2Dと同じ配置をZ=0の平面に置く（奥行きの大きいOBB）。
static int Slide3D()
{
    Dxf::FPhysicsWorld3D World;
    const auto Level = World.CreateBody({});
    Dxf::FColliderDescription3D Wall;
    Wall.Shape = Toolbox::FOBB{{6,0,0},{1,50,50}};
    World.AttachCollider(Level, Wall);
    Dxf::FColliderDescription3D Plate;
    Plate.Shape = Toolbox::FOBB{{4,3,0},{0.9f,0,50}};
    const auto PlateId = World.AttachCollider(Level, Plate);
    const Toolbox::FSphere Probe{{0,0,0},0.5f};
    const auto Head = Dxf::ComputeSlideMove(World, Probe, {10,0,0}, 0.01);
    if (Head.Stop != Dxf::EWorldSlideStop::Blocked || Toolbox::Abs(Head.EndCenter.X - 4.49f) > 1e-5f) { return 131; }
    const auto Stopped = Dxf::ComputeSlideMove(World, Probe, {10,4,0}, 0.01);
    if (Stopped.Stop != Dxf::EWorldSlideStop::Blocked || !Stopped.SlideHit || Stopped.SlideHit->Collider != PlateId || Toolbox::Abs(Stopped.EndCenter.Y - 2.49f) > 1e-5f) { return 132; }
    World.DetachCollider(PlateId);
    const auto Slid = Dxf::ComputeSlideMove(World, Probe, {10,4,0}, 0.01);
    if (Slid.Stop != Dxf::EWorldSlideStop::SlideCompleted || Slid.EndCenter.Y != 4 || Slid.EndCenter.X >= 4.5f) { return 133; }
    const auto Touching = Dxf::ComputeSlideMove(World, Toolbox::FSphere{{4.8f,0,0},0.5f}, {10,4,0}, 0.01);
    if (Touching.Stop != Dxf::EWorldSlideStop::InitialContact || Touching.EndCenter.X != 4.8f) { return 134; }
    return 0;
}
int main()
{
    Dxf::FPhysicsWorld3D World;
    const auto Body = World.CreateBody({});
    Dxf::FColliderDescription3D Description;
    Description.Shape = Toolbox::FSphere{{3,0,0},1};
    const auto Collider = World.AttachCollider(Body, Description);
    const auto Hit = World.RaycastClosest({0,0,0},{10,0,0});
    if (!Hit || Hit->Collider != Collider || Toolbox::Abs(Hit->Fraction-.2)>1e-12) { return 1; }
    if (World.RaycastClosest({0,0,0},{10,0,0},Body)) { return 2; }
    World.DestroyBody(Body);
    if (World.IsColliderAlive(Hit->Collider) || World.RaycastClosest({0,0,0},{10,0,0})) { return 3; }
    Dxf::FPhysicsWorld2D World2D;
    const auto Body2D = World2D.CreateBody({});
    Dxf::FColliderDescription2D Circle;
    Circle.Shape = Toolbox::FCircle2D{{0,0},1};
    const auto Collider2D = World2D.AttachCollider(Body2D, Circle);
    Dxf::FBodyDescription2D WallBody;
    WallBody.Type = Dxf::EBodyType::Static;
    WallBody.Position = {5,0};
    const auto Wall = World2D.CreateBody(WallBody);
    Dxf::FColliderDescription2D Box;
    Box.Shape = Toolbox::FOrientedBox2D{{0,0},{0.5f,2},0};
    const auto WallCollider = World2D.AttachCollider(Wall, Box);
    const auto Hit2D = World2D.RaycastClosest({-2,0},{2,0});
    if (!Hit2D || Hit2D->Collider != Collider2D || Toolbox::Abs(Hit2D->Fraction-.25)>1e-12 || !(Hit2D->Position == Toolbox::FVector2(-1,0))) { return 4; }
    const auto Other2D = World2D.RaycastClosest({0,0},{10,0},Body2D);
    if (!Other2D || Other2D->Collider != WallCollider || Toolbox::Abs(Other2D->Fraction-.45)>1e-12) { return 5; }
    World2D.DestroyBody(Wall);
    if (World2D.IsColliderAlive(WallCollider) || World2D.RaycastClosest({0,0},{10,0},Body2D)) { return 6; }
    // 問い合わせカテゴリ: 手前の不一致を飛ばし、自己除外と併用し、Stepなしの変更と失効を確認する。
    Dxf::FWorldQueryFilter Sight;
    Sight.IncludeCategories = 2u;
    Dxf::FBodyDescription2D Fixed2D;
    Fixed2D.Type = Dxf::EBodyType::Static;
    Dxf::FColliderDescription2D Pickup2D;
    Pickup2D.Shape = Toolbox::FCircle2D{{3,0},0.5f};
    Pickup2D.QueryCategory = 4u;
    const auto PickupCollider2D = World2D.AttachCollider(World2D.CreateBody(Fixed2D), Pickup2D);
    Dxf::FColliderDescription2D Target2D;
    Target2D.Shape = Toolbox::FCircle2D{{6,0},0.5f};
    Target2D.QueryCategory = 2u;
    const auto TargetCollider2D = World2D.AttachCollider(World2D.CreateBody(Fixed2D), Target2D);
    const auto Seen2D = World2D.RaycastClosest({0,0},{10,0},Body2D,Sight);
    if (!Seen2D || Seen2D->Collider != TargetCollider2D || World2D.RaycastClosest({0,0},{10,0},Body2D)->Collider != PickupCollider2D) { return 7; }
    World2D.SetColliderQueryCategory(TargetCollider2D, 0u);
    if (World2D.GetColliderQueryCategory(TargetCollider2D) != 0u || World2D.RaycastClosest({0,0},{10,0},{},Sight)) { return 8; }
    Dxf::FPhysicsWorld3D World3D;
    const auto Self3D = World3D.CreateBody({});
    Dxf::FColliderDescription3D Near3D;
    Near3D.Shape = Toolbox::FSphere{{3,0,0},0.5f};
    Near3D.QueryCategory = 4u;
    World3D.AttachCollider(Self3D, Near3D);
    Dxf::FColliderDescription3D Far3D;
    Far3D.Shape = Toolbox::FSphere{{6,0,0},0.5f};
    Far3D.QueryCategory = 2u;
    const auto FarBody3D = World3D.CreateBody({});
    const auto FarCollider3D = World3D.AttachCollider(FarBody3D, Far3D);
    const auto Seen3D = World3D.RaycastClosest({0,0,0},{10,0,0},{},Sight);
    if (!Seen3D || Seen3D->Collider != FarCollider3D || Toolbox::Abs(Seen3D->Fraction-.55)>1e-12) { return 9; }
    if (World3D.RaycastClosest({0,0,0},{10,0,0},FarBody3D,Sight)) { return 10; }
    World3D.SetColliderQueryCategory(FarCollider3D, 6u);
    World3D.DestroyBody(FarBody3D);
    if (World3D.IsColliderAlive(FarCollider3D) || World3D.RaycastClosest({0,0,0},{10,0,0},{},Sight)) { return 11; }
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
    Character2D.Shape = Toolbox::FCircle2D{{0,0},0.5f};
    Character2D.QueryCategory = 2u;
    Area2D.AttachCollider(Eye2D, Character2D);
    At2D.Position = {0,4};
    const auto Visible2D = Area2D.CreateBody(At2D);
    const auto VisibleCollider2D = Area2D.AttachCollider(Visible2D, Character2D);
    At2D.Position = {8,0};
    const auto Hidden2D = Area2D.CreateBody(At2D);
    const auto HiddenCollider2D = Area2D.AttachCollider(Hidden2D, Character2D);
    At2D.Position = {5,0};
    const auto WallBody2D = Area2D.CreateBody(At2D);
    Dxf::FColliderDescription2D Wall2D;
    Wall2D.Shape = Toolbox::FOrientedBox2D{{0,0},{0.5f,2},0.3f};
    Wall2D.QueryCategory = 1u;
    const auto WallCollider2D = Area2D.AttachCollider(WallBody2D, Wall2D);
    const auto Candidates2D = Area2D.OverlapAll(Toolbox::FCircle2D{{0,0},10}, Eye2D, Characters);
    if (Candidates2D.Size() != 2 || Candidates2D[0] != VisibleCollider2D || Candidates2D[1] != HiddenCollider2D) { return 12; }
    const auto See2D = Area2D.RaycastClosest({0,0}, Area2D.GetPosition(Visible2D), Eye2D, LineOfSight);
    const auto Block2D = Area2D.RaycastClosest({0,0}, Area2D.GetPosition(Hidden2D), Eye2D, LineOfSight);
    if (!See2D || See2D->Collider.Body != Visible2D || !Block2D || Block2D->Collider != WallCollider2D) { return 13; }
    if (Area2D.OverlapAll(Toolbox::FCircle2D{{5,0},0.1f}).Size() != 1) { return 14; }
    Area2D.SetColliderQueryCategory(WallCollider2D, 0u);
    Area2D.SetBodyTransform(Visible2D, {0,20}, 0);
    const auto After2D = Area2D.OverlapAll(Toolbox::FCircle2D{{0,0},10}, Eye2D, Characters);
    if (After2D.Size() != 1 || After2D[0] != HiddenCollider2D || !Area2D.OverlapAll(Toolbox::FCircle2D{{5,0},0.1f}).IsEmpty()) { return 15; }
    Area2D.DestroyBody(Hidden2D);
    const auto Reused2D = Area2D.CreateBody(At2D);
    if (Reused2D.Index != Hidden2D.Index || Area2D.IsColliderAlive(HiddenCollider2D) || !Area2D.OverlapAll(Toolbox::FCircle2D{{0,0},10}, Eye2D, Characters).IsEmpty()) { return 16; }
    // 同じ流れを3Dで（回転OBBの壁）。
    Dxf::FPhysicsWorld3D Area3D;
    Dxf::FBodyDescription3D At3D;
    At3D.Type = Dxf::EBodyType::Static;
    const auto Eye3D = Area3D.CreateBody(At3D);
    Dxf::FColliderDescription3D Character3D;
    Character3D.Shape = Toolbox::FSphere{{0,0,0},0.5f};
    Character3D.QueryCategory = 2u;
    Area3D.AttachCollider(Eye3D, Character3D);
    At3D.Position = {0,4,0};
    const auto Visible3D = Area3D.CreateBody(At3D);
    const auto VisibleCollider3D = Area3D.AttachCollider(Visible3D, Character3D);
    At3D.Position = {8,0,0};
    const auto Hidden3D = Area3D.CreateBody(At3D);
    const auto HiddenCollider3D = Area3D.AttachCollider(Hidden3D, Character3D);
    At3D.Position = {5,0,0};
    At3D.Orientation = Toolbox::FQuaternion::FromAxisAngle({0,1,0}, 0.3f);
    const auto WallBody3D = Area3D.CreateBody(At3D);
    Dxf::FColliderDescription3D Wall3D;
    Wall3D.Shape = Toolbox::FOBB{{0,0,0},{0.5f,2,2}};
    Wall3D.QueryCategory = 1u;
    const auto WallCollider3D = Area3D.AttachCollider(WallBody3D, Wall3D);
    const auto Candidates3D = Area3D.OverlapAll(Toolbox::FSphere{{0,0,0},10}, Eye3D, Characters);
    if (Candidates3D.Size() != 2 || Candidates3D[0] != VisibleCollider3D || Candidates3D[1] != HiddenCollider3D) { return 17; }
    const auto See3D = Area3D.RaycastClosest({0,0,0}, Area3D.GetPosition(Visible3D), Eye3D, LineOfSight);
    const auto Block3D = Area3D.RaycastClosest({0,0,0}, Area3D.GetPosition(Hidden3D), Eye3D, LineOfSight);
    if (!See3D || See3D->Collider.Body != Visible3D || !Block3D || Block3D->Collider != WallCollider3D) { return 18; }
    Area3D.SetColliderQueryCategory(WallCollider3D, 0u);
    const auto Open3D = Area3D.RaycastClosest({0,0,0}, Area3D.GetPosition(Hidden3D), Eye3D, LineOfSight);
    if (!Open3D || Open3D->Collider != HiddenCollider3D) { return 19; }
    Area3D.DestroyBody(Hidden3D);
    if (Area3D.IsColliderAlive(HiddenCollider3D) || Area3D.OverlapAll(Toolbox::FSphere{{0,0,0},10}, Eye3D, Characters).Size() != 1) { return 20; }
    const int Sweep2DCode = Sweep2D();
    if (Sweep2DCode != 0) { return Sweep2DCode; }
    const int Sweep3DCode = Sweep3D();
    if (Sweep3DCode != 0) { return Sweep3DCode; }
    const int Slide2DCode = Slide2D();
    if (Slide2DCode != 0) { return Slide2DCode; }
    const int Slide3DCode = Slide3D();
    if (Slide3DCode != 0) { return Slide3DCode; }
    return 0;
}
''', encoding='utf-8')
        (consumer / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(RelocatedConsumer LANGUAGES CXX)
find_package(dxlib_framework CONFIG REQUIRED)
add_executable(Consumer Main.cpp)
target_link_libraries(Consumer PRIVATE dxf::framework dxf::debug_tools)
add_executable(PhysicsOnly Physics.cpp)
target_link_libraries(PhysicsOnly PRIVATE dxf::physics)
add_executable(SupportOnly Support.cpp)
target_link_libraries(SupportOnly PRIVATE dxf::support)
if(NOT TARGET dxf::toolbox OR NOT TARGET dxf::foundation OR NOT TARGET dxf::support OR NOT TARGET dxf::runtime OR NOT TARGET dxf::gameplay)
    message(FATAL_ERROR "Layer targets are missing from the installed package")
endif()
''', encoding='utf-8')
        run('consumer-configure', ['cmake', '-S', str(consumer), '-B', str(work / 'ConsumerBuild'), '-G',
            'Ninja', '-DCMAKE_BUILD_TYPE=Debug', f'-DCMAKE_PREFIX_PATH={relocated.as_posix()}'])
        run('consumer-build', ['cmake', '--build', str(work / 'ConsumerBuild'), '--parallel', str(args.jobs)])
        suffix = '.exe' if sys.platform == 'win32' else ''
        run('consumer-run', [str(work / 'ConsumerBuild' / ('Consumer' + suffix))])
        run('support-only-run', [str(work / 'ConsumerBuild' / ('SupportOnly' + suffix))])
        run('physics-only-run', [str(work / 'ConsumerBuild' / ('PhysicsOnly' + suffix))])
        summary.update(install=True, relocation=True, external_consumer=True,
                       support_without_runtime=True, physics_without_debug_or_support=True)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
