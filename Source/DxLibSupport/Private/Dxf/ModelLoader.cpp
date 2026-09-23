// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ModelLoader.h"
#include "Dxf/Utf8.h"
#include "Toolbox/Log.h"
#include "Toolbox/Platform.h"
#include "Toolbox/Thread.h"
namespace Dxf
{
namespace
{
// 1つのモデルファイルとして受け付ける最大バイト数。
constexpr Toolbox::size_t MaxModelFileBytes = 512u * 1024u * 1024u;
} // namespace

// 必要な依存関係を受け取り、構築したスレッドを所有スレッドとして記録する。
// @param Backend ネイティブ処理。nullptrならモデルを扱えない。
// @param Registry 解放順序を管理する登録先。
FModelLoader::FModelLoader(IModelBackend* Backend, FResourceRegistry& Registry)
    : m_pBackend(Backend), m_pRegistry(&Registry), m_OwnerThread(Toolbox::FThread::CurrentThreadId())
{
}

// 保留中の解放を所有スレッドで済ませる。
FModelLoader::~FModelLoader()
{
	CollectDeferred();
}

// 所有スレッドから呼ばれているかを調べる。
bool FModelLoader::IsOwnerThread() const noexcept
{
	return Toolbox::FThread::CurrentThreadId() == m_OwnerThread;
}

// ネイティブモデルの解放を受け付ける。所有スレッド以外では保留する。
// @param Context 解放を受け付ける読み込み処理。
// @param Handle ハンドル。
void FModelLoader::Release_Internal(void* Context, Toolbox::int32 Handle) noexcept
{
	auto* Loader = static_cast<FModelLoader*>(Context);
	if (Loader->IsOwnerThread())
	{
		Loader->m_pBackend->DeleteModel(Handle);
		return;
	}
	Toolbox::FScopedLock Lock(Loader->m_DeferredMutex);
	try
	{
		Loader->m_Deferred.PushBack(Handle);
	}
	catch (...)
	{
		// 保留一覧を確保できなければ、ネイティブ側はセッション終了時にまとめて解放される。
		DXF_LOG_ERROR("Model", "Deferred model release could not be recorded (handle %d)", Handle);
	}
}

// 他のスレッドで保留した解放を行う。
void FModelLoader::CollectDeferred() noexcept
{
	if (!IsOwnerThread() || m_pBackend == nullptr)
	{
		return;
	}
	// 取り外した保留一覧。解放処理を排他の外で行う。
	Toolbox::TVector<Toolbox::int32> Pending;
	{
		Toolbox::FScopedLock Lock(m_DeferredMutex);
		Pending.Swap(m_Deferred);
	}
	for (const Toolbox::int32 Handle : Pending)
	{
		m_pBackend->DeleteModel(Handle);
	}
}

// 保留中の解放の数を取得する。
Toolbox::size_t FModelLoader::GetDeferredCount() const noexcept
{
	Toolbox::FScopedLock Lock(m_DeferredMutex);
	return m_Deferred.Size();
}

// .fbxを読み込む。
// @param Path 解決済みのファイルパス（UTF-8）。
// @param Options 読み込み設定。
TResult<FModel> FModelLoader::Load(const Toolbox::FString& Path, const FModelLoadOptions& Options)
{
	if (m_pBackend == nullptr)
	{
		return TResult<FModel>::Failure(EErrorCode::InvalidState,
		                                "Model loading is unavailable: the DxLib build cannot load models (run Setup.cmd to build DxLib from source)");
	}
	if (!IsOwnerThread())
	{
		return TResult<FModel>::Failure(EErrorCode::InvalidState, "Models must be loaded on the owner thread");
	}
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FModel>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Detail::IsValidNativeString_Internal(Path))
	{
		return TResult<FModel>::Failure(EErrorCode::InvalidArgument, "Model path must be nonempty UTF-8 without NUL");
	}
	CollectDeferred();
	// ファイル全体の内容。
	Toolbox::TVector<Toolbox::uint8> Bytes;
	try
	{
		if (!Toolbox::ReadFileBytes(Toolbox::FPath(Path), Bytes, MaxModelFileBytes))
		{
			return TResult<FModel>::Failure(EErrorCode::NotFound, "Model file not found: " + Path);
		}
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<FModel>::Failure(EErrorCode::BackendFailure, Toolbox::FString("Model file read failed: ") + Error.What());
	}
	// 座標系・単位の変換を含むFBXの変換結果。
	FModelImportOptions ImportOptions;
	ImportOptions.TargetUnitMeters = Options.TargetUnitMeters;
	ImportOptions.SamplesPerSecond = Options.SamplesPerSecond;
	auto Imported = ImportFbxModel(Bytes.Data(), Bytes.Size(), ImportOptions);
	if (!Imported)
	{
		return TResult<FModel>::Failure(Imported.Error().Code, Imported.Error().Message + " (" + Path + ")");
	}
	// テクスチャを探すディレクトリ。
	const Toolbox::FString Directory = Toolbox::FPath(Path).Parent().ToUtf8();
	auto Allocation = m_pBackend->LoadModel(Imported.Value(), Directory);
	if (!Allocation)
	{
		return TResult<FModel>::Failure(Allocation.Error());
	}
	// ネイティブハンドルの解放を保証する所有者。
	FNativeHandle Handle(Allocation.Value().NativeHandle, this, &FModelLoader::Release_Internal);
	if (Allocation.Value().NativeHandle < 0 || Allocation.Value().NativeClipDurations.Size() != Imported.Value().Clips.Size())
	{
		return TResult<FModel>::Failure(EErrorCode::BackendFailure, "Invalid model allocation");
	}
	FModelMetadata Metadata;
	Metadata.Path = Path;
	for (Toolbox::size_t Index = 0; Index < Imported.Value().Clips.Size(); ++Index)
	{
		const FImportedModelClip& Clip = Imported.Value().Clips[Index];
		Metadata.Clips.PushBack({Clip.Name, Clip.DurationSeconds, Allocation.Value().NativeClipDurations[Index]});
	}
	// 共有するリソース。
	auto Resource = Toolbox::MakeShared<FModelResource>(Toolbox::Move(Handle), Toolbox::Move(Metadata));
	if (!m_pRegistry->Register(Resource))
	{
		return TResult<FModel>::Failure(EErrorCode::InvalidState, "Resource registry stopped");
	}
	DXF_LOG_INFO("Model", "Loaded %s (clips=%u textures=%u)", Path.CStr(), static_cast<unsigned>(Imported.Value().Clips.Size()),
	             static_cast<unsigned>(Imported.Value().Textures.Size()));
	return TResult<FModel>::Success(FModel(Toolbox::Move(Resource)));
}

