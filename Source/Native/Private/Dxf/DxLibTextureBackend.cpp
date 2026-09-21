#include "Dxf/DxLibTextureBackend.h"
#include "NativeApi.h"
namespace Dxf
{
// 画像を読み込みテクスチャを取得する。
// @param Path 読み込むファイルのパス。
// @param Options 処理に適用する設定。
TResult<FTextureAllocation> FDxLibTextureBackend::LoadTexture(const Toolbox::FString& Path,
                                                              const FTextureLoadOptions& Options)
{
	// ハンドル。
	const Toolbox::int32 Handle = DxLib::LoadGraph(Path.CStr(), Options.bUse3D ? FALSE : TRUE);
	if (Handle < 0)
	{
		return TResult<FTextureAllocation>::Failure(EErrorCode::NotFound, "LoadGraph failed: " + Path);
	}
	// 幅。
	Toolbox::int32 Width = 0;
	// テクスチャの高さ。
	Toolbox::int32 Height = 0;
	if (DxLib::GetGraphSize(Handle, &Width, &Height) < 0 || Width <= 0 || Height <= 0)
	{
		DxLib::DeleteGraph(Handle);
		return TResult<FTextureAllocation>::Failure(EErrorCode::BackendFailure, "GetGraphSize failed: " + Path);
	}
	return TResult<FTextureAllocation>::Success({Handle, Width, Height});
}
// 準備済み画像データからテクスチャを取得する。ファイルを読まない。
// @param Data 画像ファイルのバイト列。呼び出し中だけ有効。
// @param Size バイト列の長さ。
// @param Options 処理に適用する設定。
TResult<FTextureAllocation> FDxLibTextureBackend::LoadTextureMemory(const void* Data, Toolbox::size_t Size,
                                                                   const FTextureLoadOptions& Options)
{
	if (Data == nullptr || Size == 0 ||
	    Size > static_cast<Toolbox::size_t>(Toolbox::TNumericLimits<Toolbox::int32>::Max()))
	{
		return TResult<FTextureAllocation>::Failure(EErrorCode::InvalidArgument, "Invalid image memory range");
	}
	// LoadGraphのNotUse3DFlagとは逆向きの指定。どちらも既定で3D利用の形式になる。
	const Toolbox::int32 Handle = DxLib::CreateGraphFromMem(Data, static_cast<Toolbox::int32>(Size), nullptr, 0,
	                                                        Options.bUse3D ? TRUE : FALSE, FALSE);
	if (Handle < 0)
	{
		return TResult<FTextureAllocation>::Failure(EErrorCode::BackendFailure, "CreateGraphFromMem failed");
	}
	// 幅。
	Toolbox::int32 Width = 0;
	// テクスチャの高さ。
	Toolbox::int32 Height = 0;
	if (DxLib::GetGraphSize(Handle, &Width, &Height) < 0 || Width <= 0 || Height <= 0)
	{
		DxLib::DeleteGraph(Handle);
		return TResult<FTextureAllocation>::Failure(EErrorCode::BackendFailure, "GetGraphSize failed");
	}
	return TResult<FTextureAllocation>::Success({Handle, Width, Height});
}
// 描画先として使うテクスチャを生成する。
// @param Width 幅。
// @param Height 高さ。
// @param bAlpha 透過を扱う描画先を生成するか。
TResult<FTextureAllocation> FDxLibTextureBackend::CreateRenderTarget(Toolbox::int32 Width, Toolbox::int32 Height,
                                                                     bool bAlpha)
{
	if (Width <= 0 || Height <= 0)
	{
		return TResult<FTextureAllocation>::Failure(EErrorCode::InvalidArgument, "Invalid render target dimensions");
	}
	// ハンドル。
	const Toolbox::int32 Handle = DxLib::MakeScreen(Width, Height, bAlpha ? TRUE : FALSE);
	if (Handle < 0)
	{
		return TResult<FTextureAllocation>::Failure(EErrorCode::BackendFailure, "MakeScreen failed");
	}
	return TResult<FTextureAllocation>::Success({Handle, Width, Height});
}
// ネイティブテクスチャを解放する。
// @param Handle ハンドル。
void FDxLibTextureBackend::DeleteTexture(Toolbox::int32 Handle) noexcept
{
	if (Handle >= 0)
	{
		DxLib::DeleteGraph(Handle);
	}
}
} // namespace Dxf
