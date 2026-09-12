#include "Dxf/DxLibFontBackend.h"
#include "NativeApi.h"
namespace Dxf
{
/**
 * ネイティブフォントを生成する。
 * @param Options 処理に適用する設定。
 */
TResult<Toolbox::int32> FDxLibFontBackend::CreateFont(const FFontOptions& Options)
{
	/**
	 * ハンドル。
	 */
	const Toolbox::int32 Handle =
	    DxLib::CreateFontToHandle(Options.Family.CStr(), Options.Size, Options.Thickness,
	                              Options.bAntialias ? DX_FONTTYPE_ANTIALIASING_4X4 : DX_FONTTYPE_NORMAL);
	return Handle < 0 ? TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "CreateFontToHandle failed")
	                  : TResult<Toolbox::int32>::Success(Handle);
}
/**
 * ネイティブフォントを解放する。
 * @param Handle ハンドル。
 */
void FDxLibFontBackend::DeleteFont(Toolbox::int32 Handle) noexcept
{
	if (Handle >= 0)
	{
		DxLib::DeleteFontToHandle(Handle);
	}
}
} // namespace Dxf
