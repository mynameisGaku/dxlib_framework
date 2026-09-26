#include "Dxf/DxLibPlatform.h"
#include "NativeApi.h"
#include "Dxf/Utf8.h"
#include "Toolbox/Log.h"
#if defined(DXF_DXLIB_MODEL_EXTENSION) && DXF_DXLIB_MODEL_EXTENSION >= 3
extern "C" void DxfReleasePbr();
#endif
namespace Dxf
{
namespace
{
// DxLibはプロセス全体で共有する。この所有権を含む操作はメインスレッドに限定する。
//
// プロセス内でDxLibを使用中の所有者。
FDxLibPlatform* GSessionOwner = nullptr;
} // namespace
// 所有する状態を終了し、必要なリソースを解放する。
FDxLibPlatform::~FDxLibPlatform()
{
	Shutdown();
}
// 使用に必要な初期化を行う。
// @param Settings 初期化に使用する設定。
TResult<void> FDxLibPlatform::Initialize(const FWindowSettings& Settings)
{
	DXF_LOG_INFO("NativeLifecycle", "Initialize platform=%p initialized=%d owner=%p", static_cast<void*>(this), m_bInitialized, static_cast<void*>(GSessionOwner));
	if (m_bInitialized || GSessionOwner != nullptr)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Another DxLib session is active");
	}
	if (Settings.Width <= 0 || Settings.Height <= 0 || !Detail::IsValidNativeString_Internal(Settings.Title, true))
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid window dimensions or UTF-8 title");
	}
	if (DxLib::SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8) < 0 ||
	    DxLib::SetMainWindowText(Settings.Title.CStr()) < 0 ||
	    DxLib::ChangeWindowMode(Settings.bWindowed ? TRUE : FALSE) < 0 ||
	    DxLib::SetGraphMode(Settings.Width, Settings.Height, 32) < 0 ||
	    DxLib::SetWaitVSyncFlag(Settings.bVSync ? TRUE : FALSE) < 0 || DxLib::SetAlwaysRunFlag(TRUE) < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "DxLib window configuration failed");
	}
	// 拡縮を許す場合はウィンドウの端で拡縮でき、描画先を表示へ引き伸ばす（入力の位置は描画先の画素で届く）。
	// 描画先の寸法を変える場合は、画面モードの変更で画像・フォント等の資源を失わない設定にする。
	if (Settings.Resize != EWindowResizeMode::Fixed && (DxLib::SetWindowSizeChangeEnableFlag(TRUE, TRUE) < 0 ||
	                                                    (Settings.Resize == EWindowResizeMode::Resizable &&
	                                                     DxLib::SetChangeScreenModeGraphicsSystemResetFlag(FALSE) < 0)))
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "DxLib window resize configuration failed");
	}
	m_Resize = Settings.Resize;
	m_bVSync = Settings.bVSync;
	m_State = {};
	GSessionOwner = this;
	// 同じ初期化呼出しの戻り値を後始末より先に残す。
	const Toolbox::int32 Initialized = DxLib::DxLib_Init();
	DXF_LOG_INFO("NativeLifecycle", "DxLib_Init platform=%p result=%d", static_cast<void*>(this), Initialized);
	if (Initialized < 0)
	{
#if defined(DXF_DXLIB_MODEL_EXTENSION) && DXF_DXLIB_MODEL_EXTENSION >= 3
		DxfReleasePbr();
#endif
		DXF_LOG_INFO("NativeLifecycle", "DxLib_End begin platform=%p initialized=%d", static_cast<void*>(this), m_bInitialized);
		// 同じ終了呼出しの結果。追加のSDK呼出しは行わない。
		const Toolbox::int32 Ended = DxLib::DxLib_End();
		DXF_LOG_INFO("NativeLifecycle", "DxLib_End end platform=%p result=%d", static_cast<void*>(this), Ended);
		GSessionOwner = nullptr;
		return TResult<void>::Failure(EErrorCode::InitializationFailed, "DxLib_Init failed; inspect Log.txt");
	}
	m_bInitialized = true;
	return {};
}
// 管理する処理とリソースを順序どおり終了する。
void FDxLibPlatform::Shutdown() noexcept
{
	if (m_bInitialized)
	{
#if defined(DXF_DXLIB_MODEL_EXTENSION) && DXF_DXLIB_MODEL_EXTENSION >= 3
		DxfReleasePbr();
#endif
		DXF_LOG_INFO("NativeLifecycle", "DxLib_End begin platform=%p initialized=%d", static_cast<void*>(this), m_bInitialized);
		// 同じ終了呼出しの結果。追加のSDK呼出しは行わない。
		const Toolbox::int32 Ended = DxLib::DxLib_End();
		DXF_LOG_INFO("NativeLifecycle", "DxLib_End end platform=%p result=%d", static_cast<void*>(this), Ended);
		m_bInitialized = false;
		GSessionOwner = nullptr;
	}
}
// フレームの境界でウィンドウの状態を確定する。
TResult<FWindowState> FDxLibPlatform::SyncWindow()
{
	if (!m_bInitialized)
	{
		return TResult<FWindowState>::Failure(EErrorCode::InvalidState, "DxLib session is not initialized");
	}
	FWindowState Next;
	Next.bKnown = true;
	Toolbox::int32 ClientWidth = 0;
	Toolbox::int32 ClientHeight = 0;
	if (DxLib::GetWindowSize(&ClientWidth, &ClientHeight) < 0)
	{
		return TResult<FWindowState>::Failure(EErrorCode::BackendFailure, "Window client size query failed");
	}
	Next.bMinimized = DxLib::GetWindowMinSizeFlag() == TRUE || ClientWidth <= 0 || ClientHeight <= 0;
	Next.bFocused = DxLib::GetWindowActiveFlag() == TRUE;
	Next.ClientWidth = Next.bMinimized ? 0 : ClientWidth;
	Next.ClientHeight = Next.bMinimized ? 0 : ClientHeight;
	Toolbox::int32 DpiX = 0;
	Toolbox::int32 DpiY = 0;
	Next.Dpi = DxLib::GetMonitorDpi(&DpiX, &DpiY) >= 0 && DpiX > 0 ? DpiX : 96;
	Toolbox::int32 DrawWidth = 0;
	Toolbox::int32 DrawHeight = 0;
	if (DxLib::GetDrawScreenSize(&DrawWidth, &DrawHeight) < 0)
	{
		return TResult<FWindowState>::Failure(EErrorCode::BackendFailure, "Draw screen size query failed");
	}
	if (m_Resize == EWindowResizeMode::Resizable && !Next.bMinimized &&
	    (DrawWidth != ClientWidth || DrawHeight != ClientHeight))
	{
		// 引き伸ばしの倍率を等倍へ戻してから、描画先をクライアント領域と同じ画素数にする（描画中の命令はない境界）。
		// 失敗した場合は以前の状態を保って失敗を返す。
		if (DxLib::SetWindowSizeExtendRate(1.0) < 0 || DxLib::SetGraphMode(ClientWidth, ClientHeight, 32) != 0 ||
		    DxLib::SetDrawScreen(DX_SCREEN_BACK) < 0 || DxLib::SetWaitVSyncFlag(m_bVSync ? TRUE : FALSE) < 0 ||
		    DxLib::GetDrawScreenSize(&DrawWidth, &DrawHeight) < 0 || DrawWidth != ClientWidth ||
		    DrawHeight != ClientHeight)
		{
			return TResult<FWindowState>::Failure(EErrorCode::BackendFailure,
			                                      Toolbox::FString("Render size change to ") +
			                                          Toolbox::ToString(ClientWidth) + "x" +
			                                          Toolbox::ToString(ClientHeight) + " failed");
		}
	}
	Next.RenderWidth = DrawWidth;
	Next.RenderHeight = DrawHeight;
	Next.ResizeMode = m_Resize;
	// マウスの捕捉は自アプリのウィンドウが実際に持っているかを返す（要求したこととは別）。
	const HWND Window = DxLib::GetMainWindowHandle();
	Next.bPointerCaptureSupported = Window != nullptr;
	Next.bPointerCaptured = Window != nullptr && GetCapture() == Window;
	Next.Revision = Next.IsSameState(m_State) ? m_State.Revision : m_State.Revision + 1;
	m_State = Next;
	if (Next.bMinimized)
	{
		// 表示できない間は短く待ち、描画のない高速な空回りを避ける。
		(void)DxLib::WaitTimer(16);
	}
	return TResult<FWindowState>::Success(Next);
}
// フレームの更新が求めた操作を反映する。
TResult<void> FDxLibPlatform::ApplyRequests(const FPlatformRequests& Requests)
{
	if (!m_bInitialized)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "DxLib session is not initialized");
	}
	const HWND Window = DxLib::GetMainWindowHandle();
	if (Window == nullptr)
	{
		return {};
	}
	// 捕捉は自アプリのウィンドウだけ。前面へ強制的に移したりはしない（取得できなければ次のフレームで喪失として扱う）。
	// 自分で解除した場合もWM_CAPTURECHANGEDが届くが、ここで取り直すことはしない（解除は冪等）。
	if (Requests.bPointerCapture && GetCapture() != Window && GetForegroundWindow() == Window)
	{
		(void)SetCapture(Window);
	}
	else if (!Requests.bPointerCapture && GetCapture() == Window)
	{
		(void)ReleaseCapture();
	}
	if (Requests.bResizeModeRequested && Requests.ResizeMode != m_Resize)
	{
		// 拡縮の許可と表示の引き伸ばしを切り替える。描画先の寸法の変更は次のSyncWindowの境界で行う。
		const bool bResizable = Requests.ResizeMode != EWindowResizeMode::Fixed;
		if (DxLib::SetWindowSizeChangeEnableFlag(bResizable ? TRUE : FALSE, TRUE) < 0 ||
		    (Requests.ResizeMode == EWindowResizeMode::Resizable &&
		     DxLib::SetChangeScreenModeGraphicsSystemResetFlag(FALSE) < 0))
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "Window resize mode change failed");
		}
		m_Resize = Requests.ResizeMode;
	}
	if (Requests.bCursorRequested)
	{
		// 標準のシステムカーソルの番号（IDC_ARROW・IDC_HAND・IDC_SIZEWE・IDC_SIZENS）。
		WORD Id = 32512;
		switch (Requests.Cursor)
		{
		case ECursorShape::Hand:
			Id = 32649;
			break;
		case ECursorShape::ResizeHorizontal:
			Id = 32644;
			break;
		case ECursorShape::ResizeVertical:
			Id = 32645;
			break;
		default:
			break;
		}
		// 標準カーソルだけを使う（共有のカーソルは解放しない）。
		(void)SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(Id)));
	}
	return {};
}
// OSイベントを処理し継続可否を返す。
TResult<bool> FDxLibPlatform::PumpEvents()
{
	if (!m_bInitialized)
	{
		return TResult<bool>::Failure(EErrorCode::InvalidState, "DxLib session is not initialized");
	}
	// 終了を返した一回の値だけを記録し、成功フレームのログは増やさない。
	const Toolbox::int32 Message = DxLib::ProcessMessage();
	if (Message != 0)
	{
		DXF_LOG_INFO("NativeLifecycle", "ProcessMessage platform=%p initialized=%d result=%d", static_cast<void*>(this), m_bInitialized, Message);
	}
	return TResult<bool>::Success(Message == 0);
}
} // namespace Dxf
