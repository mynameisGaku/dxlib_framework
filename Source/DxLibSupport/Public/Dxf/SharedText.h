// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SHARED_TEXT_H
#define DXF_SHARED_TEXT_H
#include "Toolbox/SharedPtr.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 変更しないUTF-8文字列の共有の所有。複製は参照数の増加だけで、文字列を確保しない。
 * 遅延描画の命令は受付時点の文字列を所有し続ける（元の要素が後で文字を変えても、受付済みの命令は変わらない）。
 * FStringや文字列リテラルから暗黙に作れる（その時だけ一度確保する）。
 */
class FSharedText
{
public:
	/**
	 * 空の文字列。
	 */
	FSharedText() = default;
	/**
	 * 文字列を共有の所有へ移す。
	 * @param Text 文字列。
	 */
	FSharedText(Toolbox::FString Text) : m_pText(Toolbox::MakeShared<const Toolbox::FString>(Toolbox::Move(Text)))
	{
	}
	/**
	 * 文字列リテラル等から作る。
	 * @param Text NUL終端のUTF-8文字列。
	 */
	FSharedText(const char* Text) : FSharedText(Toolbox::FString(Text))
	{
	}
	/**
	 * 文字列（空なら空の文字列）。
	 */
	FORCEINLINE const Toolbox::FString& Get() const noexcept
	{
		return m_pText ? *m_pText : GetEmpty_Internal();
	}
	/**
	 * NUL終端の文字列。
	 */
	FORCEINLINE const char* CStr() const noexcept
	{
		return Get().CStr();
	}
	/**
	 * 空か。
	 */
	FORCEINLINE bool IsEmpty() const noexcept
	{
		return Get().IsEmpty();
	}
	/**
	 * 同じ所有を共有しているか（内容の比較ではない）。
	 * @param Other 比べる文字列。
	 */
	FORCEINLINE bool IsSameShare(const FSharedText& Other) const noexcept
	{
		return m_pText.Get() == Other.m_pText.Get();
	}
	/**
	 * 内容が同じか。
	 */
	FORCEINLINE bool operator==(const FSharedText& Other) const noexcept
	{
		return IsSameShare(Other) || Get() == Other.Get();
	}
	/**
	 * 内容が同じか。
	 */
	FORCEINLINE bool operator==(const Toolbox::FString& Other) const noexcept
	{
		return Get() == Other;
	}
	/**
	 * 内容が同じか。
	 */
	FORCEINLINE bool operator==(const char* Other) const noexcept
	{
		return Get() == Other;
	}

private:
	/**
	 * 空の文字列の共通の実体。
	 */
	static const Toolbox::FString& GetEmpty_Internal() noexcept
	{
		static const Toolbox::FString Empty;
		return Empty;
	}
	/**
	 * 共有の文字列（空ならnull）。
	 */
	Toolbox::TSharedPtr<const Toolbox::FString> m_pText;
};
} // namespace Dxf
#endif
