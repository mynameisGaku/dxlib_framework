// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_FILEERROR_H
#define TOOLBOX_FILEERROR_H
#include "Toolbox/EFileOperation.h"
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 確保を伴わず、OSの原因と失敗した工程を保持する。
 */
class FFileError : public FException
{
public:
	/**
	 * @param Operation 失敗した工程。
	 * @param Code OSが返したエラー番号。
	 * @param Reason 確保せず保持できる、寿命の長い原因文字列。
	 */
	FFileError(EFileOperation Operation, uint32 Code, const char* Reason = "File operation failed") noexcept : FException(Reason), m_Operation(Operation), m_Code(Code)
	{
	}
	/**
	 * 失敗した工程を返す。
	 */
	EFileOperation GetOperation() const noexcept
	{
		return m_Operation;
	}
	/**
	 * 記録したOS番号を返す。
	 */
	uint32 GetCode() const noexcept
	{
		return m_Code;
	}

private:
	/**
	 * 失敗した工程。
	 */
	EFileOperation m_Operation;
	/**
	 * OSの原因番号。
	 */
	uint32 m_Code;
};
} // namespace Toolbox
#endif
