// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_FAKE_DXLIB_WINDOW_API_H
#define DXF_FAKE_DXLIB_WINDOW_API_H
#include "Toolbox/Utility.h"
namespace DxLib
{
/**
 * ウィンドウの状態の代替（試験で直接変えて、利用者の拡縮・最小化・DPIを再現する）。
 */
struct FTestWindow
{
	/**
	 * クライアント領域と描画先の画素数。
	 */
	Toolbox::int32 ClientWidth = 640;
	Toolbox::int32 ClientHeight = 480;
	Toolbox::int32 DrawWidth = 640;
	Toolbox::int32 DrawHeight = 480;
	/**
	 * 最小化・DPI・拡縮の許可・引き伸ばし。
	 */
	bool bMinimized = false;
	Toolbox::int32 Dpi = 96;
	bool bSizeChangeEnabled = false;
	bool bFitScreen = false;
	bool bResetGraphics = true;
	/**
	 * 画面モードの変更の回数と、失敗させるか。
	 */
	Toolbox::int32 GraphModeCalls = 0;
	bool bFailGraphMode = false;
	/**
	 * 待機の回数。
	 */
	Toolbox::int32 Waits = 0;
	/**
	 * 前面か、マウスを捕捉しているか、捕捉・解除の回数、最後に設定したカーソルの名前。
	 */
	bool bForeground = true;
	bool bCaptured = false;
	Toolbox::int32 Captures = 0;
	Toolbox::int32 Releases = 0;
	Toolbox::size_t CursorId = 0;
};
inline FTestWindow TestWindow;
inline Toolbox::int32 SetWindowSizeChangeEnableFlag(Toolbox::int32 Flag, Toolbox::int32 FitScreen = 1)
{
	TestWindow.bSizeChangeEnabled = Flag != 0;
	TestWindow.bFitScreen = FitScreen != 0;
	return 0;
}
inline Toolbox::int32 SetChangeScreenModeGraphicsSystemResetFlag(Toolbox::int32 Flag)
{
	TestWindow.bResetGraphics = Flag != 0;
	return 0;
}
inline Toolbox::int32 SetWindowSizeExtendRate(double, double = -1.0)
{
	return 0;
}
inline Toolbox::int32 GetWindowSize(Toolbox::int32* Width, Toolbox::int32* Height)
{
	*Width = TestWindow.bMinimized ? 0 : TestWindow.ClientWidth;
	*Height = TestWindow.bMinimized ? 0 : TestWindow.ClientHeight;
	return 0;
}
inline Toolbox::int32 GetWindowMinSizeFlag()
{
	return TestWindow.bMinimized ? 1 : 0;
}
inline Toolbox::int32 GetMonitorDpi(Toolbox::int32* X, Toolbox::int32* Y, Toolbox::int32 = -1)
{
	*X = TestWindow.Dpi;
	*Y = TestWindow.Dpi;
	return 0;
}
inline Toolbox::int32 WaitTimer(Toolbox::int32)
{
	++TestWindow.Waits;
	return 0;
}
} // namespace DxLib
#if !defined(_WINDOWS_)
// Win32のウィンドウ・カーソルの最小の代替（実SDKでは<windows.h>の宣言を使う）。
using HWND = void*;
using HCURSOR = void*;
using LPCWSTR = const wchar_t*;
using WORD = unsigned short;
#define MAKEINTRESOURCEW(Id) (reinterpret_cast<LPCWSTR>(static_cast<Toolbox::size_t>(Id)))
inline int FakeMainWindow = 0;
namespace DxLib
{
inline HWND GetMainWindowHandle()
{
	return &FakeMainWindow;
}
} // namespace DxLib
inline HWND GetCapture()
{
	return DxLib::TestWindow.bCaptured ? DxLib::GetMainWindowHandle() : nullptr;
}
inline HWND GetForegroundWindow()
{
	return DxLib::TestWindow.bForeground ? DxLib::GetMainWindowHandle() : nullptr;
}
inline HWND SetCapture(HWND)
{
	++DxLib::TestWindow.Captures;
	DxLib::TestWindow.bCaptured = true;
	return nullptr;
}
inline int ReleaseCapture()
{
	++DxLib::TestWindow.Releases;
	DxLib::TestWindow.bCaptured = false;
	return 1;
}
inline HCURSOR LoadCursorW(void*, LPCWSTR Name)
{
	return const_cast<wchar_t*>(Name);
}
inline HCURSOR SetCursor(HCURSOR Cursor)
{
	DxLib::TestWindow.CursorId = reinterpret_cast<Toolbox::size_t>(Cursor);
	return nullptr;
}
#endif
#endif
