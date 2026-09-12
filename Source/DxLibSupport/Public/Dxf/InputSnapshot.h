#pragma once
#include "Dxf/InputTypes.h"

namespace Dxf
{
/**
 * 現在と前回の入力から押下と解放を判定する。
 */
class FInputSnapshot
{
public:
	/**
	 * 現在押されているかを調べる。
	 * @param Key 検索または入力のキー。
	 */
	bool IsDown(EKey Key) const noexcept
	{
		return Read_Internal(m_Current.Keys, static_cast<Toolbox::size_t>(Key));
	}
	/**
	 * 今回のフレームで押されたかを調べる。
	 * @param Key 検索または入力のキー。
	 */
	bool WasPressed(EKey Key) const noexcept
	{
		return IsDown(Key) && !Read_Internal(m_Previous.Keys, static_cast<Toolbox::size_t>(Key));
	}
	/**
	 * 今回のフレームで離されたかを調べる。
	 * @param Key 検索または入力のキー。
	 */
	bool WasReleased(EKey Key) const noexcept
	{
		return !IsDown(Key) && Read_Internal(m_Previous.Keys, static_cast<Toolbox::size_t>(Key));
	}
	/**
	 * マウスボタンが押されているかを調べる。
	 * @param Button 入力ボタンの番号。
	 */
	bool IsMouseDown(EMouseButton Button) const noexcept
	{
		return Read_Internal(m_Current.MouseButtons, static_cast<Toolbox::size_t>(Button));
	}
	/**
	 * 今回のフレームでマウスボタンが押されたかを調べる。
	 * @param Button 入力ボタンの番号。
	 */
	bool WasMousePressed(EMouseButton Button) const noexcept
	{
		return IsMouseDown(Button) && !Read_Internal(m_Previous.MouseButtons, static_cast<Toolbox::size_t>(Button));
	}
	/**
	 * 今回のフレームでマウスボタンが離されたかを調べる。
	 * @param Button 入力ボタンの番号。
	 */
	bool WasMouseReleased(EMouseButton Button) const noexcept
	{
		return !IsMouseDown(Button) && Read_Internal(m_Previous.MouseButtons, static_cast<Toolbox::size_t>(Button));
	}
	/**
	 * パッドボタンが押されているかを調べる。
	 * @param Pad ゲームパッドの番号。
	 * @param Button 入力ボタンの番号。
	 */
	bool IsPadDown(Toolbox::size_t Pad, Toolbox::size_t Button) const noexcept
	{
		return PadRead_Internal(m_Current, Pad, Button);
	}
	/**
	 * 今回のフレームでパッドボタンが押されたかを調べる。
	 * @param Pad ゲームパッドの番号。
	 * @param Button 入力ボタンの番号。
	 */
	bool WasPadPressed(Toolbox::size_t Pad, Toolbox::size_t Button) const noexcept
	{
		return IsPadDown(Pad, Button) && !PadRead_Internal(m_Previous, Pad, Button);
	}
	/**
	 * 今回のフレームでパッドボタンが離されたかを調べる。
	 * @param Pad ゲームパッドの番号。
	 * @param Button 入力ボタンの番号。
	 */
	bool WasPadReleased(Toolbox::size_t Pad, Toolbox::size_t Button) const noexcept
	{
		return !IsPadDown(Pad, Button) && PadRead_Internal(m_Previous, Pad, Button);
	}
	/**
	 * デバイスから取得した入力状態を取得する。
	 */
	const FRawInput& GetRaw() const noexcept
	{
		return m_Current;
	}

private:
	friend class FInputStateTracker;
	template <Toolbox::size_t N>
	/**
	 * 対象の入力状態を読み取る。
	 * @param Values 判定対象の入力状態一覧。
	 * @param Index 要素の位置。
	 */
	static bool Read_Internal(const Toolbox::TArray<bool, N>& Values, Toolbox::size_t Index) noexcept
	{
		return Index < N && Values[Index];
	}
	/**
	 * 指定パッドのボタン状態を読み取る。
	 * @param State 現在の状態。
	 * @param Pad ゲームパッドの番号。
	 * @param Button 入力ボタンの番号。
	 */
	static bool PadRead_Internal(const FRawInput& State, Toolbox::size_t Pad, Toolbox::size_t Button) noexcept
	{
		return Pad < State.Pads.Size() && State.Pads[Pad].bConnected && Read_Internal(State.Pads[Pad].Buttons, Button);
	}
	/**
	 * 現在の状態。
	 */
	FRawInput m_Current;
	/**
	 * 前回の状態。
	 */
	FRawInput m_Previous;
};
} // namespace Dxf
