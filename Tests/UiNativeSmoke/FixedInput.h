// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_UI_FIXED_INPUT_H
#define DXF_TEST_UI_FIXED_INPUT_H
#include "Dxf/InputSource.h"
namespace Dxf::UiSmoke
{
/**
 * OS入力以外の処理は実Applicationへ任せる。人のマウス操作の代替検証とはしない。
 */
class FFixedInput final : public IInputSource
{
public:
	FRawInput Raw;
	TResult<FRawInput> Poll() override
	{
		return TResult<FRawInput>::Success(Raw);
	}
};
} // namespace Dxf::UiSmoke
#endif
