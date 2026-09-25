#include "Dxf/FontLoader.h"
#include "Dxf/Utf8.h"

namespace Dxf
{
// 一行の文字列の描画幅を計測する。
TResult<Toolbox::int32> FFontLoader::MeasureTextWidth(const FFont& Font, const Toolbox::FString& Text) const
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Font.IsValid())
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::InvalidState, "Invalid font");
	}
	if (!IsValidUtf8(Text))
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::InvalidArgument, "Text is not valid UTF-8");
	}
	for (Toolbox::size_t Index = 0; Index < Text.Size(); ++Index)
	{
		if (Text[Index] == '\n' || Text[Index] == '\r')
		{
			return TResult<Toolbox::int32>::Failure(EErrorCode::InvalidArgument, "Measured text must be a single line");
		}
	}
	if (Text.IsEmpty())
	{
		return TResult<Toolbox::int32>::Success(0);
	}
	auto Width = m_pBackend->MeasureTextWidth(Font.GetNativeHandle_Internal(), Text);
	if (Width && Width.Value() < 0)
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "Negative text width");
	}
	return Width;
}
// 行の送り。
TResult<Toolbox::int32> FFontLoader::GetLineHeight(const FFont& Font) const
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Font.IsValid())
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::InvalidState, "Invalid font");
	}
	auto Height = m_pBackend->GetFontLineHeight(Font.GetNativeHandle_Internal());
	if (Height && Height.Value() < 0)
	{
		return TResult<Toolbox::int32>::Failure(EErrorCode::BackendFailure, "Negative line height");
	}
	return Height;
}
// 対象のリソースを読み込む。
// @param Options 処理に適用する設定。
TResult<FFont> FFontLoader::Load(const FFontOptions& Options)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FFont>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (Options.Size <= 0 || Options.Thickness <= 0 || !Detail::IsValidNativeString_Internal(Options.Family, true))
	{
		return TResult<FFont>::Failure(EErrorCode::InvalidArgument, "Invalid font dimensions or family name");
	}
	// リソースの読み込み結果。
	auto Loaded = m_pBackend->CreateFont(Options);
	if (!Loaded)
	{
		return TResult<FFont>::Failure(Loaded.Error());
	}
	// ネイティブハンドルの解放を保証する所有者。
	// @param Context 処理に必要な実行環境。
	// @param Value 処理対象の値。
	FNativeHandle Handle(Loaded.Value(), m_pBackend,
	                     [](void* Context, Toolbox::int32 Value) noexcept
	                     {
		                     static_cast<IFontBackend*>(Context)->DeleteFont(Value);
	                     });
	if (Handle.Get() < 0)
	{
		return TResult<FFont>::Failure(EErrorCode::BackendFailure, "Invalid font handle");
	}
	// 共有するリソース。
	auto Resource = Toolbox::MakeShared<FFontResource>(Toolbox::Move(Handle), Options);
	if (!m_pRegistry->Register(Resource))
	{
		return TResult<FFont>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	return TResult<FFont>::Success(FFont(Toolbox::Move(Resource)));
}
} // namespace Dxf
