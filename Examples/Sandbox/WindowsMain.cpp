#include "Toolbox/UniquePtr.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/AppRunner.h"
#include "SandboxGame.h"
#include "../Shared/ProjectRootEntry.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "Toolbox/Platform.h"
#include "Toolbox/Utility.h"
#include "Toolbox/Vector.h"
namespace
{
// 起動または実行時のエラーを表示する。
// @param Message エラーの説明。
void ShowError_Internal(const Toolbox::FString& Message)
{
	if (Message.Size() > static_cast<Toolbox::size_t>(Toolbox::TNumericLimits<Toolbox::int32>::Max()))
	{
		return;
	}
	// ベクトルまたは文字列の長さ。
	const Toolbox::int32 Length = static_cast<Toolbox::int32>(Message.Size());
	// 要素数。
	const Toolbox::int32 Count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Message.Data(), Length, nullptr, 0);
	// 描画する文字列。
	Toolbox::FWideString Text = L"Application failed. Inspect the DxLib Log.txt file.";
	if (Count > 0)
	{
		Text.Resize(static_cast<Toolbox::size_t>(Count));
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Message.Data(), Length, Text.Data(), Count);
	}
	MessageBoxW(nullptr, Text.CStr(), L"dxlib_framework", MB_OK | MB_ICONERROR);
}
} // namespace
// Windowsアプリケーションを起動し、終了までサンプルを実行する。
Toolbox::int32 WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, Toolbox::int32)
{
	try
	{
		// 各ネイティブ機能の実装。
		Dxf::FDxLibBackends Backends;
		// 初期化に使用する設定。
		Dxf::FApplicationSettings Settings;
		Settings.Window.Title = "dxlib_framework - Sandbox";
		// sln配置先を基準にするアセットの起点。開発パス設定がなければexe配置先を使う。
		const Toolbox::FPath ProjectRoot = Dxf::ResolveEntryProjectRoot("Sandbox.dxfpaths");
		Settings.ProjectRoot = ProjectRoot.ToUtf8();
		// サービスを結合した実行用のアプリケーション。
		Dxf::FApplication Application(Backends.GetServices(), Settings,
		                              Toolbox::MakeUnique<Dxf::Sandbox::DSandboxGameInstance>());
		// アプリケーションの実行器。
		Dxf::FAppRunner Runner;
		// 処理結果。
		auto Result = Runner.Run(
		    Application, Toolbox::MakeUnique<Dxf::Sandbox::DSandboxScene>(ProjectRoot.ToUtf8() + "/Assets"));
		if (!Result)
		{
			ShowError_Internal(Result.Error().Message);
			return 1;
		}
		return 0;
	}
	// 捕捉した例外の内容をダイアログへ表示する。
	catch (const Toolbox::FException& Error)
	{
		ShowError_Internal(Error.What());
		return 1;
	}
	catch (...)
	{
		ShowError_Internal("Unknown application error");
		return 1;
	}
}
