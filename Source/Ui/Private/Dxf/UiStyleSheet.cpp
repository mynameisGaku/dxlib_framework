// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiStyleSheet.h"
namespace Dxf
{
// スタイルIDの定義を登録する。
TResult<void> FUiStyleSheet::AddStyle(const Toolbox::FString& Id, FUiStyleSet Set)
{
	if (Id.IsEmpty())
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Empty UI style id");
	}
	if (m_Styles.Find(Id) != m_Styles.End())
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, Toolbox::FString("Duplicate UI style id: ") + Id);
	}
	m_Styles[Id] = Toolbox::Move(Set);
	return {};
}
// トークンを登録する。
TResult<void> FUiStyleSheet::AddToken(const Toolbox::FString& Name, FUiToken Token)
{
	if (Name.IsEmpty())
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Empty UI token name");
	}
	if (m_Tokens.Find(Name) != m_Tokens.End())
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, Toolbox::FString("Duplicate UI token: ") + Name);
	}
	m_Tokens[Name] = Token;
	return {};
}
// スタイルIDの定義を引く。
const FUiStyleSet* FUiStyleSheet::Find(const Toolbox::FString& Id) const noexcept
{
	const auto It = m_Styles.Find(Id);
	return It == m_Styles.End() ? nullptr : &It->Second;
}
// トークンを引く。
const FUiToken* FUiStyleSheet::FindToken(const Toolbox::FString& Name) const noexcept
{
	const auto It = m_Tokens.Find(Name);
	return It == m_Tokens.End() ? nullptr : &It->Second;
}
// 色のトークン。
FColor FUiStyleSheet::GetColorToken(const Toolbox::FString& Name, FColor Fallback) const noexcept
{
	const FUiToken* Token = FindToken(Name);
	return Token != nullptr && Token->bColor ? Token->Color : Fallback;
}
// 数値のトークン。
Toolbox::f32 FUiStyleSheet::GetNumberToken(const Toolbox::FString& Name, Toolbox::f32 Fallback) const noexcept
{
	const FUiToken* Token = FindToken(Name);
	return Token != nullptr && !Token->bColor ? Token->Number : Fallback;
}
// スタイルIDの一覧。
Toolbox::TVector<Toolbox::FString> FUiStyleSheet::GetStyleIds() const
{
	Toolbox::TVector<Toolbox::FString> Ids;
	Ids.Reserve(m_Styles.Size());
	for (const auto& Entry : m_Styles)
	{
		Ids.PushBack(Entry.First);
	}
	return Ids;
}
} // namespace Dxf
