// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SaveStorage.h"
#include "Dxf/SaveTemporary.h"
#include "Dxf/SavePublication.h"
#include "Dxf/SaveError.h"
#include "Toolbox/FileError.h"
#include "Toolbox/FileLock.h"
#include "Toolbox/FileWriter.h"
#include "Toolbox/FilePublication.h"
#include "Toolbox/JsonWriter.h"
namespace Dxf
{
using namespace Toolbox;
namespace
{
// 新規一時ファイルのOS操作だけを借用する。追加の所有器や確保を作らない。
struct FTemporaryIo
{
	// 呼出し側が新規作成を確認した書込ハンドル。
	FFileWriter& Writer;
	// このトランザクションだけの一時パス。
	const FPath& Path;
	// 確保前の読戻し上限。
	size_t MaxBytes;
	void Write(FStringView Bytes)
	{
		Writer.Write(Bytes);
	}
	void Flush()
	{
		Writer.Flush();
	}
	void Close()
	{
		Writer.Close();
	}
	bool Matches(FStringView Bytes)
	{
		TVector<uint8> ReadBack;
		return ReadFileBytes(Path, ReadBack, MaxBytes) && FStringView(reinterpret_cast<const char*>(ReadBack.Data()), ReadBack.Size()) == Bytes;
	}
	void Remove() noexcept
	{
		RemoveOwnedFile(Path);
	}
};
// 公開中は最新正常面と異なる公開先だけを操作し、実際の全バイトを照合する。
struct FPublicationIo
{
	// 自分が新規作成した一時ファイル。
	const FPath& Temporary;
	// 最新正常面とは別の更新面。
	const FPath& Destination;
	// 読戻しの有限上限。
	size_t MaxBytes;
	// この要求が実際に書いた予定バイト。
	FStringView Face;
	void Publish()
	{
		PublishFile(Temporary, Destination);
	}
	bool Matches()
	{
		TVector<uint8> ReadBack;
		return ReadFileBytes(Destination, ReadBack, MaxBytes) && FStringView(reinterpret_cast<const char*>(ReadBack.Data()), ReadBack.Size()) == Face;
	}
};
} // namespace
// トランザクション中は他のWriterにslotを渡さない。
struct FSaveTransaction::FImpl
{
	// OSが解放するslot排他。
	TUniquePtr<FFileLock> Lock;
	// 不変の保存領域。
	FPath Root;
	// 検証済み系列名。
	FString Slot;
	// 自分がCREATE_NEWで作る一時パス。
	FPath Temporary;
	// 最新正常面と反対の公開先。
	FPath Destination;
	// 世代を割り当てた論理値。
	FSaveDocument Document;
	// 公開前後で再検証する全バイト。
	FString Face;
	// 同じ有限上限でread-backする。
	FSaveLimits Limits;
	// 成功確認済みの公開結果。
	bool Committed = false;
	// 公開前の取消し。
	bool Canceled = false;
	// 名前公開を試みた後は取消し成功と偽らない。
	bool PublicationAttempted = false;
};
FSaveTransaction::FSaveTransaction(TUniquePtr<FImpl> Impl) : m_pImpl(Move(Impl))
{
}
FSaveTransaction::~FSaveTransaction() noexcept = default;
FSaveTransaction::FSaveTransaction(FSaveTransaction&& Other) noexcept = default;
FSaveTransaction& FSaveTransaction::operator=(FSaveTransaction&& Other) noexcept = default;
FSaveTransaction FSaveTransaction::Prepare(const FPath& Root, FString Slot, FSaveDocument Document, FSaveLimits Limits)
{
	ValidateSaveLimits(Limits);
	ValidateSaveLocation(Root, Slot);
	// 全確保は公開前に行い、失敗で元の確定面を触らない。
	TUniquePtr<FImpl> Impl = MakeUnique<FImpl>();
	Impl->Root = Root;
	Impl->Slot = CanonicalSaveSlot(Slot);
	Impl->Limits = Limits;
	Impl->Lock = MakeUnique<FFileLock>(Root / FPath(Impl->Slot + ".lock"));
	// 排他取得後に最新世代と更新先を一度決める。
	const FSaveLoadResult Previous = ReadSaveFaces(Root, Impl->Slot, Limits, &Impl->Destination);
	// 空系列からは世代1を始める。
	const uint64 Generation = Previous.Document ? Previous.Document->Generation : 0;
	if (Generation == TNumericLimits<uint64>::Max())
	{
		throw FSaveError(ESaveError::GenerationOverflow, "Save generation overflow");
	}
	if (Previous.Document && Previous.Document->Kind != Document.Kind)
	{
		throw FSaveError(ESaveError::IncompatibleContent, "Progress and settings require different slots");
	}
	Document.Generation = Generation + 1;
	Impl->Document = Move(Document);
	Impl->Face = EncodeSaveFace(Impl->Document, Limits);
	Impl->Temporary = Root / FPath(Impl->Slot + ".tmp-" + FJsonWriter::Unsigned(MonotonicNanoseconds()));
	// 既存のtempも上書きしない。自分の作成が成功した場合だけ後始末できる。
	FFileWriter Writer(Impl->Temporary);
	FTemporaryIo Io{Writer, Impl->Temporary, Limits.MaxBytes + 192};
	SavePrivate::WriteTemporary(Io, Impl->Face);
	return FSaveTransaction(Move(Impl));
}
uint64 FSaveTransaction::Publish()
{
	if (!m_pImpl || m_pImpl->Canceled || m_pImpl->Committed || !m_pImpl->Lock)
	{
		throw FSaveError(ESaveError::TerminalRequest, "Save transaction is already terminal");
	}
	m_pImpl->PublicationAttempted = true;
	FPublicationIo Io{m_pImpl->Temporary, m_pImpl->Destination, m_pImpl->Limits.MaxBytes + 192, m_pImpl->Face};
	SavePrivate::VerifyPublication(Io);
	m_pImpl->Committed = true;
	m_pImpl->Lock.Reset();
	return m_pImpl->Document.Generation;
}
bool FSaveTransaction::Cancel()
{
	if (!m_pImpl || m_pImpl->Committed || m_pImpl->PublicationAttempted)
	{
		return false;
	}
	if (!m_pImpl->Canceled)
	{
		// 公開していない自分の一時ファイルだけを後始末する。
		RemoveOwnedFile(m_pImpl->Temporary);
		m_pImpl->Canceled = true;
		m_pImpl->Lock.Reset();
	}
	return true;
}
bool FSaveTransaction::IsCommitted() const noexcept
{
	return m_pImpl && m_pImpl->Committed;
}
} // namespace Dxf
