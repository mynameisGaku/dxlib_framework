// SPDX-License-Identifier: NOASSERTION
#include "JointSmoke.h"
#include "Dxf/ViewCoordinates.h"
#include "DxLib.h"
namespace Dxf::GameplaySmoke
{
namespace
{
// 描画と同じ2D投影。別のNativeカメラへ依存しない。
FVector2 Project_Internal(GameplaySample::DInteraction2DScene& Scene, Toolbox::FVector2 Point, Toolbox::int32 Side, Toolbox::int32, Toolbox::int32)
{
	return Scene.ToScreen(Point, Side);
}
// 指定Viewと現在の描画寸法でガイド線の位置を求める。
FVector2 Project_Internal(GameplaySample::DInteraction3DScene& Scene, Toolbox::FVector3 Point, Toolbox::int32 Side, Toolbox::int32 Width, Toolbox::int32 Height)
{
	// 配置または投影に使う位置。
	const auto Position = ProjectWorldToScreen(Scene.GetView(Side), Width, Height, Point);
	Check(Position && Position.Value().bInsideView, "joint project failed");
	return Position.Value().Screen;
}
// 5x5のCPU画素領域を一括読戻しの同じ画像から比較する。
template <typename TScene>
void Pixels_Internal(FSmokeApp& App, TScene& Scene, const Toolbox::FPath& Output, const char* File, Toolbox::size_t Index)
{
	// 現在の実描画サイズ。ABIに合わせる出力先。
	int Width = 0;
	// 今回描画した対象の縦幅。
	int Height = 0;
	Check(DxLib::GetDrawScreenSize(&Width, &Height) == 0, "joint render size failed");
	// 今回調べる接続Component。
	const auto* Joint = Scene.GetJointCourse().GetJoints()[Index].Get();
	Check(Joint != nullptr && Joint->GetJointId(), "joint pixel target unavailable");
	// 直近の成功Stepから得た値。
	const auto Observation = Joint->GetObservation();
	Check(static_cast<bool>(Observation), "joint pixel observation missing");
	// 両端は表示Bodyと同じ補間姿勢。非補間の観察値を描画へ代用しない。
	using FVector = decltype(Observation->AnchorA);
	// 接続のA側。
	FVector A;
	// 接続のB側。
	FVector B;
	Check(Joint->GetRenderAnchors(A, B), "joint render anchor missing");
	// 今回描画したビューの数。
	const Toolbox::int32 Views = Scene.IsSplit() ? 2 : 1;
	// ガイドの中点と、両端の物体、全画面の状態表示を同じ読戻しから調べる。
	Toolbox::TVector<FVector2> Points;
	// 同じ読戻しで得た画素色。
	Toolbox::TVector<FColor> Colors;
	// ガイドA側の表示Body番号。
	const Toolbox::size_t BodyA = Index == 0 ? 0 : (Index == 5 ? 7 : Index + 1);
	// ガイドB側の表示Body番号。
	const Toolbox::size_t BodyB = Index == 0 ? 1 : (Index == 5 ? 8 : Index + 2);
	const auto& Course = Scene.GetJointCourse();
	// ガイド中点と両端物体の照合位置。
	const FVector Centers[3] = {A * 0.5f + B * 0.5f, BodyA == 7 ? Course.GetCarrier()->GetRenderPosition() : Course.GetBodies()[BodyA].Get()->GetRenderPosition(), Course.GetBodies()[BodyB].Get()->GetRenderPosition()};
	for (Toolbox::int32 Side = 0; Side < Views; ++Side)
	{
		for (const auto& Center : Centers)
		{
			const auto Screen = Project_Internal(Scene, Center, Side, Width, Height);
			for (Toolbox::int32 Y = -2; Y <= 2; ++Y)
			{
				for (Toolbox::int32 X = -2; X <= 2; ++X)
				{
					Points.PushBack({Screen.X + X, Screen.Y + Y});
				}
			}
		}
	}
	// 全画面の文字検査を始める位置。
	const auto HudStart = Points.Size();
	for (Toolbox::int32 Y = 154; Y < 172; ++Y)
	{
		for (Toolbox::int32 X = 16; X < 155; ++X)
		{
			Points.PushBack({static_cast<Toolbox::f32>(X), static_cast<Toolbox::f32>(Y)});
		}
	}
	Colors.Resize(Points.Size());
	App.CapturePoints(Output / File, Points.Data(), Colors.Data(), Points.Size());
	for (Toolbox::int32 Side = 0; Side < Views; ++Side)
	{
		for (Toolbox::int32 Region = 0; Region < 3; ++Region)
		{
			// この領域で条件に合った画素数。
			Toolbox::int32 Matches = 0;
			for (Toolbox::int32 Pixel = 0; Pixel < 25; ++Pixel)
			{
				const FColor Color = Colors[Side * 75 + Region * 25 + Pixel];
				const bool bGuide = Color.R > 190 && Color.G > 160 && Color.B < 140;
				const bool bBody = Color.R > 60 && Color.B > Color.R && Color.G < Color.R;
				if (Region == 0 ? bGuide : bBody)
				{
					++Matches;
				}
			}
			Toolbox::Out << "JOINT_PIXEL " << File << " side=" << Side << " region=" << Region << " matches=" << Matches << "\n";
			Check(Matches > 0, Region == 0 ? "joint projected guide line pixel missing" : "joint projected endpoint body pixel missing");
		}
	}
	// 状態表示の色に合った画素数。
	Toolbox::int32 TextPixels = 0;
	for (Toolbox::size_t Pixel = HudStart; Pixel < Colors.Size(); ++Pixel)
	{
		const auto Color = Colors[Pixel];
		if (Color.R > 150 && Color.B > 200 && Color.G > 120 && Color.G < Color.R)
		{
			++TextPixels;
		}
	}
	Check(TextPixels > 20, "joint full screen status text pixels missing");
	Toolbox::Out << "JOINT_HUD " << File << " pixels=" << TextPixels << "\n";
	Check(Joint->GetObservation()->SuccessfulStep == Observation->SuccessfulStep, "joint readback changed physics");
	Check(Joint->GetObservation()->CurrentLength == Observation->CurrentLength, "joint readback changed distance");
}
// 既存Applicationで接続操作とScene再入場を通す。手作成Sceneで代用しない。
template <typename TScene>
void Accept_Internal(FSmokeApp& App, TScene& Scene, const Toolbox::FPath& Output, bool b3D)
{
	App.Step();
	App.Step();
	for (const auto& Joint : Scene.GetJointCourse().GetJoints())
	{
		Check(Joint.Get() != nullptr && Joint.Get()->GetJointId(), "joint course registration missing");
	}
	// カメラを仕掛けへ向ける。Characterは通常の公開Teleportを使う。
	Scene.GetPlayer()->GetCharacter().Teleport({7, 0.52f});
	App.Step();
	Pixels_Internal(App, Scene, Output, b3D ? "joint3d-single.png" : "joint2d-single.png", 0);
	Scene.ToggleSplit();
	App.Step();
	Pixels_Internal(App, Scene, Output, b3D ? "joint3d-split.png" : "joint2d-split.png", 0);
	App.Press(EKey::K);
	App.Step();
	Check(!Scene.GetJointCourse().GetJoints()[0].Get()->GetJointId(), "joint disconnect input failed");
	App.Press(EKey::L);
	App.Step();
	Check(static_cast<bool>(Scene.GetJointCourse().GetJoints()[0].Get()->GetJointId()), "joint reconnect input failed");
	App.Press(EKey::J);
	App.Step();
	App.Press(EKey::P);
	// 操作前の比較値。
	const auto Before = Scene.GetJointCourse().GetJoints()[0].Get()->GetObservation();
	App.Press(EKey::K);
	App.Step();
	Check(Scene.GetJointCourse().GetJoints()[0].Get()->GetObservation()->SuccessfulStep == Before->SuccessfulStep, "joint pause advanced physics");
	Check(static_cast<bool>(Scene.GetJointCourse().GetJoints()[0].Get()->GetJointId()), "joint pause accepted input");
	App.Press(EKey::P);
	App.Step();
	Check(DxLib::SetWindowSize(1001, 501) == 0, "joint odd window resize failed");
	App.Step();
	Pixels_Internal(App, Scene, Output, b3D ? "joint3d-odd.png" : "joint2d-odd.png", 0);
	Check(DxLib::SetWindowSize(1280, 720) == 0, "joint window restoration failed");
	App.Step();
	Scene.GetPlayer()->GetCharacter().Teleport({-3, 0.52f});
	App.Step();
	Pixels_Internal(App, Scene, Output, b3D ? "joint3d-chain.png" : "joint2d-chain.png", 2);
	Scene.GetPlayer()->GetCharacter().Teleport({5, 0.52f});
	App.Step();
	Pixels_Internal(App, Scene, Output, b3D ? "joint3d-carrier.png" : "joint2d-carrier.png", 5);
	// 失敗前の世代付き接続ID。
	const auto Old = Scene.GetJointCourse().GetBodies()[4];
	App.Press(EKey::N);
	App.Step();
	Check(Old.Get() == nullptr, "joint sample regeneration kept old body");
	Check(static_cast<bool>(Scene.GetJointCourse().GetJoints()[0].Get()->GetJointId()), "joint regeneration failed");
}
} // namespace
void RunJointSmoke(FDxLibBackends& Backends, const char* Root, const Toolbox::FPath& Output)
{
	{
		FSmokeApp App(Backends, Root);
		App.StartInteraction(false);
		Accept_Internal(App, App.Interaction2D(), Output, false);
		App.SwitchDimension();
		Accept_Internal(App, App.Interaction3D(), Output, true);
		App.SwitchDimension();
		App.Step();
		Check(static_cast<bool>(App.Interaction2D().GetJointCourse().GetJoints()[0].Get()->GetJointId()), "joint scene reentry failed");
		App.Quit();
	}
	{
		FSmokeApp Restart(Backends, Root);
		Restart.StartInteraction(true);
		Restart.Step();
		Check(static_cast<bool>(Restart.Interaction3D().GetJointCourse().GetJoints()[0].Get()->GetJointId()), "joint new application restart failed");
		Restart.Quit();
	}
	Toolbox::Out << "JOINT_GAMEPLAY_NATIVE_PASSED\n";
}
} // namespace Dxf::GameplaySmoke
