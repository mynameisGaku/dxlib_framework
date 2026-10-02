// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SaveStorage.h"
#include "Dxf/SaveCodec.h"
#include "Dxf/SaveError.h"
#include "Toolbox/Crc32.h"
#include "Toolbox/FileLock.h"
#include "Toolbox/FileError.h"
#include "Toolbox/FilePublication.h"
#include "Toolbox/JsonWriter.h"
#include "Toolbox/DirectoryFiles.h"
#include "Toolbox/Algorithm.h"
namespace Dxf
{
using namespace Toolbox;
namespace
{
// 単一面の読込結果。確保失敗・OS読取失敗は破損へ変換しない。
struct FFace
{
	// ファイルの存在。
	bool Exists = false;
	// 面検証済みの不変値。
	TOptional<FSaveDocument> Document;
	// 実際に検証した全payloadバイト。同世代衝突に使う。
	FString Bytes;
	// 破損の元の理由。
	FString Reason;
};
// ヘッダーの短い一行を切り出す。
FStringView HeaderLine(FStringView Bytes, size_t& Offset)
{
	// 一行の開始位置。
	const size_t Start = Offset;
	while (Offset < Bytes.Size() && Bytes[Offset] != '\n' && Offset - Start <= 32)
	{
		++Offset;
	}
	if (Offset >= Bytes.Size() || Bytes[Offset] != '\n' || Offset - Start > 32)
	{
		throw FSaveError(ESaveError::Corrupt, "Invalid save face header");
	}
	++Offset;
	return FStringView(Bytes.Data() + Start, Offset - Start - 1);
}
// 整数例外を破損として扱える型へ限定変換する。
uint64 HeaderInteger(FStringView Text)
{
	try
	{
		return FJsonWriter::ParseUnsigned(Text);
	}
	catch (const FException& Error)
	{
		throw FSaveError(ESaveError::Corrupt, Error.What());
	}
}
FFace ReadFace(const FPath& Path, FSaveLimits Limits)
{
	// 上限付きreadで所有する生バイト。
	TVector<uint8> Storage;
	// 文書上限はcodecでも検査し、ヘッダーだけ有限の余白を与える。
	FFace Face;
	try
	{
		Face.Exists = ReadFileBytes(Path, Storage, Limits.MaxBytes + 192);
	}
	catch (const FFileError& Error)
	{
		if (Error.GetOperation() != EFileOperation::SizeLimit)
		{
			throw;
		}
		Face.Exists = true;
		Face.Reason = "Save face exceeds byte limit";
		return Face;
	}
	if (!Face.Exists)
	{
		return Face;
	}
	try
	{
		// 実ファイルの長さ付き表示。
		const FStringView Bytes(reinterpret_cast<const char*>(Storage.Data()), Storage.Size());
		// 次のヘッダー行の位置。
		size_t Offset = 0;
		if (HeaderLine(Bytes, Offset) != "DXFSAVE")
		{
			throw FSaveError(ESaveError::Corrupt, "Save face magic mismatch");
		}
		if (HeaderInteger(HeaderLine(Bytes, Offset)) != 1)
		{
			throw FSaveError(ESaveError::UnsupportedVersion, "Unsupported save face version");
		}
		// 一度も浮動小数点へ変換しない面の世代。
		const uint64 Generation = HeaderInteger(HeaderLine(Bytes, Offset));
		// 実際に書かれたpayloadの長さ。
		const uint64 Length = HeaderInteger(HeaderLine(Bytes, Offset));
		// 実際のバイトを検証するCRC値。
		const uint64 Checksum = HeaderInteger(HeaderLine(Bytes, Offset));
		// ヘッダーを除いた全バイト。
		const FStringView Payload(Bytes.Data() + Offset, Bytes.Size() - Offset);
		if (Generation == 0 || Length != Payload.Size() || Checksum != FCrc32::Compute(Payload))
		{
			throw FSaveError(ESaveError::Corrupt, "Save face length or checksum mismatch");
		}
		Face.Document = FSaveCodec::Decode(Payload, Limits);
		if (Face.Document->Generation != Generation)
		{
			throw FSaveError(ESaveError::Corrupt, "Save face generation mismatch");
		}
		Face.Bytes = FString(Payload.Data(), Payload.Size());
	}
	catch (const FSaveError& Error)
	{
		if (Error.GetCode() == ESaveError::UnsupportedVersion)
		{
			throw;
		}
		Face.Document.Reset();
		Face.Reason = Error.What();
	}
	return Face;
}
// 親を順に作る。既存フォルダーの削除・変更はしない。
void EnsureDirectory(const FPath& Path)
{
	if (IsDirectory(Path))
	{
		return;
	}
	// 再帰がRootで止まることを検査する親パス。
	const FPath Parent = Path.Parent();
	if (Parent.ToUtf8() == Path.ToUtf8() || Parent.ToUtf8().IsEmpty())
	{
		throw FSaveError(ESaveError::InvalidLocation, "Save root is unavailable");
	}
	EnsureDirectory(Parent);
	CreateDirectory(Path);
}
} // namespace
void ValidateSaveLocation(const FPath& Root, FStringView Slot)
{
	// 正規化前に検査する入力。相対参照は整理して通さない。
	const FString& Text = Root.ToUtf8();
#if defined(_WIN32)
	// 初バージョンはdrive完全修飾だけ。曖昧なroot-relativeやdevice pathは拒否する。
	const bool Absolute = Text.Size() >= 3 && ((Text[0] >= 'A' && Text[0] <= 'Z') || (Text[0] >= 'a' && Text[0] <= 'z')) && Text[1] == ':' && (Text[2] == '/' || Text[2] == '\\');
#else
	const bool Absolute = !Text.IsEmpty() && Text[0] == '/';
#endif
	if (!Absolute || Text.Size() > 2048)
	{
		throw FSaveError(ESaveError::InvalidLocation, "SaveRoot must be fully qualified");
	}
	// パス成分の開始位置。..と曖昧な末尾を入力のまま拒否する。
	size_t Start = 0;
	for (size_t I = 0; I <= Text.Size(); ++I)
	{
		if (I < Text.Size() && (static_cast<uint8>(Text[I]) < 0x20 || (Text[I] == ':' && I != 1)))
		{
			throw FSaveError(ESaveError::InvalidLocation, "Invalid SaveRoot character");
		}
		if (I == Text.Size() || Text[I] == '/' || Text[I] == '\\')
		{
			// 一成分の範囲。
			const FStringView Part(Text.CStr() + Start, I - Start);
			if (Part == "." || Part == ".." || (!Part.IsEmpty() && (Part[Part.Size() - 1] == '.' || Part[Part.Size() - 1] == ' ')))
			{
				throw FSaveError(ESaveError::InvalidLocation, "Ambiguous SaveRoot component");
			}
			Start = I + 1;
		}
	}
	if (Slot.IsEmpty() || Slot.Size() > 128)
	{
		throw FSaveError(ESaveError::InvalidLocation, "Invalid slot length");
	}
	// Windowsの予約名を大文字へ正準化する。
	FString Upper;
	for (size_t I = 0; I < Slot.Size(); ++I)
	{
		// slotは短いASCII識別子であり、パスではない。
		const char C = Slot[I];
		// 先頭を含めて許可する文字。
		const bool Letter = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || C == '_';
		if (!Letter && (I == 0 || !((C >= '0' && C <= '9') || C == '-')))
		{
			throw FSaveError(ESaveError::InvalidLocation, "Invalid slot identifier");
		}
		Upper += C >= 'a' && C <= 'z' ? static_cast<char>(C - 'a' + 'A') : C;
	}
	if (Upper == "CON" || Upper == "PRN" || Upper == "AUX" || Upper == "NUL" || (Upper.Size() == 4 && (Upper.Substr(0, 3) == "COM" || Upper.Substr(0, 3) == "LPT") && Upper[3] >= '1' && Upper[3] <= '9'))
	{
		throw FSaveError(ESaveError::InvalidLocation, "Reserved Windows slot name");
	}
}
FSaveLoadResult ReadSaveFaces(const FPath& Root, FStringView Slot, FSaveLimits Limits, FPath* NextFace)
{
	// 書き込み先の名前をschema文字列からではなく検証済みslotから作る。
	const FString Name(Slot.Data(), Slot.Size());
	// 確定面以外のtempは候補にしない。
	const FFace A = ReadFace(Root / FPath(Name + ".a"), Limits);
	const FFace B = ReadFace(Root / FPath(Name + ".b"), Limits);
	if (!A.Document && !B.Document && (A.Exists || B.Exists))
	{
		throw FSaveError(ESaveError::Corrupt, "No valid save face");
	}
	if (A.Document && B.Document && A.Document->Generation == B.Document->Generation && A.Bytes != B.Bytes)
	{
		throw FSaveError(ESaveError::Conflict, "Different valid payloads share one generation");
	}
	// 最大世代の面を保護して、次回の公開先を反対側へ向ける。
	const bool UseA = A.Document && (!B.Document || A.Document->Generation >= B.Document->Generation);
	// 読込値の採用先。
	FSaveLoadResult Result;
	if (A.Document || B.Document)
	{
		Result.Document = UseA ? A.Document : B.Document;
		Result.State = ESaveLoadState::Loaded;
		if ((A.Exists && !A.Document) || (B.Exists && !B.Document))
		{
			Result.State = ESaveLoadState::RecoveredPrevious;
			Result.RecoveryReason = A.Document ? B.Reason : A.Reason;
		}
	}
	if (NextFace != nullptr)
	{
		*NextFace = Root / FPath(Name + (UseA ? ".b" : ".a"));
	}
	return Result;
}
FString CanonicalSaveSlot(FStringView Slot)
{
	// OSの名前規則に合わせる短いslot識別子。
	FString Result;
	for (Toolbox::size_t I = 0; I < Slot.Size(); ++I)
	{
#if defined(_WIN32)
		Result += Slot[I] >= 'A' && Slot[I] <= 'Z' ? static_cast<char>(Slot[I] - 'A' + 'a') : Slot[I];
#else
		Result += Slot[I];
#endif
	}
	return Result;
}
FString EncodeSaveFace(const FSaveDocument& Document, FSaveLimits Limits)
{
	// checksumを取る実際の書込バイト。
	const FString Payload = FSaveCodec::Encode(Document, Limits);
	// 形式バージョン・世代・長さ・checksumをpaddingなしで表現する。
	FString Face("DXFSAVE\n1\n");
	Face += FJsonWriter::Unsigned(Document.Generation);
	Face += '\n';
	Face += FJsonWriter::Unsigned(Payload.Size());
	Face += '\n';
	Face += FJsonWriter::Unsigned(FCrc32::Compute(Payload));
	Face += '\n';
	Face += Payload;
	return Face;
}
FSaveStore::FSaveStore(FPath Root, FSaveLimits Limits) : m_Root(Move(Root)), m_Limits(Limits)
{
	ValidateSaveLocation(m_Root, "root_validation");
	// 不正な上限は最初の操作より前に拒否する。
	FSaveDocument Validation;
	Validation.GameId = "validation";
	Validation.ContentRevision = "validation";
	Validation.SceneKey = "validation";
	Validation.CheckpointKey = "validation";
	FSaveCodec::Encode(Validation, Limits);
	m_Root = m_Root.Normalize();
	EnsureDirectory(m_Root);
}
FPath FSaveStore::DefaultRoot(FStringView ApplicationId)
{
	// OSが返す領域にアプリ識別子を一度だけ加える。
	const FPath Root = UserDataDirectory();
	ValidateSaveLocation(Root, ApplicationId);
	return Root / FPath(FString(ApplicationId.Data(), ApplicationId.Size()));
}
FSaveLoadResult FSaveStore::Load(FStringView Slot) const
{
	ValidateSaveLocation(m_Root, Slot);
	// WindowsのA/aを別系列の世代として扱わない。
	const FString Name = CanonicalSaveSlot(Slot);
	// Writerと読込を同じ非待機のOS排他で直列化する。
	FFileLock Lock(m_Root / FPath(Name + ".lock"));
	return ReadSaveFaces(m_Root, Name, m_Limits, nullptr);
}
TVector<FString> FSaveStore::ListSlots() const
{
	// 重複した二面の名前を一つにする一覧。
	TVector<FString> Slots;
	// tempやlockを候補にしない明示した二つの末尾。
	const char* Suffixes[] = {".a", ".b"};
	for (const char* Suffix : Suffixes)
	{
		for (const FString& File : DirectoryFiles(m_Root, Suffix, 128))
		{
			// 拡張子を除いたslot候補。
			const FString Slot = CanonicalSaveSlot(File.Substr(0, File.Size() - 2));
			try
			{
				ValidateSaveLocation(m_Root, Slot);
			}
			catch (const FSaveError&)
			{
				continue;
			}
			if (Find(Slots.begin(), Slots.end(), Slot) == Slots.end())
			{
				if (Slots.Size() == 128)
				{
					throw FSaveError(ESaveError::InvalidLocation, "Save slot list limit exceeded");
				}
				Slots.PushBack(Slot);
			}
		}
	}
	if (Slots.Size() > 1)
	{
		Sort(Slots.begin(), Slots.end());
	}
	return Slots;
}
FSaveTransaction FSaveStore::Stage(FString Slot, FSaveDocument Document) const
{
	return FSaveTransaction::Prepare(m_Root, Move(Slot), Move(Document), m_Limits);
}
uint64 FSaveStore::Save(FString Slot, FSaveDocument Document) const
{
	// 同期経路も同じ公開前トランザクションを通す。
	FSaveTransaction Transaction = Stage(Move(Slot), Move(Document));
	return Transaction.Publish();
}
const FPath& FSaveStore::GetRoot() const noexcept
{
	return m_Root;
}
} // namespace Dxf
