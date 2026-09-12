#include "Dxf/FontLoader.h"
#include "Dxf/Utf8.h"

namespace Dxf
{
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
