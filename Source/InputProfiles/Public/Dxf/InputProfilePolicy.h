// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_PROFILE_POLICY_H
#define DXF_INPUT_PROFILE_POLICY_H
#include "Dxf/InputActionBindings.h"
namespace Dxf
{
/**
 * ゲームごとの許可Action、予約入力、下げられる有限上限。大文字小文字は区別する。
 */
struct FInputProfilePolicy
{
	/**
	 * 完全プロファイルへ一度ずつ含めるASCII名。空集合も許す。
	 */
	Toolbox::TVector<Toolbox::FString> AllowedActions;
	/**
	 * メニューや取消し用にゲーム割当から除外する入力。
	 */
	Toolbox::TVector<FInputBindingDescription> ReservedBindings;
	/**
	 * Action数の上限。1〜128。
	 */
	Toolbox::size_t MaxActions = 128;
	/**
	 * 割当総数の上限。1〜512。
	 */
	Toolbox::size_t MaxBindings = 512;
	/**
	 * 一つのActionの割当数上限。1〜64。
	 */
	Toolbox::size_t MaxBindingsPerAction = 64;
	/**
	 * 文書全体の上限。1〜256KiB。
	 */
	Toolbox::size_t MaxBytes = 256 * 1024;
};
} // namespace Dxf
#endif
