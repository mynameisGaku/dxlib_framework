// SPDX-License-Identifier: NOASSERTION
#include "Dxf/DebugSnapshotHistory.h"
namespace Dxf
{
FDebugSnapshotHistory::FDebugSnapshotHistory(Toolbox::size_t Capacity)
{
	if (Capacity == 0 || Capacity > 240)
	{
		throw Toolbox::FException("Invalid debug history capacity");
	}
	m_Frames.Resize(Capacity);
}
TResult<void> FDebugSnapshotHistory::Push(FPhysicsDebugSnapshot3D Snapshot)
{
	if (!IsValidPhysicsDebugSnapshot3D(Snapshot) || Snapshot.World == 0)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid history snapshot");
	}
	if (m_Count != 0)
	{
		const auto& Latest = m_Frames[(m_Next + m_Frames.Size() - 1) % m_Frames.Size()];
		if (Snapshot.World != Latest.World || Snapshot.Step <= Latest.Step ||
			Snapshot.SimulationSeconds < Latest.SimulationSeconds)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "Nonmonotonic history or changed world");
		}
	}
	static_assert(noexcept(m_Frames[0] = Toolbox::Move(Snapshot)));
	m_Frames[m_Next] = Toolbox::Move(Snapshot);
	m_Next = (m_Next + 1) % m_Frames.Size();
	m_Count = Toolbox::Min(m_Count + 1, m_Frames.Size());
	return {};
}
TResult<FPhysicsDebugSnapshot3D> FDebugSnapshotHistory::ReadAge(Toolbox::size_t Age) const
{
	if (Age >= m_Count)
	{
		return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::NotFound, "Debug history frame not retained");
	}
	return TResult<FPhysicsDebugSnapshot3D>::Success(m_Frames[(m_Next + m_Frames.Size() - 1 - Age) % m_Frames.Size()]);
}
void FDebugSnapshotHistory::Clear() noexcept
{
	for (auto& Frame : m_Frames)
	{
		Frame.Items.Clear();
	}
	m_Next = 0;
	m_Count = 0;
}
}
