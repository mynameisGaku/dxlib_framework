// SPDX-License-Identifier: NOASSERTION
#include "SaveCourseProgressCodec.h"
#include "Dxf/SaveError.h"
#include "Toolbox/JsonWriter.h"
#include "Toolbox/JsonError.h"
namespace Dxf::GameplaySample
{
namespace
{
using namespace Toolbox;
// ゲーム側の不正値を既存の保存診断へ渡す。
void Require(bool Condition, const char* Reason)
{
	if (!Condition)
	{
		throw FSaveError(ESaveError::InvalidDocument, Reason);
	}
}
// このコースだけが知る二つの鍵。保存コアへゲームの規則を置かない。
bool KnownKey(FStringView Key)
{
	return Key == "northKey" || Key == "southKey";
}
// 読解と書込の同じ有限上限。
FJsonLimits Limits()
{
	return {16 * 1024, 4, 128, 64};
}
// 名前と型の確認を終えてから値へ変換する。
const FJsonValue& Field(const FJsonDocument& Json, int32 Parent, const char* Name, EJsonKind Kind)
{
	// 復号済みの名前で一度だけ解決する位置。
	const int32 Index = Json.Find(Parent, Name);
	Require(Index >= 0 && Json.Get(Index).Kind == Kind, "Save course progress field missing or wrongly typed");
	return Json.Get(Index);
}
// 余分なフィールドも誤った保存として扱い、黙って捨てない。
void Fields(const FJsonDocument& Json, int32 Parent, const char* const* Names, size_t Count)
{
	Require(Json.Get(Parent).Kind == EJsonKind::Object, "Save course progress object expected");
	// 必須名を確認してから件数を照合する。重複は共有JSON読解器が拒否する。
	for (size_t I = 0; I < Count; ++I)
	{
		Require(Json.Find(Parent, Names[I]) >= 0, "Save course progress field missing");
	}
	// 実際に宣言されたメンバーの数。
	size_t Actual = 0;
	for (int32 Child = Json.Get(Parent).FirstChild; Child >= 0; Child = Json.Get(Child).NextSibling)
	{
		++Actual;
	}
	Require(Actual == Count, "Save course progress unknown field");
}
} // namespace
void FSaveCourseProgressCodec::Validate(const FSaveCourseProgress& Progress)
{
	Require(Progress.Checkpoint == "start" || Progress.Checkpoint == "north", "Save course checkpoint is not allowed");
	Require(Progress.CollectedKeys.Size() <= 2, "Save course key count exceeds catalog");
	// 取得済みキーを許可集合と照合する位置。
	for (Toolbox::size_t I = 0; I < Progress.CollectedKeys.Size(); ++I)
	{
		Require(KnownKey(Progress.CollectedKeys[I]), "Save course collected key is not allowed");
		// 今回のキーより前に保存されたキーの位置。
		for (Toolbox::size_t Previous = 0; Previous < I; ++Previous)
		{
			Require(Progress.CollectedKeys[I] != Progress.CollectedKeys[Previous], "Save course collected key is duplicated");
		}
	}
	// 名前付きで保存する二つの扉の内側の位置。
	for (Toolbox::size_t I = 0; I < 2; ++I)
	{
		Require(!Progress.bDoorOpen[I] || Progress.bDoorUnlocked[I], "Save course locked door cannot be open");
	}
}
Toolbox::FString FSaveCourseProgressCodec::Encode(const FSaveCourseProgress& Progress)
{
	Validate(Progress);
	// コースの許可集合順に書き、共有する取得順を並べ替えない。
	Toolbox::FString Keys = "[";
	// 辞書とは別に、集合を書き出すときの安定したキー順。
	const char* KeyNames[] = {"northKey", "southKey"};
	// 最初の要素の前には区切りを入れない。
	bool First = true;
	for (const auto* Name : KeyNames)
	{
		for (const auto& Key : Progress.CollectedKeys)
		{
			if (Key == Name)
			{
				if (!First)
				{
					Keys += ',';
				}
				Keys += Toolbox::FJsonWriter::Quote(Name);
				First = false;
			}
		}
	}
	Keys += ']';
	// 保存面の新旧を決める値はStoreの世代であり、これはゲーム進行の通し番号。
	Toolbox::FString Bytes = "{\"payloadVersion\":1,\"checkpoint\":";
	Bytes += Toolbox::FJsonWriter::Quote(Progress.Checkpoint);
	Bytes += ",\"collectedKeys\":" + Keys;
	Bytes += ",\"score\":\"" + Toolbox::FJsonWriter::Unsigned(Progress.Score) + "\"";
	Bytes += ",\"spawnCounter\":\"" + Toolbox::FJsonWriter::Unsigned(Progress.SpawnCounter) + "\"";
	Bytes += ",\"revision\":\"" + Toolbox::FJsonWriter::Unsigned(Progress.Revision) + "\",\"doors\":{";
	// 配列位置を保存せず、扉を論理キーへ対応させる表。
	const char* DoorNames[] = {"doorA", "doorB"};
	for (Toolbox::size_t I = 0; I < 2; ++I)
	{
		if (I != 0)
		{
			Bytes += ',';
		}
		Bytes += Toolbox::FJsonWriter::Quote(DoorNames[I]);
		Bytes += ":{\"unlocked\":";
		Bytes += Progress.bDoorUnlocked[I] ? "true" : "false";
		Bytes += ",\"open\":";
		Bytes += Progress.bDoorOpen[I] ? "true" : "false";
		Bytes += '}';
	}
	Bytes += "}}";
	// 辞書順と厳格な文字・数値の契約は既存Toolboxを使う。
	return Toolbox::FJsonWriter::Write(Toolbox::FJsonDocument::Parse(Bytes, Limits()), 0, Limits().MaxBytes);
}
FSaveCourseProgress FSaveCourseProgressCodec::Decode(Toolbox::FStringView Bytes)
{
	try
	{
		// 長さ付きの入力を全量読み、未公開の候補値だけを構築する。
		const auto Json = Toolbox::FJsonDocument::Parse(Bytes, Limits());
		// ゲーム進行のバージョン。保存面の形式バージョンとは別。
		const auto& Version = Field(Json, 0, "payloadVersion", Toolbox::EJsonKind::Number);
		if (Toolbox::FJsonWriter::ParseUnsigned(Version.Text) != 1)
		{
			throw FSaveError(ESaveError::UnsupportedVersion, "Unsupported save course payload version");
		}
		// このバージョンで必須の進行フィールド。
		const char* Names[] = {"payloadVersion", "checkpoint", "collectedKeys", "score", "spawnCounter", "revision", "doors"};
		Fields(Json, 0, Names, 7);
		// 現在ゲームにまだ採用しない、全項目の候補。
		FSaveCourseProgress Result;
		Result.Checkpoint = Field(Json, 0, "checkpoint", Toolbox::EJsonKind::String).Text;
		Result.Score = Toolbox::FJsonWriter::ParseUnsigned(Field(Json, 0, "score", Toolbox::EJsonKind::String).Text);
		Result.SpawnCounter = Toolbox::FJsonWriter::ParseUnsigned(Field(Json, 0, "spawnCounter", Toolbox::EJsonKind::String).Text);
		Result.Revision = Toolbox::FJsonWriter::ParseUnsigned(Field(Json, 0, "revision", Toolbox::EJsonKind::String).Text);
		// 取得済みのものを再発生させないための論理キー集合。
		const auto& Keys = Field(Json, 0, "collectedKeys", Toolbox::EJsonKind::Array);
		for (Toolbox::int32 Index = Keys.FirstChild; Index >= 0; Index = Json.Get(Index).NextSibling)
		{
			Require(Result.CollectedKeys.Size() < 2 && Json.Get(Index).Kind == Toolbox::EJsonKind::String, "Save course collected key entry invalid");
			Result.CollectedKeys.PushBack(Json.Get(Index).Text);
		}
		(void)Field(Json, 0, "doors", Toolbox::EJsonKind::Object);
		// 保存配列の位置に依存しない、名前付きの扉状態。
		const Toolbox::int32 Doors = Json.Find(0, "doors");
		// このゲームが許可する公開個体名。
		const char* DoorNames[] = {"doorA", "doorB"};
		Fields(Json, Doors, DoorNames, 2);
		for (Toolbox::size_t I = 0; I < 2; ++I)
		{
			(void)Field(Json, Doors, DoorNames[I], Toolbox::EJsonKind::Object);
			// 今回の論理キーに対応する状態Object。
			const Toolbox::int32 Door = Json.Find(Doors, DoorNames[I]);
			// 保存対象とする二つのゲーム状態。
			const char* Flags[] = {"open", "unlocked"};
			Fields(Json, Door, Flags, 2);
			Result.bDoorOpen[I] = Field(Json, Door, "open", Toolbox::EJsonKind::Boolean).Boolean;
			Result.bDoorUnlocked[I] = Field(Json, Door, "unlocked", Toolbox::EJsonKind::Boolean).Boolean;
		}
		Validate(Result);
		return Result;
	}
	catch (const Toolbox::FJsonError& Error)
	{
		throw FSaveError(ESaveError::InvalidDocument, Error.What(), Error.GetLine(), Error.GetColumn());
	}
	catch (const FSaveError&)
	{
		throw;
	}
	catch (const Toolbox::FException& Error)
	{
		// 十進整数の不正も保存文書の診断として返す。
		throw FSaveError(ESaveError::InvalidDocument, Error.What());
	}
}
} // namespace Dxf::GameplaySample
