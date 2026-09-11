#pragma once
#include "Dxf/InputSnapshot.h"

namespace Dxf
{
class FInputStateTracker
{
public:
	void Advance(FRawInput Raw) noexcept
	{
		if (!Raw.bFocused)
		{
			Raw.Keys.fill(false);
			Raw.MouseButtons.fill(false);
			Raw.Pads = {};
			Raw.Wheel = 0;
		}
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
	const FInputSnapshot& GetSnapshot() const noexcept
	{
		return m_Snapshot;
	}
private:
	FInputSnapshot m_Snapshot;
};
}
