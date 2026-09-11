#include "Dxf/DxLibPlatform.h"
#include "NativeApi.h"
namespace Dxf
{
namespace
{
// DxLib is process-global. Framework operations, including this lease, are main-thread-only.
FDxLibPlatform* GSessionOwner = nullptr;
}
FDxLibPlatform::~FDxLibPlatform()
{
    Shutdown();
}
TResult<void> FDxLibPlatform::Initialize(const FWindowSettings& Settings)
{
    if (m_bInitialized || GSessionOwner != nullptr)
    {
        return TResult<void>::Failure(EErrorCode::InvalidState, "Another DxLib session is active");
    }
    if (Settings.Width <= 0 || Settings.Height <= 0)
    {
        return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid window dimensions");
    }
    if (DxLib::SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8) < 0 ||
        DxLib::SetMainWindowText(Settings.Title.c_str()) < 0 ||
        DxLib::ChangeWindowMode(Settings.bWindowed ? TRUE : FALSE) < 0 ||
        DxLib::SetGraphMode(Settings.Width, Settings.Height, 32) < 0 ||
        DxLib::SetWaitVSyncFlag(Settings.bVSync ? TRUE : FALSE) < 0 ||
        DxLib::SetAlwaysRunFlag(TRUE) < 0)
    {
        return TResult<void>::Failure(EErrorCode::BackendFailure, "DxLib window configuration failed");
    }
    GSessionOwner = this;
    if (DxLib::DxLib_Init() < 0)
    {
        DxLib::DxLib_End();
        GSessionOwner = nullptr;
        return TResult<void>::Failure(EErrorCode::InitializationFailed, "DxLib_Init failed; inspect Log.txt");
    }
    m_bInitialized = true;
    return {};
}
void FDxLibPlatform::Shutdown() noexcept
{
    if (m_bInitialized)
    {
        DxLib::DxLib_End();
        m_bInitialized = false;
        GSessionOwner = nullptr;
    }
}
TResult<bool> FDxLibPlatform::PumpEvents()
{
    if (!m_bInitialized)
    {
        return TResult<bool>::Failure(EErrorCode::InvalidState, "DxLib session is not initialized");
    }
    return TResult<bool>::Success(DxLib::ProcessMessage() == 0);
}
}
