// SPDX-License-Identifier: NOASSERTION
#include "Dxf/AssetService.h"
#include "Dxf/AssetDecode.h"
#include "Dxf/TaskDispatcher.h"
#include "Dxf/Utf8.h"
#include "Toolbox/Platform.h"
// FAssetServiceの非同期要求は借用Dispatcherへ出す。DxLibSupportからRuntimeへの
// リンク依存は作らず、最終リンクで解決する。呼び出し側はDispatcherの停止を
// サービスの終了より先に行い、借用Dispatcherを要求より長く生存させること。
namespace Dxf
{
struct FAsyncTexture::FState
{
	// 進行状態を守るMutex。
	Toolbox::FMutex Mutex;
	// 結果が確定しているか。
	bool bReady = false;
	// 確定した結果。
	Toolbox::TOptional<TResult<FTexture>> Result;
	// Workerが準備したファイルバイト列。
	Toolbox::TVector<Toolbox::uint8> Bytes;
	// 失敗を確定する。
	void Fail(EErrorCode Code, Toolbox::FString Message)
	{
		Toolbox::FScopedLock Lock(Mutex);
		Result = TResult<FTexture>::Failure(Code, Toolbox::Move(Message));
		bReady = true;
	}
	// 準備したバイト列を保持する。
	void Store(Toolbox::TVector<Toolbox::uint8>&& Data)
	{
		Toolbox::FScopedLock Lock(Mutex);
		Bytes = Toolbox::Move(Data);
	}
	// 準備したバイト列を持ち出す。
	Toolbox::TVector<Toolbox::uint8> TakeBytes()
	{
		Toolbox::FScopedLock Lock(Mutex);
		return Toolbox::Move(Bytes);
	}
	// 成功を確定する。
	void Succeed(TResult<FTexture>&& Value)
	{
		Toolbox::FScopedLock Lock(Mutex);
		Result = Toolbox::Move(Value);
		bReady = true;
	}
};
struct FAsyncSound::FState
{
	// 進行状態を守るMutex。
	Toolbox::FMutex Mutex;
	// 結果が確定しているか。
	bool bReady = false;
	// 確定した結果。
	Toolbox::TOptional<TResult<FSound>> Result;
	// Workerが準備したファイルバイト列。
	Toolbox::TVector<Toolbox::uint8> Bytes;
	// 失敗を確定する。
	void Fail(EErrorCode Code, Toolbox::FString Message)
	{
		Toolbox::FScopedLock Lock(Mutex);
		Result = TResult<FSound>::Failure(Code, Toolbox::Move(Message));
		bReady = true;
	}
	// 準備したバイト列を保持する。
	void Store(Toolbox::TVector<Toolbox::uint8>&& Data)
	{
		Toolbox::FScopedLock Lock(Mutex);
		Bytes = Toolbox::Move(Data);
	}
	// 準備したバイト列を持ち出す。
	Toolbox::TVector<Toolbox::uint8> TakeBytes()
	{
		Toolbox::FScopedLock Lock(Mutex);
		return Toolbox::Move(Bytes);
	}
	// 成功を確定する。
	void Succeed(TResult<FSound>&& Value)
	{
		Toolbox::FScopedLock Lock(Mutex);
		Result = Toolbox::Move(Value);
		bReady = true;
	}
};
FAsyncTexture::~FAsyncTexture() = default;
bool FAsyncTexture::IsReady() noexcept
{
	if (!m_State)
	{
		return false;
	}
	Toolbox::FScopedLock Lock(m_State->Mutex);
	return m_State->bReady;
}
TResult<FTexture> FAsyncTexture::Take()
{
	if (!m_State)
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidState, "Empty async ticket");
	}
	Toolbox::FScopedLock Lock(m_State->Mutex);
	if (!m_State->bReady || !m_State->Result.HasValue())
	{
		return TResult<FTexture>::Failure(EErrorCode::InvalidState, "Async result is not ready");
	}
	return m_State->Result.Value();
}
FAsyncSound::~FAsyncSound() = default;
bool FAsyncSound::IsReady() noexcept
{
	if (!m_State)
	{
		return false;
	}
	Toolbox::FScopedLock Lock(m_State->Mutex);
	return m_State->bReady;
}
TResult<FSound> FAsyncSound::Take()
{
	if (!m_State)
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Empty async ticket");
	}
	Toolbox::FScopedLock Lock(m_State->Mutex);
	if (!m_State->bReady || !m_State->Result.HasValue())
	{
		return TResult<FSound>::Failure(EErrorCode::InvalidState, "Async result is not ready");
	}
	return m_State->Result.Value();
}
// 画像の読み込みをWorkerの準備と所有側の取込に分けて要求する。
// @param Dispatcher 準備と反映の実行先。
// @param Scope 要求の所属。取り消し時は反映せず期限切れになる。
// @param Path 読み込むファイルのパス。
// @param Options 処理に適用する設定。
TResult<FAsyncTexture> FAssetService::LoadTextureAsync(FTaskDispatcher& Dispatcher, const FTaskScope& Scope,
                                                       const Toolbox::FString& Path,
                                                       const FTextureLoadOptions& Options)
{
	if (m_Registry.IsShutdown())
	{
		return TResult<FAsyncTexture>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Detail::IsValidNativeString_Internal(Path))
	{
		return TResult<FAsyncTexture>::Failure(EErrorCode::InvalidArgument,
		                                       "Texture path must be nonempty UTF-8 without NUL");
	}
	// 解決した読み込みパス。
	Toolbox::FString Resolved;
	if (!ResolveRequestPath_Internal(Path, Resolved))
	{
		return TResult<FAsyncTexture>::Failure(
		    EErrorCode::InvalidArgument,
		    Toolbox::FString("Texture path cannot resolve against root ") + m_Resolver.GetRoot().ToUtf8() + ": " + Path);
	}
	// 検索または入力のキー。
	const Toolbox::FString Key = Resolved + (Options.bUse3D ? "|3d" : "|2d");
	// 再利用可能なキャッシュを取得して有効性を確認する。
	if (auto Cached = m_TextureCache.Find(Key))
	{
		FAsyncTexture Ticket;
		Ticket.m_State = Toolbox::MakeShared<FAsyncTexture::FState>();
		Ticket.m_State->Succeed(
		    TResult<FTexture>::Success(FTexture(Toolbox::Move(Cached))));
		return TResult<FAsyncTexture>::Success(Toolbox::Move(Ticket));
	}
	// 共有する要求の進行状態。
	Toolbox::TSharedPtr<FAsyncTexture::FState> State = Toolbox::MakeShared<FAsyncTexture::FState>();
	FTaskRequest Request;
	Request.Scope = Dispatcher.GetRootScope();
	Request.Prepare = Toolbox::TFunction<ETaskPrepare()>([State, &Dispatcher, Scope, Resolved]()
	{
		if (Scope.IsValid() && Dispatcher.IsCanceled(Scope))
		{
			State->Fail(EErrorCode::InvalidState, "Async scope expired");
			return ETaskPrepare::Canceled;
		}
		Toolbox::TVector<Toolbox::uint8> Bytes;
		try
		{
			if (!Toolbox::ReadFileBytes(Toolbox::FPath(Resolved), Bytes, MaxAssetFileBytes))
			{
				State->Fail(EErrorCode::NotFound, Toolbox::FString("Async file not found: ") + Resolved);
				return ETaskPrepare::Failed;
			}
		}
		catch (const Toolbox::FException& Error)
		{
			State->Fail(EErrorCode::BackendFailure, Error.What());
			return ETaskPrepare::Failed;
		}
		catch (...)
		{
			State->Fail(EErrorCode::BackendFailure, "Async file read failed");
			return ETaskPrepare::Failed;
		}
		auto Decoded = DecodeAssetBytes(Bytes.Data(), Bytes.Size());
		if (!Decoded || Decoded.Value().Kind != EAssetDecodeKind::Bitmap)
		{
			State->Fail(EErrorCode::InvalidArgument,
			            !Decoded ? Decoded.Error().Message : Toolbox::FString("Not a bitmap file"));
			return ETaskPrepare::Failed;
		}
		State->Store(Toolbox::Move(Bytes));
		return ETaskPrepare::Success;
	});
	FTextureLoadOptions KeepOptions = Options;
	FTaskScope Watched = Scope;
	FAssetService* Self = this;
	Request.Commit = Toolbox::TFunction<bool()>([State, Self, Key, KeepOptions, Watched, &Dispatcher]()
	{
		if (Watched.IsValid() &&
		    (!Dispatcher.IsScopeAlive(Watched) || Dispatcher.IsCanceled(Watched)))
		{
			State->Fail(EErrorCode::InvalidState, "Async scope expired");
			return true;
		}
		Toolbox::TVector<Toolbox::uint8> Bytes = State->TakeBytes();
		auto Loaded = Self->m_TextureLoader.LoadMemory(Bytes.Data(), Bytes.Size(), KeepOptions);
		if (!Loaded)
		{
			State->Fail(Loaded.Error().Code, Loaded.Error().Message);
			return false;
		}
		FTexture Cached = Loaded.Value();
		Self->m_TextureCache.Insert(Key, Cached.GetResource_Internal());
		State->Succeed(Toolbox::Move(Loaded));
		return true;
	});
	if (!Dispatcher.Submit(Toolbox::Move(Request)))
	{
		return TResult<FAsyncTexture>::Failure(EErrorCode::InvalidState, "Task dispatcher is busy");
	}
	FAsyncTexture Ticket;
	Ticket.m_State = Toolbox::Move(State);
	return TResult<FAsyncTexture>::Success(Toolbox::Move(Ticket));
}
// 音声の読み込みをWorkerの準備と所有側の取込に分けて要求する。
// @param Dispatcher 準備と反映の実行先。
// @param Scope 要求の所属。取り消し時は反映せず期限切れになる。
// @param Path 読み込むファイルのパス。
// @param Options 処理に適用する設定。
TResult<FAsyncSound> FAssetService::LoadSoundAsync(FTaskDispatcher& Dispatcher, const FTaskScope& Scope,
                                                   const Toolbox::FString& Path, const FSoundLoadOptions& Options)
{
	if (m_Registry.IsShutdown())
	{
		return TResult<FAsyncSound>::Failure(EErrorCode::InvalidState, "Assets stopped");
	}
	if (!Detail::IsValidNativeString_Internal(Path) ||
	    (Options.Storage != ESoundStorage::Memory && Options.Storage != ESoundStorage::Stream))
	{
		return TResult<FAsyncSound>::Failure(EErrorCode::InvalidArgument,
		                                      "Sound requires a valid storage mode and nonempty UTF-8 path without NUL");
	}
	if (Options.Storage != ESoundStorage::Memory)
	{
		return TResult<FAsyncSound>::Failure(EErrorCode::InvalidArgument,
		                                      "Async loading supports Memory storage only");
	}
	// 解決した読み込みパス。
	Toolbox::FString Resolved;
	if (!ResolveRequestPath_Internal(Path, Resolved))
	{
		return TResult<FAsyncSound>::Failure(
		    EErrorCode::InvalidArgument,
		    Toolbox::FString("Sound path cannot resolve against root ") + m_Resolver.GetRoot().ToUtf8() + ": " + Path);
	}
	// 検索または入力のキー。
	const Toolbox::FString Key = Resolved + "|memory";
	// 再利用可能なキャッシュを取得して有効性を確認する。
	if (auto Cached = m_SoundCache.Find(Key))
	{
		FAsyncSound Ticket;
		Ticket.m_State = Toolbox::MakeShared<FAsyncSound::FState>();
		Ticket.m_State->Succeed(TResult<FSound>::Success(FSound(Toolbox::Move(Cached))));
		return TResult<FAsyncSound>::Success(Toolbox::Move(Ticket));
	}
	// 共有する要求の進行状態。
	Toolbox::TSharedPtr<FAsyncSound::FState> State = Toolbox::MakeShared<FAsyncSound::FState>();
	FTaskRequest Request;
	Request.Scope = Dispatcher.GetRootScope();
	Request.Prepare = Toolbox::TFunction<ETaskPrepare()>([State, &Dispatcher, Scope, Resolved]()
	{
		if (Scope.IsValid() && Dispatcher.IsCanceled(Scope))
		{
			State->Fail(EErrorCode::InvalidState, "Async scope expired");
			return ETaskPrepare::Canceled;
		}
		Toolbox::TVector<Toolbox::uint8> Bytes;
		try
		{
			if (!Toolbox::ReadFileBytes(Toolbox::FPath(Resolved), Bytes, MaxAssetFileBytes))
			{
				State->Fail(EErrorCode::NotFound, Toolbox::FString("Async file not found: ") + Resolved);
				return ETaskPrepare::Failed;
			}
		}
		catch (const Toolbox::FException& Error)
		{
			State->Fail(EErrorCode::BackendFailure, Error.What());
			return ETaskPrepare::Failed;
		}
		catch (...)
		{
			State->Fail(EErrorCode::BackendFailure, "Async file read failed");
			return ETaskPrepare::Failed;
		}
		auto Decoded = DecodeAssetBytes(Bytes.Data(), Bytes.Size());
		if (!Decoded || Decoded.Value().Kind != EAssetDecodeKind::Wave)
		{
			State->Fail(EErrorCode::InvalidArgument,
			            !Decoded ? Decoded.Error().Message : Toolbox::FString("Not a wave file"));
			return ETaskPrepare::Failed;
		}
		State->Store(Toolbox::Move(Bytes));
		return ETaskPrepare::Success;
	});
	FSoundLoadOptions KeepOptions = Options;
	FTaskScope Watched = Scope;
	FAssetService* Self = this;
	Request.Commit = Toolbox::TFunction<bool()>([State, Self, Key, Resolved, KeepOptions, Watched, &Dispatcher]()
	{
		if (Watched.IsValid() &&
		    (!Dispatcher.IsScopeAlive(Watched) || Dispatcher.IsCanceled(Watched)))
		{
			State->Fail(EErrorCode::InvalidState, "Async scope expired");
			return true;
		}
		Toolbox::TVector<Toolbox::uint8> Bytes = State->TakeBytes();
		auto Loaded = Self->m_SoundLoader.LoadMemory(Bytes.Data(), Bytes.Size(), KeepOptions, Resolved);
		if (!Loaded)
		{
			State->Fail(Loaded.Error().Code, Loaded.Error().Message);
			return false;
		}
		FSound Cached = Loaded.Value();
		Self->m_SoundCache.Insert(Key, Cached.GetResource_Internal());
		State->Succeed(Toolbox::Move(Loaded));
		return true;
	});
	if (!Dispatcher.Submit(Toolbox::Move(Request)))
	{
		return TResult<FAsyncSound>::Failure(EErrorCode::InvalidState, "Task dispatcher is busy");
	}
	FAsyncSound Ticket;
	Ticket.m_State = Toolbox::Move(State);
	return TResult<FAsyncSound>::Success(Toolbox::Move(Ticket));
}
} // namespace Dxf
