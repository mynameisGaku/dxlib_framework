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
