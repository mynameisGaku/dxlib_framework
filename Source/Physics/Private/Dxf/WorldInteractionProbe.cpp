// SPDX-License-Identifier: NOASSERTION
#include "WorldInteractionProbe.h"

namespace Dxf::PhysicsPrivate
{
namespace
{
// 計測はイベント・キャラクターを実行する呼出しスレッドだけ。共有の可変配列は作らない。
thread_local FWorldInteractionProbe* ActiveProbe_Internal = nullptr;
} // namespace

FWorldInteractionProbe::FWorldInteractionProbe(Toolbox::uint64 (*ReadNanoseconds)(),
                                               Toolbox::uint64 (*ReadAllocations)()) noexcept
    : m_pPrevious(ActiveProbe_Internal), m_pReadNanoseconds(ReadNanoseconds), m_pReadAllocations(ReadAllocations)
{
	ActiveProbe_Internal = this;
}

FWorldInteractionProbe::~FWorldInteractionProbe()
{
	ActiveProbe_Internal = m_pPrevious;
}

void FWorldInteractionProbe::CountCandidate_Internal() noexcept
{
	if (ActiveProbe_Internal != nullptr)
	{
		++ActiveProbe_Internal->m_Candidates;
	}
}

void FWorldInteractionProbe::Switch_Internal(EPhase Next) noexcept
{
	// 読出しと集計の費用も計測版の実行時間に残す。推定値を差し引かない。
	const Toolbox::uint64 Now = m_pReadNanoseconds();
	const Toolbox::uint64 Allocations = m_pReadAllocations();
	++m_ClockReads;
	if (m_Phase != EPhase::Count)
	{
		m_Values[static_cast<Toolbox::size_t>(m_Phase)].Nanoseconds += Now - m_StartNanoseconds;
		m_Values[static_cast<Toolbox::size_t>(m_Phase)].Allocations += Allocations - m_StartAllocations;
	}
	m_Phase = Next;
	m_StartNanoseconds = Now;
	m_StartAllocations = Allocations;
}

FWorldInteractionProbe::FRegion::FRegion(EPhase Phase) noexcept : m_pProbe(ActiveProbe_Internal)
{
	if (m_pProbe != nullptr)
	{
		m_Previous = m_pProbe->m_Phase;
		m_pProbe->Switch_Internal(Phase);
		++m_pProbe->m_Values[static_cast<Toolbox::size_t>(Phase)].Calls;
	}
}

FWorldInteractionProbe::FRegion::~FRegion()
{
	Stop();
}

void FWorldInteractionProbe::FRegion::Stop() noexcept
{
	if (m_pProbe != nullptr)
	{
		m_pProbe->Switch_Internal(m_Previous);
		m_pProbe = nullptr;
	}
}
} // namespace Dxf::PhysicsPrivate
