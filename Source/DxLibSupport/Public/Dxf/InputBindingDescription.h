// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_BINDING_DESCRIPTION_H
#define DXF_INPUT_BINDING_DESCRIPTION_H
#include "Dxf/InputBindingDevice.h"
#include "Dxf/InputTypes.h"
namespace Dxf
{
/**
 * 保存可能な一つの割当。押下履歴やOSハンドルは持たない。
 */
struct FInputBindingDescription
{
	/**
	 * 以下のどの入力値を使うか。
	 */
	EInputBindingDevice Device = EInputBindingDevice::Keyboard;
	/**
	 * Keyboardだけが使う型付きキー。
	 */
	EKey Key = EKey::A;
	/**
	 * Mouseだけが使う型付きボタン。
	 */
	EMouseButton MouseButton = EMouseButton::Left;
	/**
	 * Gamepadだけが使うslot、0〜3。
	 */
	Toolbox::uint32 PadSlot = 0;
	/**
	 * Gamepadだけが使うSnapshotボタン、0〜15。
	 */
	Toolbox::uint32 PadButton = 0;
	/**
	 * 使用する装置の値だけを比較する。未使用fieldの差は入力の差ではない。
	 */
	bool operator==(const FInputBindingDescription& Other) const noexcept;
};
} // namespace Dxf
#endif
