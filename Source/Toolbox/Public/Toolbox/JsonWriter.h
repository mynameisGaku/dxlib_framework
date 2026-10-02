// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_JSON_WRITER_H
#define TOOLBOX_JSON_WRITER_H
#include "Toolbox/JsonDocument.h"
namespace Toolbox
{
/**
 * 検証済みJSONを安定した順序で書く。配列順と数値の厳密な元表記を保持する。
 */
class FJsonWriter
{
public:
	/**
	 * @param Document 既存の厳格な読解器で検証した値。
	 * @param Index 出力する値。既定は文書全体。
	 * @param MaxBytes 出力の有限上限。超過時は例外。
	 */
	static FString Write(const FJsonDocument& Document, int32 Index = 0, size_t MaxBytes = 8 * 1024 * 1024);
	/**
	 * @param Text 復号済みUTF-8。制御文字と引用符をescapeする。不正UTF-8は例外。
	 */
	static FString Quote(FStringView Text);
	/**
	 * @param Value 浮動小数点を経由させない厳密な符号なし整数。
	 */
	static FString Unsigned(uint64 Value);
	/**
	 * @param Text 符号・先頭ゼロなしの十進整数。0だけは許可し、overflowは例外。
	 */
	static uint64 ParseUnsigned(FStringView Text);
};
} // namespace Toolbox
#endif
