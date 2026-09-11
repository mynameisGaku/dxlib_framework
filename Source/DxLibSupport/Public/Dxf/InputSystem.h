#pragma once
#include "Dxf/InputSource.h"
#include "Dxf/InputStateTracker.h"

namespace Dxf
{
class FInputSystem
{
public:
	explicit FInputSystem(IInputSource& Source) : m_pSource(&Source)
	{
	}
	TResult<void> Update()
	{
		auto Raw = m_pSource->Poll();
		if (!Raw)
		{
			return TResult<void>::Failure(Raw.Error());
		}
		m_Tracker.Advance(Raw.Value());
		return {};
	}
	const FInputSnapshot& GetSnapshot() const noexcept
	{
		return m_Tracker.GetSnapshot();
	}
private:
	IInputSource* m_pSource;
	FInputStateTracker m_Tracker;
};
}
