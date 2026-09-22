// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PhysicsDebugRecorder3D.h"
namespace Dxf
{
FPhysicsDebugRecorder3D::FPhysicsDebugRecorder3D(Toolbox::size_t HistoryCapacity, Toolbox::uint64 HistoryInterval)
    : m_History(HistoryCapacity), m_HistoryInterval(HistoryInterval)
{
	if (HistoryInterval == 0)
	{
		throw Toolbox::FException("Invalid debug history interval");
	}
}
void FPhysicsDebugRecorder3D::SetEnabled(bool bEnabled) noexcept
{
	m_bEnabled = bEnabled;
	if (!bEnabled)
	{
		// 停止中の値を現在値として表示しない。
		m_Live.Items.Clear();
		m_Live = {};
		m_bHasLive = false;
	}
}
TResult<bool> FPhysicsDebugRecorder3D::Record(const FPhysicsWorld3D& World, Toolbox::f64 SimulationSeconds,
                                              bool bForceHistory)
{
	if (!m_bEnabled)
	{
		return TResult<bool>::Success(false);
	}
	auto Captured = CapturePhysicsDebugSnapshot3D(World, SimulationSeconds);
	if (!Captured)
	{
		return TResult<bool>::Failure(Captured.Error());
	}
	const Toolbox::uint64 Step = Captured.Value().Step;
	// 同じStepの再採取では履歴を増やさない。停止中の再描画は新しいStepではない。
	const bool bNewStep = m_History.GetCount() == 0 || Step > m_HistoryStep;
	if (bNewStep && (bForceHistory || m_History.GetCount() == 0 || Step % m_HistoryInterval == 0))
	{
		// 複写・検査に失敗しても最新値を変更しない。
		auto Pushed = m_History.Push(Captured.Value());
		if (!Pushed)
		{
			return TResult<bool>::Failure(Pushed.Error());
		}
		m_HistoryStep = Step;
	}
	m_Live = Toolbox::Move(Captured).Value();
	m_bHasLive = true;
	return TResult<bool>::Success(true);
}
void FPhysicsDebugRecorder3D::Clear() noexcept
{
	m_History.Clear();
	m_Live.Items.Clear();
	m_Live = {};
	m_HistoryStep = 0;
	m_bHasLive = false;
}
} // namespace Dxf
