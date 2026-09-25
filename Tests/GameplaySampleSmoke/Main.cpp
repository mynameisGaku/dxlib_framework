// SPDX-License-Identifier: NOASSERTION
// 入力だけを固定し、キャラクター移動のサンプル（2D／3D）を実Application・実DxLibで操作する。
// 2D／3Dの両方で同じ受け入れ（SmokeAcceptance）と、シーンの切替・再入場・2画面の表示を確かめて終了し、
// 1画面／2画面・索引あり／総当たりの軌跡の一致（SmokeTraces。各回を新しいApplicationで起動）と、終了後の再起動を確かめる。
#include "SmokeAcceptance.h"
#include "SmokeTraces.h"
// Windowsの文字種マクロとToolboxの同名関数を分離する。
#ifdef CreateDirectory
#undef CreateDirectory
#endif
Toolbox::int32 main(Toolbox::int32 Count, char** Args)
{
	using namespace Dxf;
	using namespace Dxf::GameplaySmoke;
	if (Count != 3)
	{
		Toolbox::Err << "Usage: NativeGameplaySmoke <ProjectRoot> <output directory>\n";
		return 2;
	}
	try
	{
		// ネイティブ窓口はApplicationより長く生存する。
		FDxLibBackends Backends;
		const Toolbox::FPath Output(Args[2]);
		Check(Toolbox::IsDirectory(Output) || Toolbox::CreateDirectory(Output), "output directory failed");
		{
			FSmokeApp App(Backends, Args[1]);
			App.Start();
			// 2Dの受け入れ。
			RunAcceptance2D(App, Output);
			// 3Dへ切り替えて、3Dの受け入れ。
			App.SwitchDimension();
			RunAcceptance3D(App, Output);
			// 2画面の表示を保存する。
			App.Press(EKey::V);
			Check(App.Scene3D().IsSplit(), "3D split view");
			App.Step();
			(void)App.Capture(Output / "split3d.png", {640, 360});
			// 2Dへ戻ると新しいシーン（開始位置）になる。2画面の表示を保存する。
			App.SwitchDimension();
			const Toolbox::FVector2 Start{GameplaySample::StartX, GameplaySample::StartY};
			Check(App.Scene2D().GetPlayer().Get()->GetCharacter().GetCenter() == Start, "2D re-entry");
			App.Press(EKey::V);
			App.Step();
			Check(App.Scene2D().IsSplit(), "2D split view");
			(void)App.Capture(Output / "split2d.png", {320, 360});
			App.Quit();
		}
		// 軌跡の一致（それぞれ新しいApplicationで3回ずつ起動・終了する）。
		RunTraceComparisons2D(Backends, Args[1]);
		RunTraceComparisons3D(Backends, Args[1]);
		{
			// 終了後の再起動: 新しいApplicationで開始し、歩いて終了できる。
			FSmokeApp App(Backends, Args[1]);
			App.Start();
			App.Hold(EKey::D, true);
			for (Toolbox::int32 Frame = 0; Frame < 30; ++Frame)
			{
				App.Step();
			}
			App.Hold(EKey::D, false);
			const auto& Character = App.Scene2D().GetPlayer().Get()->GetCharacter();
			Check(Character.GetStepCount() >= 30 && Character.GetCenter().X > GameplaySample::StartX, "restart walk");
			App.Quit();
		}
		Toolbox::Out << "GAMEPLAY_SAMPLE_PASSED\n";
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << "\n";
		return 1;
	}
}
