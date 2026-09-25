#include "Dxf/DxLibFontBackend.h"
#include "NativeApi.h"
namespace Dxf
{
// ネイティブフォントを生成する。
// @param Options 処理に適用する設定。
TResult<Toolbox::int32> FDxLibFontBackend::CreateFont(const FFontOptions& Options)
{
	// ハンドル。
	const Toolbox::int32 Handle =
	    DxLib::CreateFontToHandle(Options.Family.CStr(), Options.Size, Options.Thickness,
	                              Options.bAntialias ? DX_FONTTYPE_ANTIALIASING_4X4 : DX_FONTTYPE_NORMAL);
	return Handle < 0 ? TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "CreateFontToHandle failed")
	                  : TResult<Toolbox::int32>::Success(Handle);
}
// ネイティブフォントを解放する。
// @param Handle ハンドル。
void FDxLibFontBackend::DeleteFont(Toolbox::int32 Handle) noexcept
{
	if (Handle >= 0)
	{
		DxLib::DeleteFontToHandle(Handle);
	}
}
// 一行の描画幅を計測する。
// @param Handle フォントのハンドル。
// @param Text UTF-8の文字列。
TResult<Toolbox::int32> FDxLibFontBackend::MeasureTextWidth(Toolbox::int32 Handle, const Toolbox::FString& Text)
{
	if (Handle < 0 || Text.Size() > static_cast<Toolbox::size_t>(Toolbox::TNumericLimits<Toolbox::int32>::Max()))
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::InvalidArgument, "Invalid font handle or text length");
	}
	// 描画幅（文字数はバイト数で渡す。文字コードはDxLib_Initの前にUTF-8へ設定済み）。
	const Toolbox::int32 Width =
	    DxLib::GetDrawStringWidthToHandle(Text.CStr(), static_cast<Toolbox::int32>(Text.Size()), Handle, FALSE);
	return Width < 0 ? TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "GetDrawStringWidthToHandle failed")
	                 : TResult<Toolbox::int32>::Success(Width);
}
// 行の送りを取得する。
// @param Handle フォントのハンドル。
TResult<Toolbox::int32> FDxLibFontBackend::GetFontLineHeight(Toolbox::int32 Handle)
{
	if (Handle < 0)
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::InvalidArgument, "Invalid font handle");
	}
	const Toolbox::int32 Height = DxLib::GetFontLineSpaceToHandle(Handle);
	return Height < 0 ? TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "GetFontLineSpaceToHandle failed")
	                  : TResult<Toolbox::int32>::Success(Height);
}
} // namespace Dxf
