// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_EVENTS_H
#define DXF_UI_EVENTS_H
#include "Dxf/UiTypes.h"
namespace Dxf
{
class FUiRoot;

/**
 * ポインターのボタン。
 */
enum class EUiPointerButton : Toolbox::uint8
{
	/**
	 * 主ボタン（左）。
	 */
	Primary,
	/**
	 * 副ボタン（右）。
	 */
	Secondary,
	/**
	 * 中ボタン。
	 */
	Middle,
	/**
	 * ボタンの数。
	 */
	Count
};

/**
 * ポインターの出来事の種類。
 */
enum class EUiPointerEventType : Toolbox::uint8
{
	/**
	 * 要素の上に入った（泡立たない）。
	 */
	Enter,
	/**
	 * 要素の上から出た（泡立たない）。
	 */
	Leave,
	/**
	 * ボタンを押した。
	 */
	Down,
	/**
	 * ボタンを離した（キャプチャ中はキャプチャした要素へ届く）。
	 */
	Up,
	/**
	 * 位置が変わった（キャプチャ中はキャプチャした要素へ届く）。
	 */
	Move,
	/**
	 * ホイールを回した（処理した量を差し引いて親へ泡立つ）。
	 */
	Wheel,
	/**
	 * キャプチャを失った（非表示・切断・無効化・取り消し）。以後この操作での決定はしない。
	 */
	CaptureLost
};

/**
 * ポインターの出来事。対象から親へ順に届く（Enter・Leave・CaptureLostは対象だけ）。
 */
struct FUiPointerEvent
{
	/**
	 * 種類。
	 */
	EUiPointerEventType Type = EUiPointerEventType::Move;
	/**
	 * ルートの論理座標の位置。
	 */
	FVector2 Position;
	/**
	 * 押した・離したボタン。
	 */
	EUiPointerButton Button = EUiPointerButton::Primary;
	/**
	 * 残りのホイールの量（ノッチ数。正は下方向）。処理した要素が差し引く。
	 */
	Toolbox::f32 WheelDelta = 0;
	/**
	 * 残りの横のホイールのノッチ数（正は右へのスクロール）。処理した分を減らして外側へ渡す。
	 */
	Toolbox::f32 WheelDeltaX = 0;
	/**
	 * ポインターが最前面の要素（またはその子孫）の上にあるか。Upでの決定に使う。
	 */
	bool bOverTarget = false;
	/**
	 * 処理済み（以後、親へ泡立たない）。
	 */
	bool bHandled = false;
};

/**
 * キーボード・ゲームパッドの操作の意味。
 */
enum class EUiNavigationCommand : Toolbox::uint8
{
	/**
	 * 次の要素へ（Tab）。
	 */
	Next,
	/**
	 * 前の要素へ（Shift+Tab）。
	 */
	Previous,
	/**
	 * 上。
	 */
	Up,
	/**
	 * 下。
	 */
	Down,
	/**
	 * 左。
	 */
	Left,
	/**
	 * 右。
	 */
	Right,
	/**
	 * 決定。
	 */
	Confirm,
	/**
	 * 戻る。
	 */
	Cancel,
	/**
	 * 種類の数。
	 */
	Count
};

/**
 * フォーカスのある要素へ届く操作。処理しなければ親へ泡立ち、最後にルートの既定動作（フォーカス移動・戻る）になる。
 */
struct FUiNavigationEvent
{
	/**
	 * 操作の意味。
	 */
	EUiNavigationCommand Command = EUiNavigationCommand::Confirm;
	/**
	 * 押し続けによる繰り返しか（決定は繰り返さない）。
	 */
	bool bRepeat = false;
	/**
	 * 処理済み。
	 */
	bool bHandled = false;
};

/**
 * 毎フレームの更新に渡す情報。
 */
struct FUiUpdateContext
{
	/**
	 * 前回からの秒数（Sceneのポーズに依らない実時間。上限で切り詰め済み）。
	 */
	Toolbox::f64 DeltaSeconds = 0;
	/**
	 * ルートの経過秒数。
	 */
	Toolbox::f64 ElapsedSeconds = 0;
};
} // namespace Dxf
#endif
