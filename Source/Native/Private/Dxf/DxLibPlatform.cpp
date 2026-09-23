#include "Dxf/DxLibPlatform.h"
#include "NativeApi.h"
#include "Dxf/Utf8.h"
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
	if (DxLib::DxLib_Init() < 0)
	{
#if defined(DXF_DXLIB_MODEL_EXTENSION) && DXF_DXLIB_MODEL_EXTENSION >= 3
		DxfReleasePbr();
#endif
		DxLib::DxLib_End();
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
		DxLib::DxLib_End();
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
	return TResult<bool>::Success(DxLib::ProcessMessage() == 0);
}
} // namespace Dxf
