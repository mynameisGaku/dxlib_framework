// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UserSettingsCodec.h"
#include "Dxf/InputProfileCodec.h"
#include "Dxf/InputProfileSchema.h"
#include "Dxf/SaveCodec.h"
namespace Dxf
{
using namespace Toolbox;
using namespace InputProfileSchema;
FSaveDocument FUserSettingsCodec::Encode(const FUserSettings& Settings, const FInputProfilePolicy& Policy, FString GameId, uint32 GameVersion)
{
	// 設定の全値を検査してから、不変のSettings文書を作る。
	Version(Settings.SettingsVersion);
	Require(IsFinite(Settings.Volume) && Settings.Volume >= 0 && Settings.Volume <= 1 && IsFinite(Settings.UiScale) && Settings.UiScale >= 0.25f && Settings.UiScale <= 8.0f, "Invalid volume or UI scale");
	// 検証後に返す独立した候補値。
	FSaveDocument Result;
	Result.Kind = ESaveDocumentKind::Settings;
	Result.GameId = Move(GameId);
	Result.GameVersion = GameVersion;
	Result.Dimension = 0;
	// 各小さい項目の長さを合計し、出力領域を拡張する前に上限を確認する。
	const FString Input = FInputProfileCodec::Encode(Settings.Input, Policy);
	// 描画とhit testへ共通に使うUI倍率。
	const FString Scale = FJsonWriter::Number(Settings.UiScale);
	// 元の音量へ掛ける有限倍率。
	const FString Volume = FJsonWriter::Number(Settings.Volume);
	// 設定Objectの開始表現。
	const FString Prefix = "{\"input\":";
	// 設定バージョンとUI倍率の区切り。
	const FString Middle = ",\"settingsVersion\":1,\"uiScale\":";
	// 音量項目の区切り。
	const FString Last = ",\"volume\":";
	// 上限判定に使う付加バイト数、または拒否する追加項目の試験表現。
	const size_t Extra = Prefix.Size() + Middle.Size() + Last.Size() + Scale.Size() + Volume.Size() + 1;
	Require(Extra <= Policy.MaxBytes && Input.Size() <= Policy.MaxBytes - Extra, "Settings output byte limit");
	Result.Payload = Prefix + Input + Middle + Scale + Last + Volume + "}";
	(void)FSaveCodec::Encode(Result);
	return Result;
}
FUserSettings FUserSettingsCodec::Decode(const FSaveDocument& Document, const FInputProfilePolicy& Policy, FString GameId, uint32 GameVersion)
{
	// 進行文書を設定として受け取らず、ゲーム識別の後でpayloadを検証する。
	Bounds(Policy);
	Require(Document.Payload.Size() <= Policy.MaxBytes, "Settings input byte limit");
	// 信頼するゲームと設定の互換条件。
	FSaveDocument Expected;
	Expected.Kind = ESaveDocumentKind::Settings;
	Expected.Dimension = 0;
	Expected.GameId = Move(GameId);
	Expected.GameVersion = GameVersion;
	(void)FSaveCodec::Encode(Document);
	// ゲーム識別を確認した上で、将来のゲーム側バージョンを推測せず拒否する。
	FSaveDocument Identity = Expected;
	Identity.GameVersion = Document.GameVersion;
	FSaveCodec::RequireCompatible(Document, Identity);
	if (Document.GameVersion > GameVersion)
	{
		throw FSaveError(ESaveError::UnsupportedVersion, "Future settings game version");
	}
	FSaveCodec::RequireCompatible(Document, Expected);
	// 全入力の文法を検証した不変のJSON値集合。
	const FJsonDocument Json = FJsonDocument::Parse(Document.Payload, Limits(Policy));
	Version(Integer(Json, 0, "settingsVersion"));
	Fields(Json, 0, "input|settingsVersion|uiScale|volume");
	// 検証後に返す独立した候補値。
	FUserSettings Result;
	Result.Input = FInputProfileCodec::Decode(FJsonWriter::Write(Json, Field(Json, 0, "input", EJsonKind::Object), Policy.MaxBytes), Policy);
	// 元の音量へ掛ける有限倍率。
	const f64 Volume = Json.Get(Field(Json, 0, "volume", EJsonKind::Number)).Number;
	// 描画とhit testへ共通に使うUI倍率。
	const f64 Scale = Json.Get(Field(Json, 0, "uiScale", EJsonKind::Number)).Number;
	Require(Volume >= 0 && Volume <= 1 && Scale >= 0.25 && Scale <= 8, "Invalid stored volume or UI scale");
	Result.Volume = static_cast<f32>(Volume);
	Result.UiScale = static_cast<f32>(Scale);
	return Result;
}
} // namespace Dxf
