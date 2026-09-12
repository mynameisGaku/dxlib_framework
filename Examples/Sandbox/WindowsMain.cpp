#include "Toolbox/UniquePtr.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/AppRunner.h"
#include "SandboxGame.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "Toolbox/Platform.h"
#include "Toolbox/Utility.h"
#include "Toolbox/Vector.h"
namespace
{
/**
 * 実行ファイルを基準にアセットの場所を求める。
 */
Toolbox::FString GetAssetRoot_Internal()
{
	/**
	 * 文字列を受け取る作業領域。
	 */
	Toolbox::TVector<wchar_t> Buffer(512);
	for (;;)
	{
		/**
		 * 有効な要素数。
		 */
		const DWORD Size = GetModuleFileNameW(nullptr, Buffer.Data(), static_cast<DWORD>(Buffer.Size()));
		if (Size == 0)
		{
			throw Toolbox::FException("GetModuleFileNameW failed");
		}
		if (Size < Buffer.Size())
		{
			/**
			 * 基準ディレクトリ。
			 */
			const auto Root = (Toolbox::FPath(Toolbox::FWideString(Buffer.Data(), Size)).Parent() / L"Assets").ToUtf8();
			return {reinterpret_cast<const char*>(Root.Data()), Root.Size()};
		}
		if (Buffer.Size() >= 32768)
		{
			throw Toolbox::FException("Executable path is too long");
		}
		Buffer.Resize(Buffer.Size() * 2);
	}
}
/**
 * 起動または実行時のエラーを表示する。
 * @param Message エラーの説明。
 */
void ShowError_Internal(const Toolbox::FString& Message)
{
	if (Message.Size() > static_cast<Toolbox::size_t>(Toolbox::TNumericLimits<Toolbox::int32>::Max()))
	{
		return;
	}
	/**
	 * ベクトルまたは文字列の長さ。
	 */
	const Toolbox::int32 Length = static_cast<Toolbox::int32>(Message.Size());
	/**
	 * 要素数。
	 */
	const Toolbox::int32 Count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Message.Data(), Length, nullptr, 0);
	/**
	 * 描画する文字列。
	 */
	Toolbox::FWideString Text = L"Application failed. Inspect the DxLib Log.txt file.";
	if (Count > 0)
	{
		Text.Resize(static_cast<Toolbox::size_t>(Count));
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Message.Data(), Length, Text.Data(), Count);
	}
	MessageBoxW(nullptr, Text.CStr(), L"dxlib_framework", MB_OK | MB_ICONERROR);
}
} // namespace
/**
 * Windowsアプリケーションを起動し、終了までサンプルを実行する。
 */
Toolbox::int32 WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, Toolbox::int32)
{
	try
	{
		/**
		 * 各ネイティブ機能の実装。
		 */
		Dxf::FDxLibBackends Backends;
		/**
		 * 初期化に使用する設定。
		 */
		Dxf::FApplicationSettings Settings;
		Settings.Window.Title = "dxlib_framework - Sandbox";
		/**
		 * サービスを結合した実行用のアプリケーション。
		 */
		Dxf::FApplication Application(Backends.GetServices(), Settings,
		                              Toolbox::MakeUnique<Dxf::Sandbox::DSandboxGameInstance>());
		/**
		 * アプリケーションの実行器。
		 */
		Dxf::FAppRunner Runner;
		/**
		 * 処理結果。
		 */
		auto Result =
		    Runner.Run(Application, Toolbox::MakeUnique<Dxf::Sandbox::DSandboxScene>(GetAssetRoot_Internal()));
		if (!Result)
		{
			ShowError_Internal(Result.Error().Message);
			return 1;
		}
		return 0;
	}
	/**
	 * 捕捉した例外の内容をダイアログへ表示する。
	 */
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