// モデルデータを共有するインスタンスを作る。
// @param Model 複製元のモデル。
TResult<FModelInstance> FModelLoader::CreateInstance(const FModel& Model)
{
	if (m_pBackend == nullptr)
	{
		return TResult<FModelInstance>::Failure(EErrorCode::InvalidState, "Model loading is unavailable");
	}
	if (!IsOwnerThread())
	{
		return TResult<FModelInstance>::Failure(EErrorCode::InvalidState, "Model instances must be created on the owner thread");
	}
	if (m_pRegistry->IsShutdown())
	{
		return TResult<FModelInstance>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Model.IsValid())
	{
		return TResult<FModelInstance>::Failure(EErrorCode::InvalidArgument, "Model is not valid");
	}
	// 別の読み込み処理で作ったモデルは、解放先が異なるため受け付けない。
	if (Model.GetResource_Internal()->GetBackendIdentity_Internal() != this)
	{
		return TResult<FModelInstance>::Failure(EErrorCode::InvalidArgument, "Model belongs to another asset service");
	}
	CollectDeferred();
	auto Duplicate = m_pBackend->DuplicateModel(Model.GetResource_Internal()->GetHandle_Internal());
	if (!Duplicate)
	{
		return TResult<FModelInstance>::Failure(Duplicate.Error());
	}
	// ネイティブハンドルの解放を保証する所有者。
	FNativeHandle Handle(Duplicate.Value(), this, &FModelLoader::Release_Internal);
	if (Duplicate.Value() < 0)
	{
		return TResult<FModelInstance>::Failure(EErrorCode::BackendFailure, "Invalid model instance allocation");
	}
	FModelInstanceMetadata Metadata;
	Metadata.Model = Model.GetResource_Internal();
	// 共有するリソース。
	auto Resource = Toolbox::MakeShared<FModelInstanceResource>(Toolbox::Move(Handle), Toolbox::Move(Metadata));
	if (!m_pRegistry->Register(Resource))
	{
		return TResult<FModelInstance>::Failure(EErrorCode::InvalidState, "Resource registry stopped");
	}
	return TResult<FModelInstance>::Success(FModelInstance(Toolbox::Move(Resource)));
}
} // namespace Dxf
