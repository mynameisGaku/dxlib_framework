// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_ACTION_BINDINGS_H
#define DXF_INPUT_ACTION_BINDINGS_H
#include "Dxf/InputBindingDescription.h"
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 一つのAction名と完全な割当。空の配列は明示した未割当。
 */
struct FInputActionBindings
{
	/**
	 * ゲームが許可カタログで宣言する名前。
	 */
	Toolbox::FString Action;
	/**
	 * ORで保持を判断する入力。意味のない同一入力の重複は拒否する。
	 */
	Toolbox::TVector<FInputBindingDescription> Bindings;
};
} // namespace Dxf
#endif
