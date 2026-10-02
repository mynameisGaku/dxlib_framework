// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/SaveStore.h"
#include "Toolbox/FileWriter.h"
#include "Toolbox/FilePublication.h"
#include "Toolbox/JsonWriter.h"
#include "../Source/SaveData/Private/Dxf/SaveTemporary.h"
#include "../Source/SaveData/Private/Dxf/SavePublication.h"
#include "../Source/SaveData/Private/Dxf/SaveStorage.h"
#include "../Source/Toolbox/Private/Toolbox/FileWriteLoop.h"
#include "../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 本体と同じ一時ファイル順序へ、一地点だけの局所故障を渡す。
enum class ETemporaryFailure
{
	// 注入せず全量確認する対照。
	None,
	// 一バイト残して書込が失敗する。
	Write,
	// 全量書込後の完了要求が失敗する。
	Flush,
	// ハンドル解放後の結果確認が失敗する。
	Close,
	// close後の読込操作が失敗する。
	Read,
	// 読戻しでファイルが見つからない。
	Missing,
	// 読戻しの最後の一バイトを欠く。
	ShortRead,
	// 読戻しの一バイトが変化している。
	ChangedRead
};
// 一時ファイルの所有と実OS操作を保ち、指定した工程だけを故障化する。
class FFailingTemporaryIo
{
public:
	FFailingTemporaryIo(FPath Path, ETemporaryFailure Failure) : m_Path(Move(Path)), m_Writer(m_Path), m_Failure(Failure)
	{
	}
	void Write(FStringView Bytes)
	{
		m_Trace += "W";
		if (m_Failure == ETemporaryFailure::Write)
		{
			// 全量成功に見えない、実際に一バイトだけ残ったwrite。
			m_Writer.Write(FStringView(Bytes.Data(), 1));
			throw FFileError(EFileOperation::Write, 101);
		}
		m_Writer.Write(Bytes);
	}
	void Flush()
	{
		m_Trace += "F";
		if (m_Failure == ETemporaryFailure::Flush)
		{
			throw FFileError(EFileOperation::Flush, 102);
		}
		m_Writer.Flush();
	}
	void Close()
	{
		m_Trace += "C";
		m_Writer.Close();
		if (m_Failure == ETemporaryFailure::Close)
		{
			throw FFileError(EFileOperation::Close, 103);
		}
	}
	bool Matches(FStringView Bytes)
	{
		m_Trace += "R";
		if (m_Failure == ETemporaryFailure::Read)
		{
			throw FFileError(EFileOperation::Read, 104);
		}
		if (m_Failure == ETemporaryFailure::Missing)
		{
			return false;
		}
		TVector<uint8> Actual;
		REQUIRE(ReadFileBytes(m_Path, Actual, 1024));
		if (m_Failure == ETemporaryFailure::ShortRead)
		{
			Actual.PopBack();
		}
		if (m_Failure == ETemporaryFailure::ChangedRead)
		{
			Actual[0] ^= 1;
		}
		return FStringView(reinterpret_cast<const char*>(Actual.Data()), Actual.Size()) == Bytes;
	}
	void Remove() noexcept
	{
		++m_Removals;
		RemoveOwnedFile(m_Path);
	}
	const FString& Trace() const noexcept
	{
		return m_Trace;
	}
	uint32 Removals() const noexcept
	{
		return m_Removals;
	}

private:
	// 新規Root内でこの試行だけが作成した一時ファイル。
	FPath m_Path;
	// CREATE_NEWで開いた実ファイルの所有者。
	FFileWriter m_Writer;
	// 今回だけ注入する工程。
	ETemporaryFailure m_Failure;
	// 実際に呼ばれた操作順序。
	FString m_Trace;
	// 所有tempの後始末回数。
	uint32 m_Removals = 0;
};
FSaveDocument Document()
{
	FSaveDocument Value;
	Value.GameId = "io_fault";
	Value.ContentRevision = "bundle1";
	Value.SceneKey = "scene";
	Value.CheckpointKey = "safe";
	Value.Payload = "{\"score\":7}";
	return Value;
}
// 公開前の失敗、公開済みだがAPI失敗、公開後の読戻し失敗を別々に注入する。
enum class EPublicationFailure
{
	// 通常公開と実バイト確認。
	None,
	// 名前変更より前にOSが失敗を返す。
	BeforeRename,
	// 名前変更は済んだがOSが失敗を返す。
	AfterRename,
	// 公開後の読み取り操作が失敗する。
	Read,
	// 公開後の読戻し配列の確保が失敗する。
	ReadAllocation,
	// 公開後の読戻し値が予定値と異なる。
	Mismatch,
	// 公開後の読み取りが対象を見つけられない。
	Missing
};
class FFailingPublicationIo
{
public:
	FFailingPublicationIo(FPath Temporary, FPath Destination, FString Bytes, EPublicationFailure Failure) : m_Temporary(Move(Temporary)), m_Destination(Move(Destination)), m_Bytes(Move(Bytes)), m_Failure(Failure)
	{
		FFileWriter Writer(m_Temporary);
		Writer.Write(m_Bytes);
		Writer.Flush();
		Writer.Close();
	}
	void Publish()
	{
		++m_Publishes;
		if (m_Failure == EPublicationFailure::BeforeRename)
		{
			throw FFileError(EFileOperation::Publish, 201);
		}
		PublishFile(m_Temporary, m_Destination);
		if (m_Failure == EPublicationFailure::AfterRename)
		{
			throw FFileError(EFileOperation::Publish, 202);
		}
	}
	bool Matches()
	{
		++m_Reads;
		if (m_Failure == EPublicationFailure::Read)
		{
			throw FFileError(EFileOperation::Read, 203);
		}
		if (m_Failure == EPublicationFailure::ReadAllocation)
		{
			Testing::SetAllocationFailureCountdown(0);
			try
			{
				TVector<uint8> Storage;
				Storage.Resize(16);
			}
			catch (...)
			{
				m_bInjected = Testing::WasAllocationFailureInjected();
				Testing::SetAllocationFailureCountdown(-1);
				throw;
			}
			Testing::SetAllocationFailureCountdown(-1);
			REQUIRE(false);
		}
		if (m_Failure == EPublicationFailure::Mismatch || m_Failure == EPublicationFailure::Missing)
		{
			return false;
		}
		TVector<uint8> Actual;
		return ReadFileBytes(m_Destination, Actual, 1024) && FStringView(reinterpret_cast<const char*>(Actual.Data()), Actual.Size()) == m_Bytes;
	}
	void AllowRetry() noexcept
	{
		m_Failure = EPublicationFailure::None;
	}
	uint32 Reads() const noexcept
	{
		return m_Reads;
	}
	uint32 Publishes() const noexcept
	{
		return m_Publishes;
	}
	bool WasInjected() const noexcept
	{
		return m_bInjected;
	}

private:
	// 今回新規作成した一時ファイル。
	FPath m_Temporary;
	// 最新正常面とは別の公開先。
	FPath m_Destination;
	// 実際に書いた面の全バイト。
	FString m_Bytes;
	// 一回目に注入する故障。
	EPublicationFailure m_Failure;
	// 公開操作の試行回数。
	uint32 m_Publishes = 0;
	// 戻り値によらず行った読戻し回数。
	uint32 m_Reads = 0;
	// 実際に確保失敗が注入されたか。
	bool m_bInjected = false;
};
} // namespace
TEST("Save I/O failure at every unpublished stage protects latest face bytes")
{
	const FPath Root = FPath(DXF_SAVE_TEST_ROOT) / FPath(FString("io-stage-") + FJsonWriter::Unsigned(MonotonicNanoseconds()));
	FSaveStore Store(Root);
	REQUIRE(Store.Save("manual", Document()) == 1);
	TVector<uint8> Original;
	REQUIRE(ReadFileBytes(Root / "manual.a", Original, 1024));
	const ETemporaryFailure Failures[] = {ETemporaryFailure::Write, ETemporaryFailure::Flush, ETemporaryFailure::Close, ETemporaryFailure::Read, ETemporaryFailure::Missing, ETemporaryFailure::ShortRead, ETemporaryFailure::ChangedRead};
	const char* Traces[] = {"WC", "WFC", "WFCC", "WFCRC", "WFCRC", "WFCRC", "WFCRC"};
	for (uint32 I = 0; I < 7; ++I)
	{
		const FPath Temporary = Root / FPath(FString("owned.tmp-") + ToString(I));
		FFailingTemporaryIo Io(Temporary, Failures[I]);
		bool Failed = false;
		try
		{
			SavePrivate::WriteTemporary(Io, "new payload bytes");
		}
		catch (const FFileError& Error)
		{
			const EFileOperation Operations[] = {EFileOperation::Write, EFileOperation::Flush, EFileOperation::Close, EFileOperation::Read};
			REQUIRE(I < 4 && Error.GetOperation() == Operations[I]);
			REQUIRE(Error.GetCode() == 101 + I);
			Failed = true;
		}
		catch (const FSaveError& Error)
		{
			REQUIRE(I >= 4 && Error.GetCode() == ESaveError::Corrupt);
			Failed = true;
		}
		REQUIRE(Failed && Io.Trace() == Traces[I] && Io.Removals() == 1);
		TVector<uint8> After;
		REQUIRE(!ReadFileBytes(Temporary, After, 1024));
		REQUIRE(ReadFileBytes(Root / "manual.a", After, 1024));
		REQUIRE(After.Size() == Original.Size());
		for (size_t Byte = 0; Byte < Original.Size(); ++Byte)
		{
			REQUIRE(After[Byte] == Original[Byte]);
		}
		REQUIRE(Store.Load("manual").Document->Generation == 1);
		Out << "SAVE_IO_FAULT stage=" << I << " original_generation=1 original_bytes_unchanged=1 temp_removed=1\n";
	}
	REQUIRE(Store.Save("manual", Document()) == 2);
	REQUIRE(Store.Load("manual").Document->Generation == 2);
}
TEST("Save I/O completes close and read-back before any publication")
{
	const FPath Root = FPath(DXF_SAVE_TEST_ROOT) / FPath(FString("io-green-") + FJsonWriter::Unsigned(MonotonicNanoseconds()));
	FSaveStore Store(Root);
	const FPath Temporary = Root / "only-owned.tmp";
	FFailingTemporaryIo Io(Temporary, ETemporaryFailure::None);
	SavePrivate::WriteTemporary(Io, "verified bytes");
	REQUIRE(Io.Trace() == "WFCR" && Io.Removals() == 0);
	TVector<uint8> Actual;
	REQUIRE(ReadFileBytes(Temporary, Actual, 1024));
	REQUIRE(FStringView(reinterpret_cast<const char*>(Actual.Data()), Actual.Size()) == "verified bytes");
	REQUIRE(Store.Load("manual").State == ESaveLoadState::Empty);
	REQUIRE(RemoveOwnedFile(Temporary));
}
TEST("Save file create failure never adopts or removes a pre-existing temporary")
{
	const FPath Root = FPath(DXF_SAVE_TEST_ROOT) / FPath(FString("io-create-") + FJsonWriter::Unsigned(MonotonicNanoseconds()));
	FSaveStore Store(Root);
	REQUIRE(Store.Save("manual", Document()) == 1);
	const FPath Temporary = Root / "pre-existing.tmp";
	{
		FFileWriter Writer(Temporary);
		Writer.Write("previous temporary");
		Writer.Close();
	}
	bool Failed = false;
	try
	{
		FFailingTemporaryIo Io(Temporary, ETemporaryFailure::None);
	}
	catch (const FFileError& Error)
	{
		REQUIRE(Error.GetOperation() == EFileOperation::Create);
		Failed = true;
	}
	TVector<uint8> Actual;
	REQUIRE(Failed && ReadFileBytes(Temporary, Actual, 1024));
	REQUIRE(FStringView(reinterpret_cast<const char*>(Actual.Data()), Actual.Size()) == "previous temporary");
	REQUIRE(Store.Load("manual").Document->Generation == 1);
	REQUIRE(Store.Save("manual", Document()) == 2);
}
TEST("Save short writes advance by actual bytes and reject zero oversized or failed writes")
{
	const FString Input = "0123456789ABCDEF";
	FString Actual;
	uint32 Calls = 0;
	const auto ShortWrite = [&](const char* Data, uint32 Count)
	{
		REQUIRE(Count == Input.Size() - Actual.Size());
		const uint32 Written = Min(Count, uint32(3));
		Actual += FString(Data, Written);
		++Calls;
		return Written;
	};
	FilePrivate::WriteAll(Input, ShortWrite);
	REQUIRE(Actual == Input && Calls == 6);
	for (uint32 Failure = 0; Failure < 3; ++Failure)
	{
		bool Failed = false;
		Calls = 0;
		try
		{
			const auto FailedWrite = [&](const char*, uint32 Count) -> uint32
			{
				++Calls;
				if (Failure == 2)
				{
					throw FFileError(EFileOperation::Write, 107);
				}
				return Failure == 0 ? 0 : Count + 1;
			};
			FilePrivate::WriteAll(Input, FailedWrite);
		}
		catch (const FFileError& Error)
		{
			REQUIRE(Error.GetOperation() == EFileOperation::Write && Error.GetCode() == (Failure == 2 ? uint32(107) : uint32(0)));
			Failed = true;
		}
		REQUIRE(Failed && Calls == 1);
	}
}
TEST("Save publication resolves ambiguous names by bytes and preserves latest normal face")
{
	const EPublicationFailure Failures[] = {EPublicationFailure::None, EPublicationFailure::BeforeRename, EPublicationFailure::AfterRename, EPublicationFailure::Read, EPublicationFailure::ReadAllocation, EPublicationFailure::Mismatch, EPublicationFailure::Missing};
	for (uint32 I = 0; I < 7; ++I)
	{
		const FPath Root = FPath(DXF_SAVE_TEST_ROOT) / FPath(FString("io-publish-") + FJsonWriter::Unsigned(MonotonicNanoseconds()));
		FSaveStore Store(Root);
		REQUIRE(Store.Save("manual", Document()) == 1);
		TVector<uint8> Original;
		REQUIRE(ReadFileBytes(Root / "manual.a", Original, 1024));
		auto Next = Document();
		Next.Generation = 2;
		Next.Payload = "{\"score\":9}";
		FFailingPublicationIo Io(Root / "owned-publish.tmp", Root / "manual.b", EncodeSaveFace(Next, {}), Failures[I]);
		bool Confirmed = false;
		bool Unknown = false;
		bool Rejected = false;
		try
		{
			SavePrivate::VerifyPublication(Io);
			Confirmed = true;
		}
		catch (const FFileError& Error)
		{
			REQUIRE(Failures[I] == EPublicationFailure::BeforeRename && Error.GetOperation() == EFileOperation::Publish && Error.GetCode() == 201);
			Rejected = true;
		}
		catch (const FSaveError& Error)
		{
			REQUIRE(I >= 3 && Error.GetCode() == ESaveError::PublicationUnknown);
			Unknown = true;
		}
		REQUIRE(Io.Publishes() == 1 && Io.Reads() == 1);
		REQUIRE(Confirmed == (I == 0 || I == 2));
		REQUIRE(Rejected == (I == 1) && Unknown == (I >= 3));
		REQUIRE(Io.WasInjected() == (Failures[I] == EPublicationFailure::ReadAllocation));
		TVector<uint8> After;
		REQUIRE(ReadFileBytes(Root / "manual.a", After, 1024));
		REQUIRE(After.Size() == Original.Size());
		for (size_t Byte = 0; Byte < Original.Size(); ++Byte)
		{
			REQUIRE(After[Byte] == Original[Byte]);
		}
		REQUIRE(Store.Load("manual").Document->Generation == (Rejected ? uint64(1) : uint64(2)));
		if (!Confirmed)
		{
			Io.AllowRetry();
			SavePrivate::VerifyPublication(Io);
			REQUIRE(Io.Publishes() == 2 && Io.Reads() == 2);
		}
		REQUIRE(Store.Load("manual").Document->Generation == 2);
		REQUIRE(Store.Load("manual").Document->Payload == Next.Payload);
		REQUIRE(Store.Save("manual", Document()) == 3);
		Out << "SAVE_PUBLICATION_FAULT stage=" << I << " confirmed=" << Confirmed << " unknown=" << Unknown << " rejected=" << Rejected << " old_face_bytes_unchanged=1 normal_recovery_generation=3\n";
	}
}
