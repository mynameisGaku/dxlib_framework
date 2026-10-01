// SPDX-License-Identifier: NOASSERTION
#include "MechanismSmoke.h"
#include "Dxf/ViewCoordinates.h"
#include "DxLib.h"
namespace Dxf::GameplaySmoke
{
namespace
{
// 画面の投影だけを行い、Nativeカメラを変更しない。
FVector2 ProjectMechanism_Internal(const GameplaySample::DInteraction2DScene& Scene, Toolbox::FVector2 Point, Toolbox::int32 Side, Toolbox::int32, Toolbox::int32)
{
	return Scene.ToScreen(Point, Side);
}
// 描画と同じ指定ビューで投影する。
FVector2 ProjectMechanism_Internal(const GameplaySample::DInteraction3DScene& Scene, Toolbox::FVector3 Point, Toolbox::int32 Side, Toolbox::int32 Width, Toolbox::int32 Height)
{
	const auto Result = ProjectWorldToScreen(Scene.GetView(Side), Width, Height, Point);
	Check(Result && Result.Value().bInsideView, "mechanism projection outside view");
	return Result.Value().Screen;
}
// Bodyを実際の描画場所で確認する。一枚のCPU画像から全領域を照合する。
template <typename TScene>
void PixelsMechanism_Internal(FSmokeApp& App, TScene& Scene, const Toolbox::FPath& Output, const char* File, Toolbox::size_t Index)
{
	int Width = 0;
	int Height = 0;
	Check(DxLib::GetDrawScreenSize(&Width, &Height) == 0, "mechanism screen size failed");
	const auto& Course = Scene.GetMechanismCourse();
	const auto* Body = Course.GetBodies()[Index].Get();
	Check(Body != nullptr && Body->HasBody(), "mechanism body unavailable");
	const auto Count = Course.GetUpdateCount();
	const Toolbox::int32 Views = Scene.IsSplit() ? 2 : 1;
	Toolbox::TVector<FVector2> Points;
	Toolbox::TVector<FColor> Colors;
	for (Toolbox::int32 Side = 0; Side < Views; ++Side)
	{
		const auto Screen = ProjectMechanism_Internal(Scene, Body->GetRenderPosition(), Side, Width, Height);
		for (Toolbox::int32 Y = -5; Y <= 5; ++Y)
		{
			for (Toolbox::int32 X = -5; X <= 5; ++X)
			{
				Points.PushBack({Screen.X + X, Screen.Y + Y});
			}
		}
	}
	Colors.Resize(Points.Size());
	App.CapturePoints(Output / File, Points.Data(), Colors.Data(), Points.Size());
	for (Toolbox::int32 Side = 0; Side < Views; ++Side)
	{
		Toolbox::int32 Matches = 0;
		for (Toolbox::int32 Pixel = 0; Pixel < 121; ++Pixel)
		{
			const auto Color = Colors[Side * 121 + Pixel];
			if (Color.R < 110 && Color.G > Color.R + 30 && Color.B > Color.R + 20 && Color.G > 80)
			{
				++Matches;
			}
		}
		Toolbox::Out << "MECHANISM_PIXEL " << File << " body=" << Index << " side=" << Side << " matches=" << Matches << "\n";
		Check(Matches > 8, "mechanism body pixels missing at projected pose");
	}
	Check(Course.GetUpdateCount() == Count, "mechanism capture advanced physics");
}
// 同じ実Sceneの装置へ目標を要求し、回転・直動・固定の各描画を確かめる。
template <typename TScene>
void AcceptMechanisms_Internal(FSmokeApp& App, TScene& Scene, const Toolbox::FPath& Output, bool b3D)
{
	App.Step();
	App.Step();
	auto& Course = Scene.GetMechanismCourse();
	auto& Controller = Course.GetController();
	Controller.Select(1);
	Controller.SetMotion(0.7, 1, 20);
	for (Toolbox::int32 Step = 0; Step < 90; ++Step)
	{
		App.Step();
	}
	Check(Course.GetRevolutes()[1].Get()->GetObservation()->State.Angle > 0.2, "native mechanism motor did not rotate");
	// 実UIから装置を選び、Modal中は物理を進めず、閉じてから要求を適用する。
	Controller.Select(1);
	App.Press(EKey::F1);
	const auto PausedCount = Course.GetUpdateCount();
	for (Toolbox::int32 Key = 0; Key < 4; ++Key)
	{
		App.Press(EKey::Down);
		App.Step();
	}
	App.Press(EKey::Enter);
	Check(Controller.GetSelection() == 2, "native mechanism selection button");
	const auto OldTarget = Controller.GetTarget();
	App.Press(EKey::Down);
	App.Press(EKey::Right);
	Check(Controller.GetTarget() > OldTarget && Course.GetUpdateCount() == PausedCount, "native UI target or modal isolation");
	App.Press(EKey::F1);
	App.Step();
	Check(Course.GetPrismatics()[0].Get()->GetObservation()->State.Drive.TargetSpeed > 0, "native UI command did not reach motor");
	const Toolbox::size_t Bodies[3] = {3, 5, 9};
	for (Toolbox::size_t Kind = 0; Kind < 3; ++Kind)
	{
		const auto Position = Course.GetBodies()[Bodies[Kind]].Get()->GetGamePosition();
		// カメラは対象へ向けるが、プレイヤー自身で照合点を覆わない。
		Scene.GetPlayer()->GetCharacter().Teleport(Position + decltype(Position){0, -1.5f});
		App.Step();
		char File[80];
		snprintf(File, sizeof(File), "mechanism-%s-kind%llu-single.png", b3D ? "3d" : "2d", static_cast<Toolbox::uint64>(Kind));
		PixelsMechanism_Internal(App, Scene, Output, File, Bodies[Kind]);
		Scene.ToggleSplit();
		App.Step();
		snprintf(File, sizeof(File), "mechanism-%s-kind%llu-split.png", b3D ? "3d" : "2d", static_cast<Toolbox::uint64>(Kind));
		PixelsMechanism_Internal(App, Scene, Output, File, Bodies[Kind]);
		// 同じ装置を奇数寸法の左右表示でも、現在の投影位置で確認する。
		Check(DxLib::SetWindowSize(1001, 501) == 0, "mechanism odd resize");
		App.Step();
		App.Step();
		snprintf(File, sizeof(File), "mechanism-%s-kind%llu-odd.png", b3D ? "3d" : "2d", static_cast<Toolbox::uint64>(Kind));
		PixelsMechanism_Internal(App, Scene, Output, File, Bodies[Kind]);
		Check(DxLib::SetWindowSize(1280, 720) == 0, "mechanism resize restore");
		App.Step();
		Scene.ToggleSplit();
	}
}
} // namespace
void RunMechanismSmoke(FDxLibBackends& Backends, const char* Root, const Toolbox::FPath& Output)
{
	FSmokeApp App(Backends, Root);
	App.StartInteraction(false);
	AcceptMechanisms_Internal(App, App.Interaction2D(), Output, false);
	App.SwitchDimension();
	AcceptMechanisms_Internal(App, App.Interaction3D(), Output, true);
	App.Quit();
	Toolbox::Out << "MECHANISM_GAMEPLAY_NATIVE_PASSED\n";
}
} // namespace Dxf::GameplaySmoke
