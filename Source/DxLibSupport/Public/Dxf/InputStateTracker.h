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
		}
		/**
		 * ゲームパッドの番号を順に処理する。
		 */
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
	const FInputSnapshot& GetSnapshot() const noexcept
	{
		return m_Snapshot;
	}

private:
	/**
	 * フレーム内で固定する入力情報。
	 */
	FInputSnapshot m_Snapshot;
};
} // namespace Dxf
