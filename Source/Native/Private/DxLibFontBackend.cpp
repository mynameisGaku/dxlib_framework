#include "Dxf/DxLibFontBackend.h"
#include "NativeApi.h"
namespace Dxf
{
TResult<int> FDxLibFontBackend::CreateFont(const FFontOptions& Options)
{
	const int Handle = DxLib::CreateFontToHandle(Options.Family.c_str(), Options.Size, Options.Thickness,
		Options.bAntialias ? DX_FONTTYPE_ANTIALIASING_4X4 : DX_FONTTYPE_NORMAL);
	return Handle < 0 ? TResult<int>::Failure(EErrorCode::BackendFailure, "CreateFontToHandle failed") : TResult<int>::Success(Handle);
}
void FDxLibFontBackend::DeleteFont(int Handle) noexcept
{
	if (Handle >= 0)
	{
		DxLib::DeleteFontToHandle(Handle);
	}
}
}
