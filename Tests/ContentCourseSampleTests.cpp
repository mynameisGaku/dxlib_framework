// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/ContentRenderBackend.h"
#include "Support/ContentModelBackend.h"
#include "Dxf/Application.h"
#include "Dxf/SceneContentSource.h"
#include "ContentCourseScene2D.h"
#include "ContentCourseScene3D.h"
namespace
{
// 既存Sampleを実Applicationで動かす。Native境界だけを記録用へ替える。
template <typename TScene>
auto RunContentCourse(bool Split)
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::Testing::FContentRenderBackend Renderer(Backend);
	Dxf::Testing::FContentModelBackend Models;
	Dxf::FApplicationSettings Settings;
	Settings.ProjectRoot = DXF_CONTENT_SAMPLE_ROOT;
	Settings.Window.Width = 1001;
	Settings.Window.Height = 501;
	Settings.ExecutionThreadCount = 1;
	Backend.GetTrace().Window.bKnown = true;
	Backend.GetTrace().Window.RenderWidth = 1001;
	Backend.GetTrace().Window.RenderHeight = 501;
	Dxf::FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer, &Models}, Settings);
	REQUIRE(App.Start(Toolbox::MakeUnique<Dxf::DScene>()));
	const auto EmptyFrame = App.Step(0);
	REQUIRE(EmptyFrame && EmptyFrame.Value());
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_SAMPLE_ROOT)};
	const auto Prepared = [&]()
	{
		if constexpr (Toolbox::IsSame<TScene, Dxf::GameplaySample::DContentCourse2DScene>)
		{
			return Dxf::PrepareScene(Source.LoadScene2D("Assets/Content/course2d.dxfscene.json"), App.GetAssets());
		}
		else
		{
			return Dxf::PrepareScene(Source.LoadScene3D("Assets/Content/course3d.dxfscene.json"), App.GetAssets());
		}
	}();
	// 準備で音を鳴らさず、三個分の同条件資源を一度だけ読み込む。
	REQUIRE(Backend.GetTrace().Clones == 0);
	REQUIRE(Backend.GetTrace().TextureLoads == 1);
	REQUIRE(Backend.GetTrace().SoundLoads == 1);
	auto Scene = Toolbox::MakeUnique<TScene>(Prepared);
	auto* Current = Scene.Get();
	Current->GetControls().bSplit = Split;
	REQUIRE(App.GetScenes().RequestChange(Toolbox::Move(Scene)));
	const auto First = App.Step(0);
	if (!First)
	{
		throw Toolbox::FException(First.Error().Message);
	}
	REQUIRE(First.Value());
	REQUIRE(Current->GetPrefab("doorA").Get()->GetState() == Dxf::EPrefabInstanceState::PendingPhysics);
	REQUIRE(Backend.GetTrace().Clones == 0);
	Toolbox::f64 Time = 0;
	const auto Step = [&]()
	{
		Time += 1.0 / 60.0;
		const auto Result = App.Step(Time + .25 / 60.0);
		if (!Result)
		{
			throw Toolbox::FException(Result.Error().Message);
		}
		REQUIRE(Result.Value());
	};
	for (Toolbox::uint32 I = 0; I < 5; ++I)
	{
		Step();
	}
	const auto A = Current->GetPrefab("doorA");
	const auto B = Current->GetPrefab("doorB");
	REQUIRE(A.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(B.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(A.Get()->GetFixedJoint("assembly").Get()->GetJointId());
	REQUIRE(A.Get()->GetRevoluteJoint("armDrive").Get()->GetJointId());
	REQUIRE(A.Get()->GetDistanceJoint("hanger").Get()->GetJointId());
	const auto DriveA = A.Get()->GetPrismaticJoint("doorDrive");
	const auto DriveB = B.Get()->GetPrismaticJoint("doorDrive");
	REQUIRE(DriveA.Get()->GetObservation()->State.Drive.TargetSpeed == 0);
	REQUIRE(DriveB.Get()->GetObservation()->State.Drive.TargetSpeed == .8);
	REQUIRE(Backend.GetTrace().Clones == 1);
	// C++の公開先操作だけで片側へ目標を与える。
	Current->GetControls().bOperate = true;
	Current->GetControls().Target = 1;
	for (Toolbox::uint32 I = 0; I < 12; ++I)
	{
		Step();
	}
	REQUIRE(DriveA.Get()->GetObservation()->State.Translation > 0);
	REQUIRE(DriveB.Get()->GetObservation()->State.Drive.TargetSpeed == .8);
	const auto Position = Current->GetPhysicsWorld().GetPosition(A.Get()->GetRigidBody("doorBody").Get()->GetBodyId());
	const auto Press = [&](Dxf::EKey Key)
	{
		Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(Key)] = true;
		Step();
		Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(Key)] = false;
		Step();
	};
	// Modal開始と閉じたフレームにはSpace・Gをゲームへ漏らさない。
	const auto Target = Current->GetControls().Target;
	Press(Dxf::EKey::F1);
	Press(Dxf::EKey::Space);
	Press(Dxf::EKey::G);
	REQUIRE(Current->GetClock().IsPaused());
	REQUIRE(Current->GetControls().Target == Target);
	REQUIRE(!Current->FindCoursePrefab("extra1"));
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(Dxf::EKey::F1)] = true;
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(Dxf::EKey::G)] = true;
	Step();
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(Dxf::EKey::F1)] = false;
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(Dxf::EKey::G)] = false;
	Step();
	REQUIRE(!Current->FindCoursePrefab("extra1"));
	Current->GetControls().NextSpeed = -.25;
	Current->GetControls().NextTravel = 1.5;
	Current->GetControls().NextEffort = 9;
	Current->GetControls().bNextWarmColor = true;
	Press(Dxf::EKey::G);
	Step();
	REQUIRE(Current->GetPrefab("extra1").Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	const auto ExtraDrive = Current->GetPrefab("extra1").Get()->GetPrismaticJoint("doorDrive");
	REQUIRE(ExtraDrive.Get()->GetDescription().Joint.Drive.TargetSpeed == -.25);
	REQUIRE(ExtraDrive.Get()->GetDescription().Joint.Limits.UpperTranslation == 1.5);
	REQUIRE(ExtraDrive.Get()->GetDescription().Joint.Drive.MaxForce == 9);
	REQUIRE(DriveB.Get()->GetDescription().Joint.Drive.TargetSpeed == .8);
	Press(Dxf::EKey::X);
	REQUIRE(!A);
	REQUIRE(B);
	if constexpr (Toolbox::IsSame<TScene, Dxf::GameplaySample::DContentCourse3DScene>)
	{
		REQUIRE(Models.Loads == 1);
		REQUIRE(Models.Instances == 4);
		REQUIRE(!Renderer.ModelTimes.IsEmpty());
		REQUIRE(Renderer.ModelTimes[Renderer.ModelTimes.Size() - 1] > 0);
		if (Split)
		{
			const auto Count = Renderer.ModelTimes.Size();
			REQUIRE(Renderer.ModelTimes[Count - 1] == Renderer.ModelTimes[Count - 4]);
		}
	}
	App.GetScenes().RequestQuit();
	const auto Quit = App.Step(Time + 1);
	REQUIRE(Quit && !Quit.Value());
	REQUIRE(!B && !DriveA && !DriveB);
	REQUIRE(Models.Live == 0);
	REQUIRE(Backend.GetTrace().Sounds.IsEmpty());
	return Position;
}
} // namespace
TEST("Content sample 2D all joints typed controller Sensor audio Modal lifetime and one two views")
{
	REQUIRE(RunContentCourse<Dxf::GameplaySample::DContentCourse2DScene>(false) == RunContentCourse<Dxf::GameplaySample::DContentCourse2DScene>(true));
}
TEST("Content sample 3D all joints typed controller model animation Sensor audio Modal lifetime and one two views")
{
	REQUIRE(RunContentCourse<Dxf::GameplaySample::DContentCourse3DScene>(false) == RunContentCourse<Dxf::GameplaySample::DContentCourse3DScene>(true));
}
