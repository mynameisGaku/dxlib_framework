// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_BINDING_DEVICE_H
#define DXF_INPUT_BINDING_DEVICE_H
namespace Dxf
{
/**
 * 割当の装置。保存には明示した文字列名を使い、enum値を固定しない。
 */
enum class EInputBindingDevice
{
	/**
	 * Snapshotのキーボード。
	 */
	Keyboard,
	/**
	 * Snapshotのマウスボタン。
	 */
	Mouse,
	/**
	 * Snapshotの最大4台のPad。
	 */
	Gamepad
};
} // namespace Dxf
#endif
