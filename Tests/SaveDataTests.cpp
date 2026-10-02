// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/SaveStore.h"
#include "Dxf/SaveCodec.h"
#include "Dxf/SaveError.h"
#include "Toolbox/Crc32.h"
#include "Toolbox/FileError.h"
#include "Toolbox/FileLock.h"
#include "Toolbox/FileWriter.h"
#include "Toolbox/FilePublication.h"
#include "Toolbox/JsonWriter.h"
#include "Toolbox/Sha256.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 各ケースが今回だけ所有する新規Root。過去保存やAssetsを上書きしない。
FPath NewRoot()
{
	return FPath(DXF_SAVE_TEST_ROOT) / FPath(FString(reinterpret_cast<const char*>(u8"専用 日本語-")) + FJsonWriter::Unsigned(MonotonicNanoseconds()));
}
// 本体が依存しない独自ゲームの値。
FSaveDocument Document()
{
	FSaveDocument Value;
	Value.GameId = "save_test";
	Value.ContentRevision = "bundle-v1";
	Value.SceneKey = "course/first";
	Value.CheckpointKey = "safe/north";
	Value.Payload = reinterpret_cast<const char*>(u8"{\"score\":42,\"name\":\"鍵と扉\",\"collected\":[\"keyA\"]}");
	return Value;
}
// 本体の面生成を呼ばず、公開formatを試験内で独立に組み立てる。
FString Face(const FSaveDocument& Value, uint64 Version = 1)
{
	const FString Payload = FSaveCodec::Encode(Value);
	return FString("DXFSAVE\n") + FJsonWriter::Unsigned(Version) + "\n" + FJsonWriter::Unsigned(Value.Generation) + "\n" + FJsonWriter::Unsigned(Payload.Size()) + "\n" + FJsonWriter::Unsigned(FCrc32::Compute(Payload)) + "\n" + Payload;
}
// 新規の試験ファイルだけを書き込む。
void Write(const FPath& Path, const FString& Bytes)
{
	FFileWriter Writer(Path);
	Writer.Write(Bytes);
	Writer.Flush();
	Writer.Close();
}
// 試験が作った対象だけを故障化する。
void ReplaceFixture(const FPath& Path, const FString& Bytes)
{
	REQUIRE(RemoveOwnedFile(Path));
	Write(Path, Bytes);
}
// 実ファイルを長さ付きで読む。
FString Read(const FPath& Path)
{
	TVector<uint8> Bytes;
	REQUIRE(ReadFileBytes(Path, Bytes, 9 * 1024 * 1024));
	return FString(reinterpret_cast<const char*>(Bytes.Data()), Bytes.Size());
}
// 期待した拒否だけを成功と数える。
template <typename Function>
void Reject(Function Run, ESaveError Code)
{
	bool Rejected = false;
	try
	{
		Run();
	}
	catch (const FSaveError& Error)
	{
		REQUIRE(Error.GetCode() == Code);
		Rejected = true;
	}
	REQUIRE(Rejected);
}
} // namespace
TEST("Save CRC known vectors and changed bytes")
{
	REQUIRE(FCrc32::Compute("") == 0);
	REQUIRE(FCrc32::Compute("123456789") == 0xcbf43926U);
	REQUIRE(FCrc32::Compute("123456788") != 0xcbf43926U);
}
TEST("Save SHA256 standard known vectors and two block padding")
{
	REQUIRE(FSha256::Compute("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	REQUIRE(FSha256::Compute("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	REQUIRE(FSha256::Compute("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}
TEST("Save SHA256 binary block boundaries match independent reference digests")
{
	// Python hashlibで独立に求めた、0〜255の繰り返しバイト列の期待値。
	const char* Expected[] = {"463eb28e72f82e0a96c0a4cc53690c571281131f672aa229e0d45ae59b598b59", "da2ae4d6b36748f2a318f23e7ab1dfdf45acdc9d049bd80e59de82a60895f562", "29af2686fd53374a36b0846694cc342177e428d1647515f078784d69cdb9e488", "fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108", "4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781", "92ca0fa6651ee2f97b884b7246a562fa71250fedefe5ebf270d31c546bfea976", "471fb943aa23c511f6f72f8d1652d9c880cfa392ad80503120547703e56a2be5", "5099c6a56203f9687f7d33f4bfdf576d31dc91f6b695ecea38b2770c87631135"};
	const size_t Lengths[] = {55, 56, 63, 64, 65, 127, 128, 129};
	for (size_t I = 0; I < 8; ++I)
	{
		FString Bytes;
		Bytes.Resize(Lengths[I]);
		for (size_t J = 0; J < Bytes.Size(); ++J)
		{
			Bytes[J] = static_cast<char>(static_cast<uint8>(J));
		}
		REQUIRE(FSha256::Compute(Bytes) == Expected[I]);
	}
}
TEST("Save explicitly migrates legacy game payload fixture without changing file or generation")
{
	// この機能で宣言する旧ゲームschema fixture。過去の製品バージョン互換とは主張しない。
	const FPath Fixture = FPath(DXF_SAVE_FIXTURE_ROOT) / FPath("legacy_progress_v1.json");
	const FString Before = Read(Fixture);
	const auto Legacy = FSaveCodec::Decode(Before);
	FSaveDocument Expected = Document();
	Expected.GameVersion = 2;
	FSavePayloadMigration Migration;
	Migration.FromVersion = 1;
	Migration.ToVersion = 2;
	Migration.Convert = [](FStringView Bytes)
	{
		// coinsだけを持つ既知の旧値を検証し、scoreへの改名を行う。
		const auto Json = FJsonDocument::Parse(Bytes);
		const auto Index = Json.Find(0, "coins");
		if (Json.Size() != 2 || Index < 0 || Json.Get(Index).Kind != EJsonKind::Number)
		{
			throw FException("Invalid legacy game payload");
		}
		return FString("{\"score\":") + FJsonWriter::Unsigned(FJsonWriter::ParseUnsigned(Json.Get(Index).Text)) + "}";
	};
	const auto Migrated = FSaveCodec::Migrate(Legacy, Expected, Migration);
	REQUIRE(Migrated.GameVersion == 2 && Migrated.Generation == Legacy.Generation);
	REQUIRE(Migrated.Payload == "{\"score\":7}");
	REQUIRE(Legacy.Payload == "{\"coins\":7}");
	REQUIRE(Read(Fixture) == Before);
	// 新しい専用系列へ明示保存するまで、元保存は上書きしない。
	FSaveStore Store(NewRoot());
	REQUIRE(Store.Save("migrated", Migrated) == 1);
	REQUIRE(Store.Load("migrated").Document->Payload == "{\"score\":7}");
}
TEST("Save migration rejects future payload and incompatible identity before conversion")
{
	FSaveDocument Source = Document();
	FSaveDocument Expected = Document();
	Expected.GameVersion = 2;
	FSavePayloadMigration Migration;
	Migration.FromVersion = 1;
	Migration.ToVersion = 2;
	Migration.Convert = [](FStringView) -> FString
	{
		throw FException("Conversion must not run for incompatible identity");
	};
	Source.GameId = "another_game";
	Reject([&]
	       {
		       FSaveCodec::Migrate(Source, Expected, Migration);
	       },
	       ESaveError::IncompatibleContent);
	Source = Document();
	Source.ContentRevision = "other";
	Reject([&]
	       {
		       FSaveCodec::Migrate(Source, Expected, Migration);
	       },
	       ESaveError::IncompatibleContent);
	Source = Document();
	Source.GameVersion = 3;
	Reject([&]
	       {
		       FSaveCodec::Migrate(Source, Expected, Migration);
	       },
	       ESaveError::UnsupportedVersion);
	Source = Document();
	Reject([&]
	       {
		       FSaveCodec::Migrate(Source, Expected);
	       },
	       ESaveError::UnsupportedVersion);
}
TEST("Save migration revalidates output schema syntax and configured byte limit")
{
	FSaveDocument Expected = Document();
	Expected.GameVersion = 2;
	FSavePayloadMigration Migration;
	Migration.FromVersion = 1;
	Migration.ToVersion = 2;
	Migration.Convert = [](FStringView)
	{
		return FString("[]");
	};
	Reject([&]
	       {
		       FSaveCodec::Migrate(Document(), Expected, Migration);
	       },
	       ESaveError::InvalidDocument);
	Migration.Convert = [](FStringView)
	{
		return FString("{\"x\":1,\"x\":2}");
	};
	Reject([&]
	       {
		       FSaveCodec::Migrate(Document(), Expected, Migration);
	       },
	       ESaveError::InvalidDocument);
	Migration.Convert = [](FStringView)
	{
		FString Text;
		Text.Resize(600);
		for (size_t I = 0; I < Text.Size(); ++I)
		{
			Text[I] = 'a';
		}
		return FString("{\"text\":") + FJsonWriter::Quote(Text) + "}";
	};
	FSaveLimits Limits;
	Limits.MaxBytes = 512;
	Limits.MaxStringBytes = 512;
	Reject([&]
	       {
		       FSaveCodec::Migrate(Document(), Expected, Migration, Limits);
	       },
	       ESaveError::InvalidDocument);
}
TEST("Save writer sorts decoded keys and preserves array order")
{
	REQUIRE(FJsonWriter::Write(FJsonDocument::Parse("{\"z\":[2,1],\"a\":18446744073709551615}")) == "{\"a\":18446744073709551615,\"z\":[2,1]}");
}
TEST("Save writer escapes control NUL quote and Japanese UTF8")
{
	const char Bytes[] = {'a', 0, '\n', '"', '\\'};
	const FString Quoted = FJsonWriter::Quote(FStringView(Bytes, sizeof(Bytes)));
	REQUIRE(FJsonDocument::Parse(Quoted).Get(0).Text == FString(Bytes, sizeof(Bytes)));
	REQUIRE(FJsonDocument::Parse(FJsonWriter::Quote(reinterpret_cast<const char*>(u8"日本語"))).Get(0).Text == reinterpret_cast<const char*>(u8"日本語"));
}
TEST("Save writer rejects invalid UTF8")
{
	bool Rejected = false;
	try
	{
		FJsonWriter::Quote(FStringView("\xc0\xaf", 2));
	}
	catch (const FException&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected);
}
TEST("Save unsigned maximum and above f64 exact range")
{
	REQUIRE(FJsonWriter::ParseUnsigned("18446744073709551615") == TNumericLimits<uint64>::Max());
	REQUIRE(FJsonWriter::Unsigned(TNumericLimits<uint64>::Max()) == "18446744073709551615");
	REQUIRE(FJsonWriter::ParseUnsigned("9007199254740993") == 9007199254740993ULL);
}
TEST("Save unsigned rejects signs leading zero fraction and overflow")
{
	const char* Inputs[] = {"", "00", "01", "-1", "+1", "1.0", "1e2", "18446744073709551616"};
	for (const char* Input : Inputs)
	{
		bool Rejected = false;
		try
		{
			FJsonWriter::ParseUnsigned(Input);
		}
		catch (const FException&)
		{
			Rejected = true;
		}
		REQUIRE(Rejected);
	}
}
TEST("Save envelope round trip preserves exact generation and payload")
{
	FSaveDocument Value = Document();
	Value.Generation = 9007199254740993ULL;
	const FString Bytes = FSaveCodec::Encode(Value);
	const FSaveDocument Decoded = FSaveCodec::Decode(Bytes);
	REQUIRE(Decoded.Generation == Value.Generation);
	REQUIRE(Decoded.CheckpointKey == Value.CheckpointKey);
	REQUIRE(Decoded.Payload == FJsonWriter::Write(FJsonDocument::Parse(Value.Payload)));
	REQUIRE(FSaveCodec::Encode(Decoded) == Bytes);
}
TEST("Save envelope rejects malformed JSON with input position")
{
	const char* Inputs[] = {"{}{}", "{\"x\":1,\"\\u0078\":2}", "{\"x\":1e400}", "{\"x\":\"\\ud800\"}", "{\"x\":0,}", "{\"x\":/* comment */0}"};
	for (const char* Input : Inputs)
	{
		Reject([&]
		       {
			       FSaveCodec::Decode(Input);
		       },
		       ESaveError::InvalidDocument);
	}
	try
	{
		FSaveCodec::Decode("{\n\"format\": ]}");
		REQUIRE(false);
	}
	catch (const FSaveError& Error)
	{
		REQUIRE(Error.GetLine() == 2);
		REQUIRE(Error.GetColumn() > 1);
	}
}
TEST("Save future version is distinguished from corrupt schema")
{
	Reject([]
	       {
		       FSaveCodec::Decode("{\"format\":2}");
	       },
	       ESaveError::UnsupportedVersion);
	Reject([]
	       {
		       FSaveCodec::Decode("{\"format\":1}");
	       },
	       ESaveError::InvalidDocument);
}
TEST("Save unknown field is refused")
{
	const FString Bytes = FSaveCodec::Encode(Document());
	const FString Unknown = FString("{\"unknown\":true,") + Bytes.Substr(1);
	Reject([&]
	       {
		       FSaveCodec::Decode(Unknown);
	       },
	       ESaveError::InvalidDocument);
}
TEST("Save finite document and nesting limits apply to envelope")
{
	FSaveLimits Limits;
	Limits.MaxBytes = 200;
	Limits.MaxStringBytes = 100;
	bool Rejected = false;
	try
	{
		FSaveCodec::Encode(Document(), Limits);
	}
	catch (const FException&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected);
	FSaveDocument Value = Document();
	Value.Payload = "{\"a\":{\"b\":{}}}";
	Limits = {};
	Limits.MaxDepth = 3;
	Rejected = false;
	try
	{
		FSaveCodec::Encode(Value, Limits);
	}
	catch (const FException&)
	{
		Rejected = true;
	}
	REQUIRE(Rejected);
}
TEST("Save compatibility rejects game version content scene dimension and kind")
{
	const FSaveDocument Expected = Document();
	for (uint32 Case = 0; Case < 6; ++Case)
	{
		FSaveDocument Other = Expected;
		switch (Case)
		{
		case 0:
			Other.GameId = "other";
			break;
		case 1:
			Other.GameVersion = 2;
			break;
		case 2:
			Other.ContentRevision = "bundle-v2";
			break;
		case 3:
			Other.SceneKey = "course/second";
			break;
		case 4:
			Other.Dimension = 3;
			break;
		case 5:
			Other.Kind = ESaveDocumentKind::Settings;
			break;
		}
		Reject([&]
		       {
			       FSaveCodec::RequireCompatible(Other, Expected);
		       },
		       ESaveError::IncompatibleContent);
	}
}
TEST("Save settings has a separate envelope and series")
{
	FSaveDocument Settings = Document();
	Settings.Kind = ESaveDocumentKind::Settings;
	Settings.Dimension = 0;
	Settings.SceneKey = "";
	Settings.CheckpointKey = "";
	Settings.ContentRevision = "";
	REQUIRE(FSaveCodec::Decode(FSaveCodec::Encode(Settings)).Kind == ESaveDocumentKind::Settings);
	FSaveStore Store(NewRoot());
	REQUIRE(Store.Save("progress", Document()) == 1);
	Reject([&]
	       {
		       Store.Save("progress", Settings);
	       },
	       ESaveError::IncompatibleContent);
	REQUIRE(Store.Save("settings", Settings) == 1);
	REQUIRE(Store.Load("progress").Document->Kind == ESaveDocumentKind::Progress);
}
TEST("Save root rejects relative and traversal without fallback")
{
	const char* Roots[] = {"relative", "C:relative", "C:/test/../outside", "C:/test/./save", "C:/test/trailing."};
	for (const char* Root : Roots)
	{
		Reject([&]
		       {
			       FSaveStore Store{FPath(Root)};
		       },
		       ESaveError::InvalidLocation);
	}
}
TEST("Save slot rejects path and Windows reserved names")
{
	FSaveStore Store(NewRoot());
	const char* Slots[] = {"", "../save", "a/b", "a\\b", "C:save", "CON", "nul", "com1", "LPT9", "bad.name", "1slot"};
	for (const char* Slot : Slots)
	{
		Reject([&]
		       {
			       Store.Load(Slot);
		       },
		       ESaveError::InvalidLocation);
	}
	const char Nul[] = {'a', 0, 'b'};
	Reject([&]
	       {
		       Store.Load(FStringView(Nul, sizeof(Nul)));
	       },
	       ESaveError::InvalidLocation);
}
TEST("Save empty slot and alternate generations protect latest face")
{
	FSaveStore Store(NewRoot());
	REQUIRE(Store.Load("manual").State == ESaveLoadState::Empty);
	REQUIRE(Store.Save("manual", Document()) == 1);
	const FString First = Read(Store.GetRoot() / FPath("manual.a"));
	REQUIRE(Store.Save("manual", Document()) == 2);
	REQUIRE(Read(Store.GetRoot() / FPath("manual.a")) == First);
	REQUIRE(Store.Load("manual").Document->Generation == 2);
	const FString Second = Read(Store.GetRoot() / FPath("manual.b"));
	REQUIRE(Store.Save("manual", Document()) == 3);
	REQUIRE(Read(Store.GetRoot() / FPath("manual.b")) == Second);
	REQUIRE(Store.Load("manual").Document->Generation == 3);
}
TEST("Save unpublished temp is never a load candidate")
{
	FSaveStore Store(NewRoot());
	Store.Save("manual", Document());
	FSaveDocument Future = Document();
	Future.Generation = 999;
	Write(Store.GetRoot() / FPath("manual.tmp-orphan"), Face(Future));
	REQUIRE(Store.Load("manual").Document->Generation == 1);
	REQUIRE(Store.Save("manual", Document()) == 2);
}
TEST("Save prepared cancellation keeps last normal generation")
{
	FSaveStore Store(NewRoot());
	Store.Save("manual", Document());
	FSaveTransaction Transaction = Store.Stage("manual", Document());
	REQUIRE(Transaction.Cancel());
	Reject([&]
	       {
		       Transaction.Publish();
	       },
	       ESaveError::TerminalRequest);
	REQUIRE(Store.Load("manual").Document->Generation == 1);
	REQUIRE(Store.Save("manual", Document()) == 2);
}
TEST("Save cancellation after publish cannot claim Canceled")
{
	FSaveStore Store(NewRoot());
	FSaveTransaction Transaction = Store.Stage("manual", Document());
	REQUIRE(Transaction.Publish() == 1);
	REQUIRE(Transaction.IsCommitted());
	REQUIRE(!Transaction.Cancel());
	REQUIRE(Store.Load("manual").Document->Generation == 1);
}
TEST("Save second Store same slot is Busy but other slot works")
{
	const FPath Root = NewRoot();
	FSaveStore A(Root);
	FSaveStore B(Root);
	FSaveTransaction Transaction = A.Stage("manual", Document());
	bool Busy = false;
	try
	{
		B.Save("manual", Document());
	}
	catch (const FFileError& Error)
	{
		REQUIRE(Error.GetOperation() == EFileOperation::Busy);
		Busy = true;
	}
	REQUIRE(Busy);
	REQUIRE(B.Save("other", Document()) == 1);
	REQUIRE(Transaction.Cancel());
	REQUIRE(B.Save("manual", Document()) == 1);
}
TEST("Save abandoned transaction leaves normal face and releases OS lock")
{
	FSaveStore Store(NewRoot());
	Store.Save("manual", Document());
	{
		FSaveTransaction Abandoned = Store.Stage("manual", Document());
	}
	REQUIRE(Store.Load("manual").Document->Generation == 1);
	REQUIRE(Store.Save("manual", Document()) == 2);
}
TEST("Save corrupt latest checksum recovers older face with reason")
{
	FSaveStore Store(NewRoot());
	Store.Save("manual", Document());
	FSaveDocument Value = Document();
	Value.Payload = "{\"score\":100}";
	Store.Save("manual", Value);
	FString Broken = Read(Store.GetRoot() / FPath("manual.b"));
	Broken += 'x';
	ReplaceFixture(Store.GetRoot() / FPath("manual.b"), Broken);
	const FSaveLoadResult Recovered = Store.Load("manual");
	REQUIRE(Recovered.State == ESaveLoadState::RecoveredPrevious);
	REQUIRE(!Recovered.RecoveryReason.IsEmpty());
	REQUIRE(Recovered.Document->Generation == 1);
	REQUIRE(Store.Save("manual", Value) == 2);
	REQUIRE(Store.Load("manual").State == ESaveLoadState::Loaded);
}
TEST("Save both corrupt faces refuse load and overwrite")
{
	FSaveStore Store(NewRoot());
	Write(Store.GetRoot() / FPath("manual.a"), "broken-a");
	Write(Store.GetRoot() / FPath("manual.b"), "broken-b");
	Reject([&]
	       {
		       Store.Load("manual");
	       },
	       ESaveError::Corrupt);
	Reject([&]
	       {
		       Store.Save("manual", Document());
	       },
	       ESaveError::Corrupt);
	REQUIRE(Read(Store.GetRoot() / FPath("manual.a")) == "broken-a");
}
TEST("Save future face prevents older face downgrade and overwrite")
{
	FSaveStore Store(NewRoot());
	Store.Save("manual", Document());
	FSaveDocument Value = Document();
	Value.Generation = 2;
	Write(Store.GetRoot() / FPath("manual.b"), Face(Value, 2));
	Reject([&]
	       {
		       Store.Load("manual");
	       },
	       ESaveError::UnsupportedVersion);
	Reject([&]
	       {
		       Store.Save("manual", Document());
	       },
	       ESaveError::UnsupportedVersion);
	REQUIRE(Read(Store.GetRoot() / FPath("manual.b")) == Face(Value, 2));
}
TEST("Save divergent equal generations are conflict")
{
	FSaveStore Store(NewRoot());
	FSaveDocument Value = Document();
	Value.Generation = 5;
	Write(Store.GetRoot() / FPath("manual.a"), Face(Value));
	Value.Payload = "{\"score\":99}";
	Write(Store.GetRoot() / FPath("manual.b"), Face(Value));
	Reject([&]
	       {
		       Store.Load("manual");
	       },
	       ESaveError::Conflict);
}
TEST("Save equal generation equal bytes are not conflict")
{
	FSaveStore Store(NewRoot());
	FSaveDocument Value = Document();
	Value.Generation = 5;
	Write(Store.GetRoot() / FPath("manual.a"), Face(Value));
	Write(Store.GetRoot() / FPath("manual.b"), Face(Value));
	REQUIRE(Store.Load("manual").Document->Generation == 5);
	REQUIRE(Store.Save("manual", Document()) == 6);
}
TEST("Save maximum generation rejects increment without changing face")
{
	FSaveStore Store(NewRoot());
	FSaveDocument Value = Document();
	Value.Generation = TNumericLimits<uint64>::Max();
	const FString Bytes = Face(Value);
	Write(Store.GetRoot() / FPath("manual.a"), Bytes);
	REQUIRE(Store.Load("manual").Document->Generation == TNumericLimits<uint64>::Max());
	Reject([&]
	       {
		       Store.Save("manual", Document());
	       },
	       ESaveError::GenerationOverflow);
	REQUIRE(Read(Store.GetRoot() / FPath("manual.a")) == Bytes);
}
TEST("Save file writer never truncates existing normal file")
{
	FSaveStore Store(NewRoot());
	Store.Save("manual", Document());
	const FPath Path = Store.GetRoot() / FPath("manual.a");
	const FString Before = Read(Path);
	bool Rejected = false;
	try
	{
		FFileWriter Writer(Path);
	}
	catch (const FFileError& Error)
	{
		REQUIRE(Error.GetOperation() == EFileOperation::Create);
		Rejected = true;
	}
	REQUIRE(Rejected);
	REQUIRE(Read(Path) == Before);
}
TEST("Save slot list includes distinct faces but ignores temp lock and invalid names")
{
	FSaveStore Store(NewRoot());
	REQUIRE(Store.ListSlots().IsEmpty());
	Store.Save("manual", Document());
	Store.Save("manual", Document());
	Store.Save("auto", Document());
	Write(Store.GetRoot() / FPath("manual.tmp-unpublished"), "unused");
	Write(Store.GetRoot() / FPath("bad.name.a"), "invalid");
	const auto Slots = Store.ListSlots();
	REQUIRE(Slots.Size() == 2);
	REQUIRE(Slots[0] == "auto" && Slots[1] == "manual");
}
TEST("Save oversized corrupt face recovers valid bounded face")
{
	FSaveLimits Limits;
	Limits.MaxBytes = 512;
	Limits.MaxStringBytes = 512;
	FSaveStore Store(NewRoot(), Limits);
	Store.Save("manual", Document());
	FString Broken;
	Broken.Resize(900);
	Write(Store.GetRoot() / FPath("manual.b"), Broken);
	REQUIRE(Store.Load("manual").State == ESaveLoadState::RecoveredPrevious);
	REQUIRE(Store.Load("manual").Document->Generation == 1);
}
TEST("Save oversized future face refuses downgrade and overwrite")
{
	FSaveLimits Limits;
	Limits.MaxBytes = 512;
	Limits.MaxStringBytes = 512;
	FSaveStore Store(NewRoot(), Limits);
	Store.Save("manual", Document());
	const FString Protected = Read(Store.GetRoot() / FPath("manual.a"));
	FString Future("DXFSAVE\n2\n");
	Future.Resize(900);
	Write(Store.GetRoot() / FPath("manual.b"), Future);
	Reject([&]
	       {
		       Store.Load("manual");
	       },
	       ESaveError::UnsupportedVersion);
	Reject([&]
	       {
		       Store.Save("manual", Document());
	       },
	       ESaveError::UnsupportedVersion);
	REQUIRE(Read(Store.GetRoot() / FPath("manual.a")) == Protected);
	REQUIRE(Read(Store.GetRoot() / FPath("manual.b")) == Future);
}
TEST("Save direct transaction validates byte limits before file access")
{
	const FPath Location = NewRoot();
	FSaveLimits Invalid;
	Invalid.MaxBytes = TNumericLimits<size_t>::Max();
	Reject([&]
	       {
		       FSaveTransaction::Prepare(Location, "manual", Document(), Invalid);
	       },
	       ESaveError::InvalidDocument);
	REQUIRE(!IsDirectory(Location));
}
TEST("File prefix reads only requested bytes while full read rejects excess")
{
	FSaveStore Store(NewRoot());
	const FPath Path = Store.GetRoot() / FPath("prefix-test");
	Write(Path, "0123456789");
	TVector<uint8> Bytes;
	REQUIRE(ReadFilePrefix(Path, Bytes, 3));
	REQUIRE(Bytes.Size() == 3 && Bytes[0] == '0' && Bytes[2] == '2');
	bool Failed = false;
	try
	{
		ReadFileBytes(Path, Bytes, 3);
	}
	catch (const FFileError& Error)
	{
		REQUIRE(Error.GetOperation() == EFileOperation::SizeLimit);
		Failed = true;
	}
	REQUIRE(Failed && Bytes.Size() == 3);
	REQUIRE(ReadFilePrefix(Path, Bytes, 12) && Bytes.Size() == 10);
	REQUIRE(ReadFilePrefix(Path, Bytes, 0) && Bytes.IsEmpty());
}
#if defined(_WIN32)
TEST("Save publication sharing violation retains protected face and cannot report canceled")
{
	FSaveStore Store(NewRoot());
	Store.Save("manual", Document());
	const FString Protected = Read(Store.GetRoot() / FPath("manual.a"));
	FSaveTransaction Transaction = Store.Stage("manual", Document());
	{
		// この試験が作る反対面だけを共有拒否で開く。
		FFileLock Destination(Store.GetRoot() / FPath("manual.b"));
		Reject([&]
		       {
			       Transaction.Publish();
		       },
		       ESaveError::PublicationUnknown);
		REQUIRE(!Transaction.Cancel());
		REQUIRE(!Transaction.IsCommitted());
		REQUIRE(Read(Store.GetRoot() / FPath("manual.a")) == Protected);
	}
	// 未確定の同じ要求が、排他を保ったまま公開の回復を確認できる。
	REQUIRE(Transaction.Publish() == 2);
	REQUIRE(Store.Load("manual").Document->Generation == 2);
}
#endif
