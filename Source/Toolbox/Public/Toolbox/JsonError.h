// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_JSON_ERROR_H
#define TOOLBOX_JSON_ERROR_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * JSON読み取りの最初の原因と入力位置。ファイルのパスは呼び出し側が補う。
 */
class FJsonError : public FException
{
public:
	/**
	 * @param Reason 固定長の例外へ保存する原因。
	 * @param Line 1始まりの行。
	 * @param Column 1始まりのUTF-8バイト列。
	 */
	FJsonError(const char* Reason, uint32 Line, uint32 Column) noexcept
	    : FException(Reason), m_Line(Line), m_Column(Column)
	{
	}
	/**
	 * エラーを検出した行を返す。
	 */
	uint32 GetLine() const noexcept
	{
		return m_Line;
	}
	/**
	 * エラーを検出したバイト列を返す。
	 */
	uint32 GetColumn() const noexcept
	{
		return m_Column;
	}

private:
	/**
	 * 最初の異常の行。
	 */
	uint32 m_Line;
	/**
	 * 最初の異常の列。
	 */
	uint32 m_Column;
};
} // namespace Toolbox
#endif
