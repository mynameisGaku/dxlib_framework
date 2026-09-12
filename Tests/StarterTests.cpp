// SPDX-License-Identifier: NOASSERTION
#include "BootScene.h"
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/Application.h"

TEST("Starter runs an empty scene without assets and exits when its window closes")
{
	// 画像・音声・フォントが一切ない環境を再現する接続先。
	Dxf::Testing::FFakeBackend Backend;
	Backend.GetTrace().bFailTexture = true;
	// Starterの実際のシーンを動かすアプリケーション。
	Dxf::FApplication Application({Backend, Backend, Backend, Backend, Backend, Backend});
	REQUIRE(Application.Start(Toolbox::MakeUnique<Starter::ABootScene>()));
	REQUIRE(Application.GetScenes().GetCurrent()->TryCast<Starter::ABootScene>()->GetObjectCount() == 0);
	REQUIRE(Application.Step(0).Value());
	REQUIRE(Application.Step(0.016).Value());
	REQUIRE(Backend.GetTrace().TextureLoads == 0);
	REQUIRE(Backend.GetTrace().Sounds.IsEmpty());
	REQUIRE(Backend.GetTrace().Fonts.IsEmpty());
	REQUIRE(Backend.GetTrace().DrawHandles.IsEmpty());
	Backend.GetTrace().bQuit = true;
	// ウィンドウを閉じたときの正常終了結果。
	const auto Stopped = Application.Step(0.032);
	REQUIRE(Stopped && !Stopped.Value());
	REQUIRE(!Application.IsRunning());
}
