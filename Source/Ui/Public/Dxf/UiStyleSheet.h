// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_STYLE_SHEET_H
#define DXF_UI_STYLE_SHEET_H
#include "Dxf/Result.h"
#include "Dxf/UiStyle.h"
#include "Toolbox/Map.h"
namespace Dxf
{
/**
 * 共通トークンの値（色または数値）。
 */
struct FUiToken
{
	/**
	 * 色か（falseなら数値）。
	 */
	bool bColor = false;
	/**
	 * 色の値。
	 */
	FColor Color;
	/**
	 * 数値の値。
	 */
	Toolbox::f32 Number = 0;
};

/**
 * スタイルIDから見た目の定義を引く表と、共通トークン。
 * 作成後は変更せず、再読込では新しい表へ丸ごと差し替える（一部の部品だけ新値になる状態を作らない）。
 */
class FUiStyleSheet
{
public:
	/**
	 * スタイルIDの定義を登録する。同じIDの重複は失敗する。
	 * @param Id スタイルID（空は不可）。
	 * @param Set 定義。
	 */
	TResult<void> AddStyle(const Toolbox::FString& Id, FUiStyleSet Set);
	/**
	 * トークンを登録する。同じ名前の重複は失敗する。
	 * @param Name トークン名。
	 * @param Token 値。
	 */
	TResult<void> AddToken(const Toolbox::FString& Name, FUiToken Token);
	/**
	 * スタイルIDの定義（なければnullptr）。
	 * @param Id スタイルID。
	 */
	const FUiStyleSet* Find(const Toolbox::FString& Id) const noexcept;
	/**
	 * トークン（なければnullptr）。
	 * @param Name トークン名。
	 */
	const FUiToken* FindToken(const Toolbox::FString& Name) const noexcept;
	/**
	 * 色のトークン（なければ既定値）。C++側から共通の色を使うときの入口。
	 * @param Name トークン名。
	 * @param Fallback 既定値。
	 */
	FColor GetColorToken(const Toolbox::FString& Name, FColor Fallback) const noexcept;
	/**
	 * 数値のトークン（なければ既定値）。
	 * @param Name トークン名。
	 * @param Fallback 既定値。
	 */
	Toolbox::f32 GetNumberToken(const Toolbox::FString& Name, Toolbox::f32 Fallback) const noexcept;
	/**
	 * 定義の数。
	 */
	FORCEINLINE Toolbox::size_t GetStyleCount() const noexcept
	{
		return m_Styles.Size();
	}
	/**
	 * トークンの数。
	 */
	FORCEINLINE Toolbox::size_t GetTokenCount() const noexcept
	{
		return m_Tokens.Size();
	}
	/**
	 * スタイルIDの一覧（登録順）。
	 */
	Toolbox::TVector<Toolbox::FString> GetStyleIds() const;
	/**
	 * スタイルIDのない要素に使う既定の定義。
	 */
	FORCEINLINE const FUiStyleSet& GetDefault() const noexcept
	{
		return m_Default;
	}
	/**
	 * 既定の定義を設定する。
	 * @param Set 定義。
	 */
	FORCEINLINE void SetDefault(FUiStyleSet Set)
	{
		m_Default = Toolbox::Move(Set);
	}

private:
	/**
	 * スタイルIDの定義。
	 */
	Toolbox::TMap<Toolbox::FString, FUiStyleSet> m_Styles;
	/**
	 * トークン。
	 */
	Toolbox::TMap<Toolbox::FString, FUiToken> m_Tokens;
	/**
	 * 既定の定義。
	 */
	FUiStyleSet m_Default;
};
} // namespace Dxf
#endif
