// SPDX-License-Identifier: NOASSERTION
// 入力だけを固定し、キャラクター移動のサンプル（2D／3D）を実Application・実DxLibで操作する。
// 確認:
// 段差・坂・急坂・壁の上の移動、ジャンプと着地、リセット、初期重なりからの復帰、一時停止と再開、3Dの二つの壁の角、
// 2画面表示で固定更新の回数が変わらないこと、シーンの再入場、終了。画素はプレイヤーの描画位置で照合する。
#include "CharacterSample.h"
#include "Dxf/Application.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/ViewCoordinates.h"
#include "Toolbox/Platform.h"
#include "DxLib.h"
// Windowsの文字種マクロとToolboxの同名関数を分離する。
#ifdef CreateDirectory
#undef CreateDirectory
#endif
namespace
{
using namespace Dxf;
using namespace Dxf::GameplaySample;
// OS入力の揺れを排除する。更新と押下判定は本物のInputSystemへ任せる。
class FScriptedInput final : public IInputSource
{
public:
	// 次のフレームの入力。
	FRawInput State;
	TResult<FRawInput> Poll() override
	{
		return TResult<FRawInput>::Success(State);
	}
};
// 失敗内容を実行結果へ伝える。
void Check(bool bOk, const char* Message)
{
	if (!bOk)
	{
		throw Toolbox::FException(Message);
	}
}
// 表示した画面を保存し、指定した画面座標の色を返す。
FColor Capture(const Toolbox::FPath& Path, FVector2 Point)
{
	Check(DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0, "front buffer selection failed");
	// ABIに合わせたRGB出力先。
	int R = 0;
	int G = 0;
	int B = 0;
	const Toolbox::int32 Color = DxLib::GetPixel(static_cast<int>(Point.X), static_cast<int>(Point.Y));
	DxLib::GetColor2(Color, &R, &G, &B);
	const Toolbox::int32 Saved = DxLib::SaveDrawScreenToPNG(0, 0, 1280, 720, Path.ToUtf8().CStr());
	Check(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0, "back buffer restoration failed");
	Check(Saved == 0, "capture failed");
	return {static_cast<Toolbox::uint8>(R), static_cast<Toolbox::uint8>(G), static_cast<Toolbox::uint8>(B), 255};
}
} // namespace
Toolbox::int32 main(Toolbox::int32 Count, char** Args)
{
	if (Count != 3)
	{
		Toolbox::Err << "Usage: NativeGameplaySmoke <ProjectRoot> <output directory>\n";
		return 2;
	}
	try
	{
		// ネイティブ窓口はApplicationより長く生存する。
		FDxLibBackends Backends;
		FScriptedInput Input;
		const auto Services = Backends.GetServices();
		FApplicationSettings Settings;
		Settings.ProjectRoot = Args[1];
		Settings.Window.Width = 1280;
		Settings.Window.Height = 720;
		Settings.Window.bVSync = false;
		Settings.ExecutionThreadCount = 1;
		FApplication App(
		    {Services.Platform, Input, Services.Textures, Services.Sounds, Services.Fonts, Services.Renderer},
		    Settings);
		const Toolbox::FPath Output(Args[2]);
		Check(Toolbox::IsDirectory(Output) || Toolbox::CreateDirectory(Output), "output directory failed");
		Check(static_cast<bool>(App.Start(Toolbox::MakeUnique<DCharacterSample2DScene>())), "start failed");
		// 最初のStepの時刻が基準になる。その後、固定更新の境界から4分の1ずらした時刻で進め、1フレームに1回の固定更新にする。
		Toolbox::f64 Time = 0;
		const auto First = App.Step(Time);
		Check(First && First.Value(), "first step failed");
		Time += 0.25 / 60.0;
		const auto Step = [&]
		{
			const auto Result = App.Step(Time += 1.0 / 60.0);
			Check(Result && Result.Value(), "game step failed");
		};
		const auto Press = [&](EKey Key)
		{
			Input.State.Keys[static_cast<Toolbox::size_t>(Key)] = true;
			Step();
			Input.State.Keys[static_cast<Toolbox::size_t>(Key)] = false;
		};
		const auto Hold = [&](EKey Key, bool bDown)
		{
			Input.State.Keys[static_cast<Toolbox::size_t>(Key)] = bDown;
		};
		const auto Scene2D = [&]() -> DCharacterSample2DScene&
		{
			auto* Scene = App.GetScenes().GetCurrent()->TryCast<DCharacterSample2DScene>();
			Check(Scene != nullptr, "2D scene missing");
			return *Scene;
		};
		const auto Scene3D = [&]() -> DCharacterSample3DScene&
		{
			auto* Scene = App.GetScenes().GetCurrent()->TryCast<DCharacterSample3DScene>();
			Check(Scene != nullptr, "3D scene missing");
			return *Scene;
		};
		// 2D: 静止して接地する。
		Step();
		Step();
		auto* Player2D = &Scene2D().GetPlayer().Get()->GetCharacter();
		Check(Player2D->IsGrounded() && Player2D->GetCenter() == Toolbox::FVector2{StartX, StartY}, "2D start state");
		// 2D: 右へ歩き、低い段差を上り、30度の坂を上って台へ、60度の急坂の手前で止まる。重ならない。
		Hold(EKey::D, true);
		bool bSteppedUp = false;
		Toolbox::f32 Highest = 0;
		const Toolbox::int64 WalkStart = Player2D->GetStepCount();
		for (Toolbox::int32 Frame = 0; Frame < 420; ++Frame)
		{
			Step();
			bSteppedUp = bSteppedUp || Player2D->GetLastStep().bSteppedUp;
			Highest = Toolbox::Max(Highest, Player2D->GetCenter().Y);
			const auto Contacts =
			    Scene2D().GetPhysicsWorld().QueryContacts({Player2D->GetCenter(), 0.5f}, 0, Player2D->GetBodyId());
			for (Toolbox::uint32 Index = 0; Index < Contacts.Count; ++Index)
			{
				Check(Contacts.Items[Index].Separation >= -1e-4, "2D character overlapped the level");
			}
		}
		Hold(EKey::D, false);
		Check(Player2D->GetStepCount() - WalkStart == 420, "2D fixed steps per frame");
		Check(bSteppedUp && Highest > 3.4f && Player2D->GetCenter().X < 17.2f, "2D walk over step/ramp/steep slope");
		for (Toolbox::int32 Frame = 0; Frame < 30; ++Frame)
		{
			Step();
		}
		Check(Player2D->IsGrounded(), "2D grounded after walk");
		// 2D: 描画位置の画素はプレイヤーの色。
		const Toolbox::FVector2 Center2D = Player2D->GetRenderCenter();
		const FColor Pixel2D =
		    Capture(Output / "walk2d.png", DCharacterSample2DScene::ToScreen(Center2D.X, Center2D.Y));
		Check(Pixel2D.R == 255 && Pixel2D.G == 200 && Pixel2D.B == 40, "2D player pixel mismatch");
		// 2D: リセット、ジャンプと着地。
		Press(EKey::R);
		Check(Player2D->GetCenter() == Toolbox::FVector2{StartX, StartY}, "2D reset");
		Step();
		Press(EKey::Space);
		bool bJumped = Player2D->GetLastStep().bJumped;
		bool bLanded = false;
		for (Toolbox::int32 Frame = 0; Frame < 90 && !bLanded; ++Frame)
		{
			Step();
			bJumped = bJumped || Player2D->GetLastStep().bJumped;
			bLanded = Player2D->GetLastStep().bLanded;
		}
		Check(bJumped && bLanded && Player2D->IsGrounded(), "2D jump and landing");
		// 2D: 床へめり込ませると、次の固定更新で押し出して接地する。
		Press(EKey::O);
		Step();
		Check(Player2D->GetLastStep().Recovery.Status == ECharacterRecoveryStatus::Resolved && Player2D->IsGrounded(),
		      "2D overlap recovery");
		// 2D: 一時停止中は固定更新が進まず、再開で戻る。
		Press(EKey::P);
		const Toolbox::int64 Paused = Player2D->GetStepCount();
		for (Toolbox::int32 Frame = 0; Frame < 10; ++Frame)
		{
			Step();
		}
		Check(Player2D->GetStepCount() == Paused, "2D pause");
		Press(EKey::P);
		Step();
		Check(Player2D->GetStepCount() > Paused, "2D resume");
		// 3Dへ切り替える。
		Press(EKey::Tab);
		Step();
		auto* Player3D = &Scene3D().GetPlayer().Get()->GetCharacter();
		Check(Player3D->GetCenter() == Toolbox::FVector3{StartX, StartY, 0}, "3D start state");
		// 3D: 右へ歩いて低い段差を上る。
		Hold(EKey::D, true);
		bSteppedUp = false;
		for (Toolbox::int32 Frame = 0; Frame < 60; ++Frame)
		{
			Step();
			bSteppedUp = bSteppedUp || Player3D->GetLastStep().bSteppedUp;
		}
		Hold(EKey::D, false);
		Check(bSteppedUp, "3D step up");
		// 3D: 奥の壁と右端の壁の角へ斜めに進むと、二つの壁の稜線で止まる（各面から接触余裕0.02）。
		Player3D->Teleport({22, 0.52f, 2});
		Hold(EKey::D, true);
		Hold(EKey::W, true);
		for (Toolbox::int32 Frame = 0; Frame < 120; ++Frame)
		{
			Step();
		}
		Hold(EKey::D, false);
		Hold(EKey::W, false);
		const Toolbox::FVector3 Corner = Player3D->GetCenter();
		Check(Toolbox::Abs(Corner.X - 24.48f) < 1e-3f && Toolbox::Abs(Corner.Z - 3.98f) < 1e-3f &&
		          Player3D->IsGrounded(),
		      "3D corner stop");
		// 3D: 描画位置の画素はプレイヤーの色（陰影があるので黄色系で照合）。
		for (Toolbox::int32 Frame = 0; Frame < 10; ++Frame)
		{
			Step();
		}
		const auto Projected = ProjectWorldToScreen(Scene3D().GetView(0), 1280, 720, Player3D->GetRenderCenter());
		Check(Projected && Projected.Value().bInsideView, "3D projection");
		const FColor Pixel3D = Capture(Output / "corner3d.png", Projected.Value().Screen);
		Check(Pixel3D.R > 150 && Pixel3D.G > 100 && Pixel3D.B < 110 && Pixel3D.R > Pixel3D.B + 80,
		      "3D player pixel mismatch");
		// 3D: 2画面表示でも固定更新の回数は同じ。
		const Toolbox::int64 Single = Player3D->GetStepCount();
		for (Toolbox::int32 Frame = 0; Frame < 60; ++Frame)
		{
			Step();
		}
		const Toolbox::int64 SingleSteps = Player3D->GetStepCount() - Single;
		Press(EKey::V);
		Check(Scene3D().IsSplit(), "3D split view");
		const Toolbox::int64 Split = Player3D->GetStepCount();
		for (Toolbox::int32 Frame = 0; Frame < 60; ++Frame)
		{
			Step();
		}
		const Toolbox::int64 SplitSteps = Player3D->GetStepCount() - Split;
		Check(SingleSteps == 60 && SplitSteps == 60, "fixed steps changed with the number of views");
		Capture(Output / "split3d.png", {640, 360});
		// 2Dへ戻ると新しいシーン（開始位置）になる。
		Press(EKey::Tab);
		Step();
		Player2D = &Scene2D().GetPlayer().Get()->GetCharacter();
		Check(Player2D->GetCenter() == Toolbox::FVector2{StartX, StartY}, "2D re-entry");
		Input.State.Keys[static_cast<Toolbox::size_t>(EKey::Escape)] = true;
		const auto Quit = App.Step(Time += 1.0 / 60.0);
		Check(Quit && !Quit.Value(), "quit failed");
		Toolbox::Out << "GAMEPLAY_SAMPLE_PASSED\n";
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << "\n";
		return 1;
	}
}
