// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVEERROR_H
#define DXF_SAVEERROR_H
#include "Dxf/ESaveError.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 保存の原因と入力位置を確保なしで保持する。呼出し側がslotやパスを補う。
 */
class FSaveError : public Toolbox::FException
{
public:
	/**
	 * @param Code 原因区分。@param Reason 説明。@param Line 入力行。@param Column 入力列。
	 */
	FSaveError(ESaveError Code, const char* Reason, Toolbox::uint32 Line = 0, Toolbox::uint32 Column = 0) noexcept : FException(Reason), m_Code(Code), m_Line(Line), m_Column(Column)
	{
	}
	/**
	 * 原因区分を返す。
	 */
	ESaveError GetCode() const noexcept
	{
		return m_Code;
	}
	/**
	 * 入力位置の行を返す。
	 */
	Toolbox::uint32 GetLine() const noexcept
	{
		return m_Line;
	}
	/**
	 * 入力位置の列を返す。
	 */
	Toolbox::uint32 GetColumn() const noexcept
	{
		return m_Column;
	}

private:
	/**
	 * 採用を止めた原因。
	 */
	ESaveError m_Code;
	/**
	 * 最初の入力行。
	 */
	Toolbox::uint32 m_Line;
	/**
	 * 最初の入力列。
	 */
	Toolbox::uint32 m_Column;
};
} // namespace Dxf
#endif
