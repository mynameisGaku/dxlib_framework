// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_INPUT_H
#define DXF_UI_INPUT_H
#include "Dxf/UiEvents.h"
#include "Toolbox/Array.h"
namespace Dxf
{
/**
 * ルートの表示面に対するポインターの一フレームの状態。OSの入力は読まず、仲介側が入力の事実から作る。
 */
struct FUiPointerFrame
{
	/**
	 * ポインターがこのルートの表示面の上にあるか（面の外・他のルートが前面なら偽）。
	 */
	bool bPresent = false;
	/**
	 * ルートの論理座標の位置（bPresentでなくても、キャプチャ中はこの位置を使う）。
	 */
	FVector2 Position;
	/**
	 * 押されているボタン。
	 */
	Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EUiPointerButton::Count)> Down{};
	/**
	 * このフレームで押したボタン。
	 */
	Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EUiPointerButton::Count)> Pressed{};
	/**
	 * このフレームで離したボタン。
	 */
	Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EUiPointerButton::Count)> Released{};
	/**
	 * ホイールの回転（ノッチ数。正は手前＝下方向へのスクロール）。
	 */
	Toolbox::f32 WheelNotches = 0;
};

/**
 * ルートへ届けるキーボード・パッドの操作の一フレームの状態（割当済みの意味）。
 */
struct FUiNavigationFrame
{
	/**
	 * このルートが操作を受けるか（入力プレイヤーの割当に一致するか）。
	 */
	bool bActive = false;
	/**
	 * 押されている操作。
	 */
	Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EUiNavigationCommand::Count)> Down{};
	/**
	 * このフレームで押した操作。
	 */
	Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EUiNavigationCommand::Count)> Pressed{};
};

/**
 * ルートの入力の一フレーム。
 */
struct FUiInputFrame
{
	/**
	 * ポインター。
	 */
	FUiPointerFrame Pointer;
	/**
	 * キーボード・パッド。
	 */
	FUiNavigationFrame Navigation;
	/**
	 * 前回からの秒数（繰り返し・遅延に使う。Sceneのポーズに依らない）。
	 */
	Toolbox::f64 DeltaSeconds = 0;
};

/**
 * 入力を処理した結果。ゲーム側への配送の判断に使う。
 */
struct FUiInputResult
{
	/**
	 * ポインターがUIの受ける要素の上にある（またはキャプチャ中）。
	 */
	bool bPointerOverUi = false;
	/**
	 * このフレームで押し始めたボタンをUIが受けた（離すまでUIの操作）。
	 */
	Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EUiPointerButton::Count)> ButtonsClaimed{};
	/**
	 * ホイールをUIが処理した。
	 */
	bool bWheelConsumed = false;
	/**
	 * キーボード・パッドの操作をUIが処理した（フォーカスがある・Modal中）。
	 */
	bool bNavigationConsumed = false;
	/**
	 * Modalが開いている（背後の操作とゲームへの入力を遮る）。
	 */
	bool bModal = false;
	/**
	 * フォーカスのある要素がある（キーボード・パッドの操作をUIが受ける状態）。
	 */
	bool bHasFocus = false;
};

/**
 * 方向入力の繰り返しの設定。
 */
struct FUiNavigationSettings
{
	/**
	 * 押し続けてから最初の繰り返しまでの秒数。
	 */
	Toolbox::f64 InitialRepeatDelay = 0.4;
	/**
	 * 繰り返しの間隔（秒）。
	 */
	Toolbox::f64 RepeatInterval = 0.08;
	/**
	 * ツールチップを出すまでの秒数（ポインターが同じ要素の上に留まった時間）。
	 */
	Toolbox::f64 TooltipDelay = 0.5;
	/**
	 * ドラッグとみなす移動量（論理単位）。
	 */
	Toolbox::f32 DragThreshold = 4;
};
} // namespace Dxf
#endif
