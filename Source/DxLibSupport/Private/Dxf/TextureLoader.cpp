#include "Dxf/TextureLoader.h"
#include "Dxf/Utf8.h"

namespace Dxf
{
// ネイティブリソースの所有権を引き受ける。
// @param Allocation ネイティブリソースの確保結果。
// @param bRenderTarget 描画先として確保したリソースか。
TResult<Toolbox::TSharedPtr<FTextureResource>> FTextureLoader::Adopt_Internal(FTextureAllocation Allocation,
                                                                              bool bRenderTarget,
                                                                              bool bPremultipliedAlpha)
{
	// ネイティブハンドルの解放を保証する所有者。
	// @param Context 処理に必要な実行環境。
	// @param Value 処理対象の値。
	FNativeHandle Handle(Allocation.NativeHandle, m_pBackend,
	                     [](void* Context, Toolbox::int32 Value) noexcept
	                     {
		                     static_cast<ITextureBackend*>(Context)->DeleteTexture(Value);
	                     });
	if (Allocation.NativeHandle < 0 || Allocation.Width <= 0 || Allocation.Height <= 0)
	{
		return TResult<Toolbox::TSharedPtr<FTextureResource>>::Failure(EErrorCode::BackendFailure,
		                                                               "Invalid texture allocation");
	}
	// 共有するリソース。
	auto Resource = Toolbox::MakeShared<FTextureResource>(
	    Toolbox::Move(Handle),
	    FTextureMetadata{Allocation.Width, Allocation.Height, bRenderTarget, bPremultipliedAlpha});
	if (!m_pRegistry->Register(Resource))
	{
		return TResult<Toolbox::TSharedPtr<FTextureResource>>::Failure(EErrorCode::InvalidState,
		                                                               "Resource registry stopped");
	}
	return TResult<Toolbox::TSharedPtr<FTextureResource>>::Success(Toolbox::Move(Resource));
}
// 対象のリソースを読み込む。
// @param Path 読み込むファイルのパス。
// @param Options 処理に適用する設定。
TResult<FTexture> FTextureLoader::Load(const Toolbox::FString& Path, const FTextureLoadOptions& Options)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Detail::IsValidNativeString_Internal(Path))
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidArgument, "Path must be nonempty UTF-8 without NUL");
	}
	// ネイティブリソースの確保結果。
	auto Allocation = m_pBackend->LoadTexture(Path, Options);
	if (!Allocation)
	{
		return TResult<FTexture>::Failure(Allocation.Error());
	}
	// 共有するリソース。
	auto Resource = Adopt_Internal(Allocation.Value(), false, Options.bPremultipliedAlpha);
	return Resource ? TResult<FTexture>::Success(FTexture(Toolbox::Move(Resource).Value()))
	                : TResult<FTexture>::Failure(Resource.Error());
}
// 準備済みデータからリソースを読み込む。ファイルを読まない。
// @param Data 画像ファイルのバイト列。呼び出し中だけ有効。
// @param Size バイト列の長さ。
// @param Options 処理に適用する設定。
TResult<FTexture> FTextureLoader::LoadMemory(const void* Data, Toolbox::size_t Size,
                                             const FTextureLoadOptions& Options)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (Data == nullptr || Size == 0)
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidArgument, "Image memory is empty");
	}
	// ネイティブリソースの確保結果。
	auto Allocation = m_pBackend->LoadTextureMemory(Data, Size, Options);
	if (!Allocation)
	{
		return TResult<FTexture>::Failure(Allocation.Error());
	}
	// 共有するリソース。
	auto Resource = Adopt_Internal(Allocation.Value(), false, Options.bPremultipliedAlpha);
	return Resource ? TResult<FTexture>::Success(FTexture(Toolbox::Move(Resource).Value()))
	                : TResult<FTexture>::Failure(Resource.Error());
}
// 描画先として使うテクスチャを生成する。
// @param Width 幅。
// @param Height 高さ。
// @param bAlpha 透過を扱う描画先を生成するか。
TResult<FRenderTarget> FTextureLoader::CreateRenderTarget(Toolbox::int32 Width, Toolbox::int32 Height, bool bAlpha)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FRenderTarget>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (Width <= 0 || Height <= 0)
	{
		return TResult<FRenderTarget>::Failure(EErrorCode::InvalidArgument, "Invalid target size");
	}
	// ネイティブリソースの確保結果。
	auto Allocation = m_pBackend->CreateRenderTarget(Width, Height, bAlpha);
	if (!Allocation)
	{
		return TResult<FRenderTarget>::Failure(Allocation.Error());
	}
	// 共有するリソース。
	auto Resource = Adopt_Internal(Allocation.Value(), true);
	return Resource ? TResult<FRenderTarget>::Success(FRenderTarget(Toolbox::Move(Resource).Value()))
	                : TResult<FRenderTarget>::Failure(Resource.Error());
}
} // namespace Dxf
