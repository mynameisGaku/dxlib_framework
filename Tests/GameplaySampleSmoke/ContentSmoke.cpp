// SPDX-License-Identifier: NOASSERTION
#include "ContentSmoke.h"
#include "ContentCourseScene2D.h"
#include "ContentCourseScene3D.h"
#include "Dxf/SceneContentSource.h"
#include "Dxf/SceneContentRequest.h"
#include "Dxf/ViewCoordinates.h"
#include "DxLib.h"
namespace Dxf::GameplaySmoke
{
namespace
{
// 描画と同じ現在の寸法から2Dの補間位置を画素へ移す。
FVector2 Project(const FSceneDefinition2D& D, Toolbox::FVector2 P, bool Split, Toolbox::int32 Side, Toolbox::int32 W, Toolbox::int32 H)
{
	const auto Origin =
	    Split ? (Side == 0 ? static_cast<Toolbox::f32>(W) / 4 : W / 2 + static_cast<Toolbox::f32>(W - W / 2) / 2)
	          : static_cast<Toolbox::f32>(W) / 2;
	return {Origin + P.X * D.Views[0].PixelsPerMeter, H * .65f - P.Y * D.Views[0].PixelsPerMeter};
}
// 指定Viewのみから投影する。Native状態は読まない。
FVector2 Project(const FSceneDefinition3D& D, Toolbox::FVector3 P, bool Split, Toolbox::int32 Side, Toolbox::int32 W, Toolbox::int32 H)
{
	auto View = D.Views[0];
	View.bViewport = Split;
	View.Viewport = Side == 0 ? FIntRect{0, 0, W / 2, H} : FIntRect{W / 2, 0, W, H};
	const auto Screen = ProjectWorldToScreen(View, W, H, P);
	Check(Screen && Screen.Value().bInsideView, "content projection outside view");
	return Screen.Value().Screen;
}
// 一回の読戻しで両ViewのBodyを調べる。定義の色と補間位置を照合する。
template <typename TScene, typename TDefinition>
void Pixels(FSmokeApp& App, TScene& Scene, const TDefinition& D, const Toolbox::FPath& Output, const char* File)
{
	int Width = 0;
	int Height = 0;
	Check(DxLib::GetDrawScreenSize(&Width, &Height) == 0, "content target size");
	const auto A = Scene.GetPrefab("doorA");
	const auto Body = A.Get()->GetRigidBody("doorBody");
	const auto Before = Body.Get()->GetBodyId();
	const auto Position = Body.Get()->GetRenderPosition();
	const auto Expected = A.Get()->GetDefinition().Parts[1].Visual.Color;
	Toolbox::TVector<FVector2> Points;
	const auto Views = Scene.GetControls().bSplit ? 2 : 1;
	const auto& Part = A.Get()->GetDefinition().Parts[A.Get()->GetDefinition().Parts.Size() - 1];
	auto AssetPosition = Part.Body.Position;
	const auto& Placement = D.Placements[0];
	if constexpr (Toolbox::IsSame<TDefinition, FSceneDefinition2D>)
	{
		const auto C = static_cast<Toolbox::f32>(Toolbox::Cos(Placement.Rotation));
		const auto S = static_cast<Toolbox::f32>(Toolbox::Sin(Placement.Rotation));
		AssetPosition = Placement.Position + Toolbox::FVector2{C * AssetPosition.X - S * AssetPosition.Y,
		                                                       S * AssetPosition.X + C * AssetPosition.Y};
	}
	else
	{
		// 同梱モデルは高さ2m。0.8倍の中央を、実データの配置で投影する。
		AssetPosition.Y += .8f;
		AssetPosition = Placement.Position + Placement.Rotation.Rotate(AssetPosition);
	}
	constexpr Toolbox::size_t BodyPixels = 121;
	constexpr Toolbox::size_t AssetPixels = 31 * 31;
	constexpr Toolbox::size_t Stride = BodyPixels + AssetPixels;
	for (Toolbox::int32 Side = 0; Side < Views; ++Side)
	{
		const auto Screen = Project(D, Position, Views == 2, Side, Width, Height);
		printf("CONTENT_PROJECTION side=%d x=%.9f y=%.9f\n", Side, static_cast<Toolbox::f64>(Screen.X), static_cast<Toolbox::f64>(Screen.Y));
		for (Toolbox::int32 Y = -5; Y <= 5; ++Y)
		{
			for (Toolbox::int32 X = -5; X <= 5; ++X)
			{
				Points.PushBack({Screen.X + X, Screen.Y + Y});
			}
		}
		const auto AssetScreen = Project(D, AssetPosition, Views == 2, Side, Width, Height);
		for (Toolbox::int32 Y = -15; Y <= 15; ++Y)
		{
			for (Toolbox::int32 X = -15; X <= 15; ++X)
			{
				Points.PushBack({AssetScreen.X + X, AssetScreen.Y + Y});
			}
		}
	}
	Toolbox::TVector<FColor> Colors;
	Colors.Resize(Points.Size());
	App.CapturePoints(Output / File, Points.Data(), Colors.Data(), Points.Size());
	for (Toolbox::int32 Side = 0; Side < Views; ++Side)
	{
		Toolbox::int32 Matches = 0;
		for (Toolbox::int32 I = 0; I < 121; ++I)
		{
			const auto C = Colors[Side * Stride + I];
			if (Toolbox::Abs(static_cast<Toolbox::int32>(C.R) - Expected.R) <= 35 && Toolbox::Abs(static_cast<Toolbox::int32>(C.G) - Expected.G) <= 35 && Toolbox::Abs(static_cast<Toolbox::int32>(C.B) - Expected.B) <= 35)
			{
				++Matches;
			}
		}
		Toolbox::Out << "CONTENT_PIXEL " << File << " side=" << Side << " matches=" << Matches << "\n";
		Check(Matches > 50, "content pixels differ from definition at interpolated Body");
		Toolbox::int32 AssetMatches = 0;
		for (Toolbox::size_t I = 0; I < AssetPixels; ++I)
		{
			const auto C = Colors[Side * Stride + BodyPixels + I];
			if (C.G > 40 && C.B > 40 && Toolbox::Abs(static_cast<Toolbox::int32>(C.R) - C.B) > 8)
			{
				++AssetMatches;
			}
		}
		Toolbox::Out << "CONTENT_ASSET_PIXEL " << File << " side=" << Side << " matches=" << AssetMatches << "\n";
		Check(AssetMatches > (Toolbox::IsSame<TDefinition, FSceneDefinition2D> ? 200 : 30), "content Texture or Model pixels missing at data placement");
	}
	Check(Body.Get()->GetBodyId() == Before && Body.Get()->GetRenderPosition() == Position, "capture changed content physics");
}
// 実Applicationの通常境界から準備済みSceneを採用する。
template <typename TScene, typename TPrepared>
void Accept(FSmokeApp& App, const TPrepared& Prepared, const Toolbox::FPath& Output, bool b3D)
{
	auto Scene = Toolbox::MakeUnique<TScene>(Prepared);
	auto* Current = Scene.Get();
	Check(static_cast<bool>(App.Application().GetScenes().RequestChange(Toolbox::Move(Scene))), "content scene request");
	for (Toolbox::uint32 I = 0; I < 5; ++I)
	{
		App.Step();
	}
	const auto A = Current->GetPrefab("doorA");
	const auto B = Current->GetPrefab("doorB");
	Check(A.Get()->GetState() == EPrefabInstanceState::Ready && B.Get()->GetState() == EPrefabInstanceState::Ready, "content native Ready");
	Check(A.Get()->GetDistanceJoint("hanger").Get()->GetJointId() && A.Get()->GetRevoluteJoint("armDrive").Get()->GetJointId() && A.Get()->GetFixedJoint("assembly").Get()->GetJointId(), "content native all Joint kinds");
	const auto DriveA = A.Get()->GetPrismaticJoint("doorDrive");
	const auto DriveB = B.Get()->GetPrismaticJoint("doorDrive");
	const auto SpeedB = DriveB.Get()->GetObservation()->State.Drive.TargetSpeed;
	printf("CONTENT_DRIVE definition=%.9f description=%.9f observation=%.9f\n", Prepared.Prefabs[1].Definition->Joints[0].Prismatic.Drive.TargetSpeed, DriveB.Get()->GetDescription().Joint.Drive.TargetSpeed, SpeedB);
	Check(SpeedB == Prepared.Prefabs[1].Definition->Joints[0].Prismatic.Drive.TargetSpeed, "native content drive differs from data");
	Check(DriveA.Get()->GetJointId() && DriveB.Get()->GetJointId() && *DriveA.Get()->GetJointId() != *DriveB.Get()->GetJointId(), "content independent instance joints");
	const auto Position = Current->GetPhysicsWorld().GetPosition(A.Get()->GetRigidBody("doorBody").Get()->GetBodyId());
	printf("CONTENT_WORLD %sd instances=%llu x=%.9f y=%.9f speedB=%.9f\n", b3D ? "3" : "2", static_cast<Toolbox::uint64>(Prepared.Prefabs.Size()), static_cast<Toolbox::f64>(Position.X), static_cast<Toolbox::f64>(Position.Y), SpeedB);
	Pixels(App, *Current, *Prepared.Definition, Output, b3D ? "content3d-single.png" : "content2d-single.png");
	Current->GetControls().bSplit = true;
	App.Step();
	Pixels(App, *Current, *Prepared.Definition, Output, b3D ? "content3d-split.png" : "content2d-split.png");
	Check(DxLib::SetWindowSize(1001, 501) == 0, "content odd resize");
	App.Step();
	App.Step();
	Pixels(App, *Current, *Prepared.Definition, Output, b3D ? "content3d-odd.png" : "content2d-odd.png");
	Check(DxLib::SetWindowSize(1280, 720) == 0, "content resize restore");
	App.Step();
	Current->GetControls().bOperate = true;
	Current->GetControls().Target = 1;
	for (Toolbox::uint32 I = 0; I < 20; ++I)
	{
		App.Step();
	}
	Check(DriveA.Get()->GetObservation()->State.Translation > 0 && DriveB.Get()->GetObservation()->State.Drive.TargetSpeed == SpeedB, "content native independent target control");
	FSceneContentRequest Request{FSceneContentSource{App.Application().GetAssets().GetProjectRoot()}};
	const auto Broken = Request.LoadScene2D("Assets/Content/broken.dxfscene.json");
	Request.Poll(App.Application().GetAssets());
	Check(Broken.GetState() == ESceneContentRequestState::Failed && App.Application().GetScenes().GetCurrent() == Current && A && B, "broken content changed current scene");
	const auto Retry = b3D ? Request.LoadScene3D("Assets/Content/course3d.dxfscene.json")
	                       : Request.LoadScene2D("Assets/Content/course2d.dxfscene.json");
	Request.Poll(App.Application().GetAssets());
	Check(Retry.GetState() == ESceneContentRequestState::Ready, "native content retry did not prepare");
	if constexpr (Toolbox::IsSame<TScene, GameplaySample::DContentCourse2DScene>)
	{
		Check(static_cast<bool>( App.Application().GetScenes().RequestChange(Toolbox::MakeUnique<TScene>(Request.GetPrepared2D()))), "2D content retry scene request");
	}
	else
	{
		Check(static_cast<bool>( App.Application().GetScenes().RequestChange(Toolbox::MakeUnique<TScene>(Request.GetPrepared3D()))), "3D content retry scene request");
	}
	for (Toolbox::uint32 I = 0; I < 5; ++I)
	{
		App.Step();
	}
	Check(!A && !B && !DriveA && !DriveB, "old content handles followed retry generation");
	const auto* Retried = App.Application().GetScenes().GetCurrent()->template TryCast<TScene>();
	Check(Retried && Retried->GetPrefab("doorA").Get()->GetState() == EPrefabInstanceState::Ready, "native retry generation not Ready");
	Toolbox::Out << "CONTENT_FAILED_PREPARE_OLD_SCENE_RETRY_PASSED\n";
	App.Quit();
}
} // namespace
void RunContentSmoke(FDxLibBackends& Backends, const char* Root, const Toolbox::FPath& Output)
{
	FSceneContentSource Source{Toolbox::FPath(Root)};
	{
		FSmokeApp App(Backends, Root);
		App.StartInteraction(false);
		const auto Start = Toolbox::MonotonicNanoseconds();
		auto Definition = Source.LoadScene2D("Assets/Content/course2d.dxfscene.json");
		const auto Parsed = Toolbox::MonotonicNanoseconds();
		const auto Prepared = PrepareScene(Toolbox::Move(Definition), App.Application().GetAssets());
		const auto Ready = Toolbox::MonotonicNanoseconds();
		printf("CONTENT_PREPARATION dimension=2 cpu_us=%.3f owner_native_us=%.3f\n", static_cast<Toolbox::f64>(Parsed - Start) / 1000, static_cast<Toolbox::f64>(Ready - Parsed) / 1000);
		Accept<GameplaySample::DContentCourse2DScene>(App, Prepared, Output, false);
	}
	{
		FSmokeApp App(Backends, Root);
		App.StartInteraction(true);
		const auto Start = Toolbox::MonotonicNanoseconds();
		auto Definition = Source.LoadScene3D("Assets/Content/course3d.dxfscene.json");
		const auto Parsed = Toolbox::MonotonicNanoseconds();
		const auto Prepared = PrepareScene(Toolbox::Move(Definition), App.Application().GetAssets());
		const auto Ready = Toolbox::MonotonicNanoseconds();
		printf("CONTENT_PREPARATION dimension=3 cpu_us=%.3f owner_native_us=%.3f\n", static_cast<Toolbox::f64>(Parsed - Start) / 1000, static_cast<Toolbox::f64>(Ready - Parsed) / 1000);
		Accept<GameplaySample::DContentCourse3DScene>(App, Prepared, Output, true);
	}
	Toolbox::Out << "CONTENT_GAMEPLAY_NATIVE_PASSED\n";
}
} // namespace Dxf::GameplaySmoke
