// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_PROFILE_H
#define DXF_INPUT_PROFILE_H
#include "Dxf/InputActionBindings.h"
namespace Dxf
{
/**
 * 押下履歴を持たない完全な入力割当。空の割当も明示的な未割当として保持する。
 */
struct FInputProfile
{
	/**
	 * 入力プロファイルのバージョン。現在は1のみ。
	 */
	Toolbox::uint32 ProfileVersion = 1;
	/**
	 * ゲームが許可したすべてのActionと割当。
	 */
	Toolbox::TVector<FInputActionBindings> Actions;
};
} // namespace Dxf
#endif
