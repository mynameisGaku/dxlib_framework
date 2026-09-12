#pragma once
#include "Dxf/Input.h"
namespace Dxf
{
/**
 * DxLibによるデバイス入力の取得を管理する型。
 */
class FDxLibInputSource final : public IInputSource
{
public:
	/**
	 * 現在のデバイス入力を取得する。
	 */
	TResult<FRawInput> Poll() override;
};
} // namespace Dxf
