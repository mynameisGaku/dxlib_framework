// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SaveCodec.h"
#include "Dxf/SaveError.h"
#include "Toolbox/JsonError.h"
#include "Toolbox/JsonWriter.h"
namespace Dxf
{
using namespace Toolbox;
namespace
{
// 入力と出力の双方で使う有限上限。
FJsonLimits JsonLimits(FSaveLimits Limits)
{
	if (Limits.MaxBytes == 0 || Limits.MaxBytes > 64 * 1024 * 1024 || Limits.MaxDepth == 0 || Limits.MaxDepth > 64 || Limits.MaxStringBytes == 0 || Limits.MaxStringBytes > Limits.MaxBytes || Limits.MaxValues == 0 || Limits.MaxKeyBytes == 0 || Limits.MaxKeyBytes > Limits.MaxStringBytes)
	{
		throw FSaveError(ESaveError::InvalidDocument, "Invalid save limits");
	}
	return {Limits.MaxBytes, Limits.MaxDepth, Limits.MaxStringBytes, Limits.MaxValues};
}
// ASCIIの論理キーだけを解決入口へ通す。
bool IsKey(FStringView Text, size_t Limit, bool Hierarchy)
{
	if (Text.IsEmpty() || Text.Size() > Limit)
	{
		return false;
	}
	// 各階層の先頭を検査する。
	bool First = true;
	for (size_t I = 0; I < Text.Size(); ++I)
	{
		// 入力の一文字。
		const char C = Text[I];
		if (Hierarchy && C == '/' && !First)
		{
			First = true;
			continue;
		}
		// 先頭に使えるASCII文字。
		const bool Letter = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || C == '_';
		if (!Letter && (First || !((C >= '0' && C <= '9') || C == '-')))
		{
			return false;
		}
		First = false;
	}
	return !First;
}
// 必須fieldを型付きで取得し、原因位置を保持する。
const FJsonValue& Field(const FJsonDocument& Json, const char* Key, EJsonKind Kind)
{
	// 順序に依存しない参照解決。
	const int32 Index = Json.Find(0, Key);
	if (Index < 0)
	{
		throw FSaveError(ESaveError::InvalidDocument, "Missing save field");
	}
	// 必須値の入力位置。
	const FJsonValue& Value = Json.Get(Index);
	if (Value.Kind != Kind)
	{
		throw FSaveError(ESaveError::InvalidDocument, "Save field type mismatch", Value.Line, Value.Column);
	}
	return Value;
}
// JSONの整数を元の十進字句から読む。f64の近似値は使わない。
uint32 Integer(const FJsonDocument& Json, const char* Key)
{
	// 既存readerが検証した整数候補。
	const FJsonValue& Value = Field(Json, Key, EJsonKind::Number);
	// 範囲検査前の厳密な値。
	uint64 Number = 0;
	try
	{
		Number = FJsonWriter::ParseUnsigned(Value.Text);
	}
	catch (const FException& Error)
	{
		throw FSaveError(ESaveError::InvalidDocument, Error.What(), Value.Line, Value.Column);
	}
	if (Number > TNumericLimits<uint32>::Max())
	{
		throw FSaveError(ESaveError::InvalidDocument, "Save integer overflow", Value.Line, Value.Column);
	}
	return static_cast<uint32>(Number);
}
// 数値やJSONの検証とは独立した封筒の契約。
void Validate(const FSaveDocument& Value, FSaveLimits Limits)
{
	if (Value.FormatVersion != 1)
	{
		throw FSaveError(ESaveError::UnsupportedVersion, "Unsupported save format version");
	}
	if (!IsKey(Value.GameId, Limits.MaxKeyBytes, false) || Value.GameVersion == 0 || Value.ContentRevision.Size() > Limits.MaxKeyBytes)
	{
		throw FSaveError(ESaveError::InvalidDocument, "Invalid game identity or payload version");
	}
	for (char C : Value.ContentRevision)
	{
		if (static_cast<uint8>(C) < 0x20)
		{
			throw FSaveError(ESaveError::InvalidDocument, "Invalid content revision");
		}
	}
	if (Value.Kind == ESaveDocumentKind::Progress)
	{
		if ((Value.Dimension != 2 && Value.Dimension != 3) || Value.ContentRevision.IsEmpty() || !IsKey(Value.SceneKey, Limits.MaxKeyBytes, true) || !IsKey(Value.CheckpointKey, Limits.MaxKeyBytes, true))
		{
			throw FSaveError(ESaveError::InvalidDocument, "Invalid checkpoint identity");
		}
	}
	else if (Value.Kind != ESaveDocumentKind::Settings || Value.Dimension != 0 || !Value.SceneKey.IsEmpty() || !Value.CheckpointKey.IsEmpty() || !Value.ContentRevision.IsEmpty())
	{
		throw FSaveError(ESaveError::InvalidDocument, "Invalid settings envelope");
	}
}
} // namespace
FString FSaveCodec::Encode(const FSaveDocument& Document, FSaveLimits Limits)
{
	// 読解と出力の共通上限。
	const FJsonLimits Bound = JsonLimits(Limits);
	Validate(Document, Limits);
	// ゲームの型付きcodecから受け取った純粋payload。
	const FJsonDocument Payload = FJsonDocument::Parse(Document.Payload, Bound);
	if (Payload.Get(0).Kind != EJsonKind::Object)
	{
		throw FSaveError(ESaveError::InvalidDocument, "Save payload must be an object");
	}
	// 固定のschemaだけを生成し、ゲームpayloadを任意fieldへ拡散しない。
	FString Bytes("{\"format\":");
	Bytes += FJsonWriter::Unsigned(Document.FormatVersion);
	Bytes += ",\"kind\":";
	Bytes += FJsonWriter::Quote(Document.Kind == ESaveDocumentKind::Progress ? "progress" : "settings");
	Bytes += ",\"gameId\":";
	Bytes += FJsonWriter::Quote(Document.GameId);
	Bytes += ",\"gameVersion\":";
	Bytes += FJsonWriter::Unsigned(Document.GameVersion);
	Bytes += ",\"contentRevision\":";
	Bytes += FJsonWriter::Quote(Document.ContentRevision);
	Bytes += ",\"scene\":";
	Bytes += FJsonWriter::Quote(Document.SceneKey);
	Bytes += ",\"dimension\":";
	Bytes += FJsonWriter::Unsigned(Document.Dimension);
	Bytes += ",\"checkpoint\":";
	Bytes += FJsonWriter::Quote(Document.CheckpointKey);
	Bytes += ",\"generation\":";
	Bytes += FJsonWriter::Quote(FJsonWriter::Unsigned(Document.Generation));
	Bytes += ",\"payload\":";
	Bytes += FJsonWriter::Write(Payload, 0, Limits.MaxBytes);
	Bytes += '}';
	// 封筒による深度・バイト増加も同じ制限で検査する。
	return FJsonWriter::Write(FJsonDocument::Parse(Bytes, Bound), 0, Limits.MaxBytes);
}
FSaveDocument FSaveCodec::Decode(FStringView Bytes, FSaveLimits Limits)
{
	try
	{
		// schemaの前に字句・重複・深度・総量を検査する。
		const FJsonDocument Json = FJsonDocument::Parse(Bytes, JsonLimits(Limits));
		if (Json.Get(0).Kind != EJsonKind::Object)
		{
			throw FSaveError(ESaveError::InvalidDocument, "Save envelope must be an object");
		}
		// バージョンを最初に確認し、未知バージョンのfieldを既定値で補わない。
		FSaveDocument Result;
		Result.FormatVersion = Integer(Json, "format");
		if (Result.FormatVersion != 1)
		{
			throw FSaveError(ESaveError::UnsupportedVersion, "Unsupported save format version");
		}
		for (int32 Child = Json.Get(0).FirstChild; Child >= 0; Child = Json.Get(Child).NextSibling)
		{
			// 許可fieldの固定schema。
			const FString& Key = Json.Get(Child).Key;
			if (Key != "format" && Key != "kind" && Key != "gameId" && Key != "gameVersion" && Key != "contentRevision" && Key != "scene" && Key != "dimension" && Key != "checkpoint" && Key != "generation" && Key != "payload")
			{
				throw FSaveError(ESaveError::InvalidDocument, "Unknown save field", Json.Get(Child).Line, Json.Get(Child).Column);
			}
		}
		// 進行と設定の混同を防ぐ識別。
		const FString& Kind = Field(Json, "kind", EJsonKind::String).Text;
		if (Kind != "progress" && Kind != "settings")
		{
			throw FSaveError(ESaveError::InvalidDocument, "Unknown save document kind");
		}
		Result.Kind = Kind == "progress" ? ESaveDocumentKind::Progress : ESaveDocumentKind::Settings;
		Result.GameId = Field(Json, "gameId", EJsonKind::String).Text;
		Result.GameVersion = Integer(Json, "gameVersion");
		Result.ContentRevision = Field(Json, "contentRevision", EJsonKind::String).Text;
		Result.SceneKey = Field(Json, "scene", EJsonKind::String).Text;
		Result.Dimension = Integer(Json, "dimension");
		Result.CheckpointKey = Field(Json, "checkpoint", EJsonKind::String).Text;
		try
		{
			Result.Generation = FJsonWriter::ParseUnsigned(Field(Json, "generation", EJsonKind::String).Text);
		}
		catch (const FSaveError&)
		{
			throw;
		}
		catch (const FException& Error)
		{
			throw FSaveError(ESaveError::InvalidDocument, Error.What());
		}
		Field(Json, "payload", EJsonKind::Object);
		Result.Payload = FJsonWriter::Write(Json, Json.Find(0, "payload"), Limits.MaxBytes);
		Validate(Result, Limits);
		return Result;
	}
	catch (const FJsonError& Error)
	{
		throw FSaveError(ESaveError::InvalidDocument, Error.What(), Error.GetLine(), Error.GetColumn());
	}
}
void FSaveCodec::RequireCompatible(const FSaveDocument& Document, const FSaveDocument& Expected)
{
	if (Document.FormatVersion != Expected.FormatVersion || Document.Kind != Expected.Kind || Document.GameId != Expected.GameId || Document.GameVersion != Expected.GameVersion || Document.ContentRevision != Expected.ContentRevision || Document.SceneKey != Expected.SceneKey || Document.Dimension != Expected.Dimension)
	{
		throw FSaveError(ESaveError::IncompatibleContent, "Save identity or content revision mismatch");
	}
}
} // namespace Dxf
