#include "Dxf/SoundLoader.h"
#include "Dxf/Utf8.h"

namespace Dxf
{
// 対象のリソースを読み込む。
// @param Path 読み込むファイルのパス。
// @param Options 処理に適用する設定。
TResult<FSound> FSoundLoader::Load(const Toolbox::FString& Path, const FSoundLoadOptions& Options)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Detail::IsValidNativeString_Internal(Path))
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidArgument, "Path must be nonempty UTF-8 without NUL");
	}
	if (Options.Storage != ESoundStorage::Memory && Options.Storage != ESoundStorage::Stream)
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidArgument, "Invalid sound storage mode");
	}
	// リソースの読み込み結果。
	auto Loaded = m_pBackend->LoadSound(Path, Options);
	if (!Loaded)
	{
		return TResult<FSound>::Failure(Loaded.Error());
	}
	// ネイティブハンドルの解放を保証する所有者。
	// @param Context 処理に必要な実行環境。
	// @param Value 処理対象の値。
	FNativeHandle Handle(Loaded.Value(), m_pBackend,
	                     [](void* Context, Toolbox::int32 Value) noexcept
	                     {
		                     static_cast<ISoundBackend*>(Context)->DeleteSound(Value);
	                     });
	if (Handle.Get() < 0)
	{
		return TResult<FSound>::Failure(EErrorCode::BackendFailure, "Invalid sound handle");
	}
	// 共有するリソース。
	auto Resource = Toolbox::MakeShared<FSoundResource>(Toolbox::Move(Handle), FSoundMetadata{Path, Options});
	if (!m_pRegistry->Register(Resource))
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	return TResult<FSound>::Success(FSound(Toolbox::Move(Resource)));
}
// 準備済みデータからリソースを読み込む。ファイルを読まない。
// @param Data 音声ファイルのバイト列。呼び出し中だけ有効。
// @param Size バイト列の長さ。
// @param Options 処理に適用する設定。
// @param Path 登録に残すファイルのパス。
TResult<FSound> FSoundLoader::LoadMemory(const void* Data, Toolbox::size_t Size, const FSoundLoadOptions& Options,
                                         const Toolbox::FString& Path)
{
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (Data == nullptr || Size == 0)
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidArgument, "Sound memory is empty");
	}
	// リソースの読み込み結果。
	auto Loaded = m_pBackend->LoadSoundMemory(Data, Size, Options);
	if (!Loaded)
	{
		return TResult<FSound>::Failure(Loaded.Error());
	}
	// ネイティブハンドルの解放を保証する所有者。
	// @param Context 処理に必要な実行環境。
	// @param Value 処理対象の値。
	FNativeHandle Handle(Loaded.Value(), m_pBackend,
	                     [](void* Context, Toolbox::int32 Value) noexcept
	                     {
		                     static_cast<ISoundBackend*>(Context)->DeleteSound(Value);
	                     });
	if (Handle.Get() < 0)
	{
		return TResult<FSound>::Failure(EErrorCode::BackendFailure, "Invalid sound handle");
	}
	// 共有するリソース。
	auto Resource = Toolbox::MakeShared<FSoundResource>(Toolbox::Move(Handle), FSoundMetadata{Path, Options});
	if (!m_pRegistry->Register(Resource))
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	return TResult<FSound>::Success(FSound(Toolbox::Move(Resource)));
}
} // namespace Dxf
