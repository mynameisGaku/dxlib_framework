#pragma once
#include "Toolbox/Array.h"
#include "Toolbox/Utility.h"

namespace Dxf
{
/**
 * 対応するキーボードのキーを識別する。
 */
enum class EKey : Toolbox::size_t
{
	/**
	 * Aキー。
	 */
	A,
	/**
	 * Bキー。
	 */
	B,
	/**
	 * Cキー。
	 */
	C,
	/**
	 * Dキー。
	 */
	D,
	/**
	 * Eキー。
	 */
	E,
	/**
	 * Fキー。
	 */
	F,
	/**
	 * Gキー。
	 */
	G,
	/**
	 * Hキー。
	 */
	H,
	/**
	 * Iキー。
	 */
	I,
	/**
	 * Jキー。
	 */
	J,
	/**
	 * Kキー。
	 */
	K,
	/**
	 * Lキー。
	 */
	L,
	/**
	 * Mキー。
	 */
	M,
	/**
	 * Nキー。
	 */
	N,
	/**
	 * Oキー。
	 */
	O,
	/**
	 * Pキー。
	 */
	P,
	/**
	 * Qキー。
	 */
	Q,
	/**
	 * Rキー。
	 */
	R,
	/**
	 * Sキー。
	 */
	S,
	/**
	 * Tキー。
	 */
	T,
	/**
	 * Uキー。
	 */
	U,
	/**
	 * Vキー。
	 */
	V,
	/**
	 * Wキー。
	 */
	W,
	/**
	 * Xキー。
	 */
	X,
	/**
	 * Yキー。
	 */
	Y,
	/**
	 * Zキー。
	 */
	Z,
	/**
	 * Num0キー。
	 */
	Num0,
	/**
	 * Num1キー。
	 */
	Num1,
	/**
	 * Num2キー。
	 */
	Num2,
	/**
	 * Num3キー。
	 */
	Num3,
	/**
	 * Num4キー。
	 */
	Num4,
	/**
	 * Num5キー。
	 */
	Num5,
	/**
	 * Num6キー。
	 */
	Num6,
	/**
	 * Num7キー。
	 */
	Num7,
	/**
	 * Num8キー。
	 */
	Num8,
	/**
	 * Num9キー。
	 */
	Num9,
	/**
	 * スペースキー。
	 */
	Space,
	/**
	 * エスケープキー。
	 */
	Escape,
	/**
	 * エンターキー。
	 */
	Enter,
	/**
	 * タブキー。
	 */
	Tab,
	/**
	 * バックスペースキー。
	 */
	Backspace,
	/**
	 * 左矢印キー。
	 */
	Left,
	/**
	 * 右矢印キー。
	 */
	Right,
	/**
	 * 上矢印キー。
	 */
	Up,
	/**
	 * 下矢印キー。
	 */
	Down,
	/**
	 * 左シフトキー。
	 */
	LeftShift,
	/**
	 * 右シフトキー。
	 */
	RightShift,
	/**
	 * 左コントロールキー。
	 */
	LeftControl,
	/**
	 * 右コントロールキー。
	 */
	RightControl,
	/**
	 * F1キー。
	 */
	F1,
	/**
	 * F2キー。
	 */
	F2,
	/**
	 * F3キー。
	 */
	F3,
	/**
	 * F4キー。
	 */
	F4,
	/**
	 * F5キー。
	 */
	F5,
	/**
	 * F6キー。
	 */
	F6,
	/**
	 * F7キー。
	 */
	F7,
	/**
	 * F8キー。
	 */
	F8,
	/**
	 * F9キー。
	 */
	F9,
	/**
	 * F10キー。
	 */
	F10,
	/**
	 * F11キー。
	 */
	F11,
	/**
	 * F12キー。
	 */
	F12,
	/**
	 * 有効な識別子の総数。
	 */
	Count
};
/**
 * マウスボタンを識別する。
 */
enum class EMouseButton : Toolbox::size_t
{
	/**
	 * 左のマウスボタン。
	 */
	Left,
	/**
	 * 右のマウスボタン。
	 */
	Right,
	/**
	 * 中央のマウスボタン。
	 */
	Middle,
	/**
	 * 有効な識別子の総数。
	 */
	Count
};
/**
 * ゲームパッド1台分の接続状態と入力を保持する。
 */
struct FGamepadState
{
	/**
	 * デバイスが接続されているか。
	 */
	bool bConnected = false;
	/**
	 * ボタンごとの押下状態。
	 */
	Toolbox::TArray<bool, 16> Buttons{};
	/**
	 * 左スティックのX軸入力。
	 */
	Toolbox::f32 LeftX = 0.0f;
	/**
	 * 左スティックのY軸入力。
	 */
	Toolbox::f32 LeftY = 0.0f;
};
/**
 * 各入力デバイスから取得した状態を保持する。
 */
struct FRawInput
{
	/**
	 * キーごとの押下状態。
	 */
	Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EKey::Count)> Keys{};
	/**
	 * マウスボタンごとの押下状態。
	 */
	Toolbox::TArray<bool, static_cast<Toolbox::size_t>(EMouseButton::Count)> MouseButtons{};
	/**
	 * ゲームパッドごとの入力状態。
	 */
	Toolbox::TArray<FGamepadState, 4> Pads{};
	/**
	 * マウスのX座標。
	 */
	Toolbox::int32 MouseX = 0;
	/**
	 * マウスのY座標。
	 */
	Toolbox::int32 MouseY = 0;
	/**
	 * マウスホイールの移動量。
	 */
	Toolbox::int32 Wheel = 0;
	/**
	 * ウィンドウが入力フォーカスを持つか。
	 */
	bool bFocused = true;
};
} // namespace Dxf
