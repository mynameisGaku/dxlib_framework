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
