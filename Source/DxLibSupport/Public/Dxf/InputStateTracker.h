#pragma once
#include "Dxf/InputSnapshot.h"

namespace Dxf
{
/**
 * フレーム間の入力履歴を更新する。
 */
class FInputStateTracker
{
public:
	/**
	 * 一時停止と時間倍率を反映して時計を進める。
	 * @param Raw デバイスから取得した入力状態。
	 */
	void Advance(FRawInput Raw) noexcept
	{
		if (!Raw.bFocused)
		{
			Raw.Keys.Fill(false);
			Raw.MouseButtons.Fill(false);
			Raw.Pads = {};
			Raw.Wheel = 0;
			m_bFocusLost = true;
		}
		else if (m_bFocusLost)
		{
			// フォーカスが戻った時点で押され続けている入力は、物理的に離す（スティックは中立へ戻す）まで渡さない。
			// 失焦の間の押し続けを、復帰したフレームの新しい押下（誤った決定・ジャンプ）にしない。
			m_bFocusLost = false;
			m_HeldKeys = Raw.Keys;
			m_HeldMouse = Raw.MouseButtons;
			for (Toolbox::size_t Pad = 0; Pad < Raw.Pads.Size(); ++Pad)
			{
				m_HeldPadButtons[Pad] = Raw.Pads[Pad].Buttons;
				m_HeldStick[Pad] = !IsNeutral_Internal(Raw.Pads[Pad]);
			}
		}
		MaskHeld_Internal(Raw);
		// ゲームパッドの番号を順に処理する。
		for (auto& Pad : Raw.Pads)
		{
			if (!Pad.bConnected)
			{
				Pad = {};
			}
		}
		m_Snapshot.m_Previous = m_Snapshot.m_Current;
		m_Snapshot.m_Current = Raw;
	}
	/**
	 * フレーム内で固定する入力情報を取得する。
	 */
	FORCEINLINE const FInputSnapshot& GetSnapshot() const noexcept
	{
		return m_Snapshot;
	}

private:
	/**
	 * スティックが中立か（復帰時の押し続けの判定）。
	 */
	static bool IsNeutral_Internal(const FGamepadState& Pad) noexcept
	{
		return Pad.LeftX * Pad.LeftX + Pad.LeftY * Pad.LeftY < NeutralStick * NeutralStick;
	}
	/**
	 * 復帰時から押され続けている入力を離した扱いにし、離したものは以後の対象から外す。
	 */
	void MaskHeld_Internal(FRawInput& Raw) noexcept
	{
		for (Toolbox::size_t Key = 0; Key < Raw.Keys.Size(); ++Key)
		{
			m_HeldKeys[Key] = m_HeldKeys[Key] && Raw.Keys[Key];
			Raw.Keys[Key] = Raw.Keys[Key] && !m_HeldKeys[Key];
		}
		for (Toolbox::size_t Button = 0; Button < Raw.MouseButtons.Size(); ++Button)
		{
			m_HeldMouse[Button] = m_HeldMouse[Button] && Raw.MouseButtons[Button];
			Raw.MouseButtons[Button] = Raw.MouseButtons[Button] && !m_HeldMouse[Button];
		}
		for (Toolbox::size_t Pad = 0; Pad < Raw.Pads.Size(); ++Pad)
		{
			FGamepadState& State = Raw.Pads[Pad];
			for (Toolbox::size_t Button = 0; Button < State.Buttons.Size(); ++Button)
			{
				m_HeldPadButtons[Pad][Button] = m_HeldPadButtons[Pad][Button] && State.Buttons[Button];
				State.Buttons[Button] = State.Buttons[Button] && !m_HeldPadButtons[Pad][Button];
			}
			m_HeldStick[Pad] = m_HeldStick[Pad] && !IsNeutral_Internal(State);
			if (m_HeldStick[Pad])
			{
				State.LeftX = 0.0f;
				State.LeftY = 0.0f;
			}
		}
	}
	/**
	 * 中立とみなすスティックの大きさ。
	 */
	static constexpr Toolbox::f32 NeutralStick = 0.2f;
	/**
	 * フォーカスを失っているか（戻ったフレームで押し続けを記録する）。
	 */
	bool m_bFocusLost = false;
	/**
	 * 復帰時から押され続けている入力。
	 */
	decltype(FRawInput{}.Keys) m_HeldKeys{};
	decltype(FRawInput{}.MouseButtons) m_HeldMouse{};
	Toolbox::TArray<Toolbox::TArray<bool, 16>, 4> m_HeldPadButtons{};
	Toolbox::TArray<bool, 4> m_HeldStick{};
	/**
	 * フレーム内で固定する入力情報。
	 */
	FInputSnapshot m_Snapshot;
};
} // namespace Dxf
