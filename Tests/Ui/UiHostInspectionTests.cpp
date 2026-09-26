// SPDX-License-Identifier: NOASSERTION
// W4: 表示先ごとの検査（同じルートを倍率の異なる表示先へ出す場合と、画面の切り抜きを持つ2Dワールドのパネル）。
#include "Ui/UiRuntimeTestSupport.h"
using namespace UiTest;

TEST("UI host inspects the same root per display with its own surface and display clip")
{
	FUiRoot Root;
	FullButton(Root);
	FUiSceneHost Host;
	FUiDisplayOptions Double = PixelOptions();
	Double.Scale.UserScale = 2;
	const auto Screen = Host.AddScreen(Root, PixelOptions());
	const auto Viewport = Host.AddViewport(Root, {641, 0, 1280, 719}, Double);
	FUiWorldPanel2D Panel;
	Panel.WorldTopLeft = {0, 2};
	Panel.WorldSize = {4, 2};
	Panel.Transform.Origin = {100, 300};
	Panel.Transform.PixelsPerUnit = 50;
	// 画面の切り抜きがパネルの右半分を切る。
	Panel.ScreenClip = {0, 0, 200, 720};
	const auto World = Host.AddWorldPanel2D(Root, Panel, PixelOptions());
	auto First = Host.InspectDisplay(Screen);
	REQUIRE(First);
	REQUIRE(First.Value().DisplayId == Screen && First.Value().Scale == 1);
	REQUIRE(First.Value().SurfacePixels.Right == 1280 && First.Value().LogicalSize.Width == 1280);
	auto Second = Host.InspectDisplay(Viewport);
	REQUIRE(Second);
	REQUIRE(Second.Value().DisplayId == Viewport && Second.Value().Scale == 2);
	REQUIRE(Second.Value().SurfacePixels.Left == 641 && Second.Value().LogicalSize.Width == 319.5f);
	// 表示先の切り抜きは命令と検査の両方に掛かるため、正しい命令は問題にならない。
	auto Third = Host.InspectDisplay(World);
	REQUIRE(Third);
	REQUIRE(Third.Value().DisplayId == World && Third.Value().SurfacePixels.Width() == 200);
	for (const auto& Issue : Third.Value().Issues)
	{
		REQUIRE(Issue.Kind != EUiInspectionKind::ClipFit && Issue.DisplayId == World);
	}
	REQUIRE(!Host.InspectDisplay(9999));
}
