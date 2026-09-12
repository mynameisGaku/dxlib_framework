#pragma once
#include "Dxf/InputTypes.h"
#include "Dxf/Result.h"

namespace Dxf
{
/**
 * 入力デバイスの読み取り窓口を管理する型。
 */
class IInputSource
{
public:
	/**
	 * 派生型を含むオブジェクトを安全に終了する。
	 */
	virtual ~IInputSource() = default;
	/**
	 * 現在のデバイス入力を取得する。
	 */
	virtual TResult<FRawInput> Poll() = 0;
};
} // namespace Dxf
