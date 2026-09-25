// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_NAVIGATION_BINDINGS_H
#define DXF_UI_NAVIGATION_BINDINGS_H
#include "Dxf/InputTypes.h"
#include "Dxf/UiEvents.h"
#include "Toolbox/Array.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * キーの割当に付けるShiftの条件（Tab と Shift+Tab の区別に使う）。
 */
enum class EUiShiftCondition : Toolbox::uint8
{
	/**
	 * Shiftに依らない。
	 */
	Any,
	/**
	 * Shiftを押しているときだけ。
	 */
	Shift,
	/**
	 * Shiftを押していないときだけ。
	 */
	NoShift
};

/**
 * キー一つの割当。
 */
struct FUiKeyBinding
{
	/**
	 * キー。
	 */
	EKey Key = EKey::Count;
	/**
	 * Shiftの条件。
	 */
	EUiShiftCondition Shift = EUiShiftCondition::Any;
};

/**
 * UIの操作（決定・戻る・Tab・方向）への物理入力の割当。コントロールはボタン番号を持たず、この表から意味を受け取る。
 * UIが受けた操作の物理入力は、離すまでゲームへ届けない（どのキーを遮るかもこの表で決まる）。
 */
struct FUiNavigationBindings
{
	/**
	 * 操作ごとのキー。
	 */
	Toolbox::TArray<Toolbox::TVector<FUiKeyBinding>, static_cast<Toolbox::size_t>(EUiNavigationCommand::Count)> Keys;
	/**
	 * 操作ごとのゲームパッドのボタン番号（0〜15）。
	 */
	Toolbox::TArray<Toolbox::TVector<Toolbox::size_t>, static_cast<Toolbox::size_t>(EUiNavigationCommand::Count)> PadButtons;
	/**
	 * 左スティックを方向の操作に使うか。
	 */
	bool bStickNavigation = true;
	/**
	 * スティックを方向とみなす傾き（0〜1）。
	 */
	Toolbox::f32 StickThreshold = 0.5f;
	/**
	 * 既定の割当: Tab／Shift+Tab、矢印キー、Enter・Spaceで決定、Escape・Backspaceで戻る。
	 * パッドはボタン0で決定、1で戻る、5・4でTab・Shift+Tab、左スティックで方向。
	 */
	static FUiNavigationBindings MakeDefault();
};

/**
 * 入力プレイヤーが使う機器。
 */
struct FUiPlayerDevices
{
	/**
	 * キーボードを使うか。
	 */
	bool bKeyboard = true;
	/**
	 * ゲームパッドの番号（負は使わない）。
	 */
	Toolbox::int32 Pad = 0;
};
} // namespace Dxf
#endif
