// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SaveStorage.h"
#include "Dxf/SaveError.h"
#include "Toolbox/FileError.h"
#include "Toolbox/FileLock.h"
#include "Toolbox/FileWriter.h"
#include "Toolbox/FilePublication.h"
#include "Toolbox/JsonWriter.h"
namespace Dxf
{
using namespace Toolbox;
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
	try
	{
		Writer.Write(Impl->Face);
		Writer.Flush();
		Writer.Close();
		// close後に実ファイルの全バイトが書込予定と一致することを検査する。
		TVector<uint8> ReadBack;
		if (!ReadFileBytes(Impl->Temporary, ReadBack, Limits.MaxBytes + 192) || FStringView(reinterpret_cast<const char*>(ReadBack.Data()), ReadBack.Size()) != FStringView(Impl->Face))
		{
			throw FSaveError(ESaveError::Corrupt, "Save temporary read-back mismatch");
		}
	}
	catch (...)
	{
		// ハンドルを閉じてから、自分が作ったtempだけを取り除く。
		try
		{
			Writer.Close();
		}
		catch (...)
		{
		}
		RemoveOwnedFile(Impl->Temporary);
		throw;
	}
	return FSaveTransaction(Move(Impl));
}
uint64 FSaveTransaction::Publish()
{
	if (!m_pImpl || m_pImpl->Canceled || m_pImpl->Committed || !m_pImpl->Lock)
	{
		throw FSaveError(ESaveError::TerminalRequest, "Save transaction is already terminal");
	}
	// 名前公開失敗時も、実際に新世代が存在するか必ず検査する。
	bool Failed = false;
	// 名前公開が失敗したOS原因を読戻し検査後も保持する。
	uint32 FailureCode = 0;
	m_pImpl->PublicationAttempted = true;
	try
	{
		PublishFile(m_pImpl->Temporary, m_pImpl->Destination);
	}
	catch (const FFileError& Error)
	{
		Failed = true;
		FailureCode = Error.GetCode();
	}
	// 公開対象を保持したまま、全ファイルの実バイトを照合する。
	TVector<uint8> ReadBack;
	try
	{
		if (!ReadFileBytes(m_pImpl->Destination, ReadBack, m_pImpl->Limits.MaxBytes + 192) || FStringView(reinterpret_cast<const char*>(ReadBack.Data()), ReadBack.Size()) != FStringView(m_pImpl->Face))
		{
			if (Failed)
			{
				throw FFileError(EFileOperation::Publish, FailureCode);
			}
			throw FSaveError(ESaveError::PublicationUnknown, "Published save could not be verified");
		}
	}
	catch (const FSaveError&)
	{
		throw;
	}
	catch (const FFileError& Error)
	{
		// 読戻し自体を完了できなければ、公開後の名前状態を断定しない。
		if (Error.GetOperation() != EFileOperation::Publish)
		{
			throw FSaveError(ESaveError::PublicationUnknown, "Publication read-back I/O failed; ownership retained");
		}
		throw;
	}
	catch (...)
	{
		throw FSaveError(ESaveError::PublicationUnknown, "Publication read-back failed; ownership retained");
	}
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
