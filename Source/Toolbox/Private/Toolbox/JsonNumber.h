// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_JSON_NUMBER_PRIVATE_H
#define TOOLBOX_JSON_NUMBER_PRIVATE_H
#include "Toolbox/Utility.h"
#include <locale.h>
namespace Toolbox::JsonPrivate
{
/**
 * JSON数値の読解と出力だけに使うC locale。プロセス全体の設定を変えない。
 */
class FNumberLocale
{
public:
	/**
	 * 専用localeを取得する。取得失敗は例外。
	 */
	FNumberLocale();
	/**
	 * 専用localeを解放する。
	 */
	~FNumberLocale() noexcept;
	/**
	 * localeの所有権を複製しない。
	 */
	FNumberLocale(const FNumberLocale&) = delete;
	/**
	 * localeの所有権を複製しない。
	 */
	FNumberLocale& operator=(const FNumberLocale&) = delete;
	/**
	 * @param Text 字句検証済みのNUL終端数値。
	 */
	f64 Convert(const char* Text) const noexcept;
	/**
	 * @param Buffer 出力先。@param Capacity バイト容量。@param Value 有限な数値。
	 * 17桁の表現を生成し、出力長を返す。失敗・切捨ては例外。
	 */
	int32 Format(char* Buffer, size_t Capacity, f64 Value) const;

private:
#ifdef _WIN32
	/**
	 * Windows CRTが所有を移す数値locale。
	 */
	_locale_t m_Locale = nullptr;
#else
	/**
	 * POSIXが所有を移す数値locale。
	 */
	locale_t m_Locale = nullptr;
#endif
};
} // namespace Toolbox::JsonPrivate
#endif
