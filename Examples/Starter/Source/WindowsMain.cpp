// SPDX-License-Identifier: NOASSERTION
#include "BootScene.h"
#include "Dxf/AppRunner.h"
#include "Dxf/NativeBackends.h"
#include "Toolbox/Platform.h"
#include "../../Shared/ProjectRootEntry.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

// 空のウィンドウと最初のシーンを起動する。起動・実行に失敗した場合は1を返す。
// Windowsから渡される起動引数は使用しない。
Toolbox::int32 WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, Toolbox::int32)
{
	try
	{
		// ウィンドウ・入力・描画・音声をDxLibへ接続する実装。
		Dxf::FDxLibBackends Backends;
		// ゲームのウィンドウや背景色。必要に応じてここで変更する。
		Dxf::FApplicationSettings Settings;
		Settings.Window.Title = "My Game";
		// sln配置先を基準にするアセットの起点。開発パス設定がなければexe配置先を使う。
		Settings.ProjectRoot = Dxf::ResolveEntryProjectRoot("Starter.dxfpaths").ToUtf8();
		// シーンとゲーム用のサービスを管理する実行本体。
		Dxf::FApplication Application(Backends.GetServices(), Settings);
		// ウィンドウを閉じるまで更新と描画を繰り返す実行器。
		Dxf::FAppRunner Runner;
		// 最初のシーンから終了までの実行結果。
		const auto Result = Runner.Run(Application, Toolbox::MakeUnique<Starter::ABootScene>());
		if (!Result)
		{
			MessageBoxW(nullptr, Toolbox::ToWide(Result.Error().Message).CStr(), L"My Game", MB_OK | MB_ICONERROR);
			return 1;
		}
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		MessageBoxW(nullptr, Toolbox::ToWide(Error.What()).CStr(), L"My Game", MB_OK | MB_ICONERROR);
		return 1;
	}
	catch (...)
	{
		MessageBoxW(nullptr, L"Application failed. See the DxLib Log.txt file.", L"My Game", MB_OK | MB_ICONERROR);
		return 1;
	}
}
