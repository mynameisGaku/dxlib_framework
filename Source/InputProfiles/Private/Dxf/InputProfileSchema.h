// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_PROFILE_SCHEMA_H
#define DXF_INPUT_PROFILE_SCHEMA_H
#include "Dxf/SaveError.h"
#include "Dxf/InputProfilePolicy.h"
#include "Toolbox/JsonWriter.h"
namespace Dxf::InputProfileSchema
{
/**
 * @param Condition 必要条件。
 * @param Reason 拒否理由。条件を満たさなければ値の不正を通知する。
 */
inline void Require(bool Condition, const char* Reason)
{
	if (!Condition)
	{
		throw FSaveError(ESaveError::InvalidDocument, Reason);
	}
}
/**
 * @param Policy ゲーム側の上限。下げる操作だけを許す。
 */
inline void Bounds(const FInputProfilePolicy& Policy)
{
	Require(Policy.MaxActions > 0 && Policy.MaxActions <= 128 && Policy.MaxBindings > 0 && Policy.MaxBindings <= 512 && Policy.MaxBindingsPerAction > 0 && Policy.MaxBindingsPerAction <= 64 && Policy.MaxBytes > 0 && Policy.MaxBytes <= 256 * 1024, "Invalid input profile limits");
}
/**
 * @param Policy 有限上限。JSON共通処理の深度・文字列・値数も制限する。
 */
inline Toolbox::FJsonLimits Limits(const FInputProfilePolicy& Policy)
{
	Bounds(Policy);
	Toolbox::FJsonLimits Result;
	Result.MaxBytes = Policy.MaxBytes;
	Result.MaxDepth = 8;
	Result.MaxStringBytes = 128;
	Result.MaxValues = 4096;
	return Result;
}
/**
 * @param Json 値集合。
 * @param Index オブジェクト。
 * @param Allowed 許可名を区切った集合。
 */
inline void Fields(const Toolbox::FJsonDocument& Json, Toolbox::int32 Index, Toolbox::FStringView Allowed)
{
	Require(Json.Get(Index).Kind == Toolbox::EJsonKind::Object, "Input profile object expected");
	for (Toolbox::int32 Child = Json.Get(Index).FirstChild; Child >= 0; Child = Json.Get(Child).NextSibling)
	{
		// 許可集合へ正しく対応したか。
		bool Found = false;
		Toolbox::size_t Start = 0;
		for (Toolbox::size_t Offset = 0; Offset <= Allowed.Size(); ++Offset)
		{
			if (Offset == Allowed.Size() || Allowed[Offset] == '|')
			{
				Found = Found || Json.Get(Child).Key == Toolbox::FStringView(Allowed.Data() + Start, Offset - Start);
				Start = Offset + 1;
			}
		}
		Require(Found, "Unknown input profile field");
	}
}
/**
 * @param Json 値集合。
 * @param Parent 親Object。
 * @param Name 必須名。
 * @param Kind 必須型。
 */
inline Toolbox::int32 Field(const Toolbox::FJsonDocument& Json, Toolbox::int32 Parent, const char* Name, Toolbox::EJsonKind Kind)
{
	// 対象集合の現在位置。
	const Toolbox::int32 Index = Json.Find(Parent, Name);
	Require(Index >= 0 && Json.Get(Index).Kind == Kind, "Missing or wrongly typed input profile field");
	return Index;
}
/**
 * @param Json 値集合。
 * @param Parent 親Object。
 * @param Name 必須文字列名。
 */
inline const Toolbox::FString& Text(const Toolbox::FJsonDocument& Json, Toolbox::int32 Parent, const char* Name)
{
	return Json.Get(Field(Json, Parent, Name, Toolbox::EJsonKind::String)).Text;
}
/**
 * @param Json 値集合。
 * @param Parent 親Object。
 * @param Name 必須整数名。小数や指数表記は拒否する。
 */
inline Toolbox::uint64 Integer(const Toolbox::FJsonDocument& Json, Toolbox::int32 Parent, const char* Name)
{
	return Toolbox::FJsonWriter::ParseUnsigned(Json.Get(Field(Json, Parent, Name, Toolbox::EJsonKind::Number)).Text);
}
/**
 * @param Value 保存から得たバージョン。1以外は推測せず拒否する。
 */
inline void Version(Toolbox::uint64 Value)
{
	if (Value != 1)
	{
		throw FSaveError(ESaveError::UnsupportedVersion, "Unsupported input/settings version");
	}
}
} // namespace Dxf::InputProfileSchema
#endif
