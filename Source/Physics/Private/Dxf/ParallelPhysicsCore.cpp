// SPDX-License-Identifier: NOASSERTION
#include "ParallelPhysicsCore.h"
#include "Toolbox/Algorithm.h"
namespace Dxf::PhysicsPrivate
{
namespace
{
constexpr Toolbox::size_t BroadPhaseChunkSize = 64;
constexpr Toolbox::size_t InvalidIndex = static_cast<Toolbox::size_t>(-1);

FORCEINLINE bool IsValidBounds_Internal(const FBroadPhaseBounds& Bounds) noexcept
{
	if (!Toolbox::IsFinite(Bounds.MinX) || !Toolbox::IsFinite(Bounds.MinY) ||
	    !Toolbox::IsFinite(Bounds.MaxX) || !Toolbox::IsFinite(Bounds.MaxY))
	{
		return false;
	}
	if (Bounds.MinX > Bounds.MaxX || Bounds.MinY > Bounds.MaxY)
	{
		return false;
	}
	if (Bounds.bUseZ)
	{
		if (!Toolbox::IsFinite(Bounds.MinZ) || !Toolbox::IsFinite(Bounds.MaxZ) || Bounds.MinZ > Bounds.MaxZ)
		{
			return false;
		}
	}
	return true;
}

FORCEINLINE bool IsSameBody_Internal(const FBroadPhaseEntry& A, const FBroadPhaseEntry& B) noexcept
{
	return A.BodyIndex == B.BodyIndex && A.BodyGeneration == B.BodyGeneration;
}

FORCEINLINE bool Overlaps_Internal(const FBroadPhaseBounds& A, const FBroadPhaseBounds& B,
                                   Toolbox::f64 Margin) noexcept
{
	if (Toolbox::f64(A.MinX) - Margin > Toolbox::f64(B.MaxX) + Margin ||
	    Toolbox::f64(B.MinX) - Margin > Toolbox::f64(A.MaxX) + Margin ||
	    Toolbox::f64(A.MinY) - Margin > Toolbox::f64(B.MaxY) + Margin ||
	    Toolbox::f64(B.MinY) - Margin > Toolbox::f64(A.MaxY) + Margin)
	{
		return false;
	}
	if (A.bUseZ || B.bUseZ)
	{
		if (!(A.bUseZ && B.bUseZ))
		{
			return false;
		}
		if (Toolbox::f64(A.MinZ) - Margin > Toolbox::f64(B.MaxZ) + Margin ||
		    Toolbox::f64(B.MinZ) - Margin > Toolbox::f64(A.MaxZ) + Margin)
		{
			return false;
		}
	}
	return true;
}

bool EntryLess_Internal(const FBroadPhaseEntry& A, const FBroadPhaseEntry& B) noexcept
{
	if (A.Bounds.MinX != B.Bounds.MinX)
	{
		return A.Bounds.MinX < B.Bounds.MinX;
	}
	return A.ColliderIndex < B.ColliderIndex;
}

bool PairLess_Internal(const FBroadPhasePair& A, const FBroadPhasePair& B) noexcept
{
	if (A.FirstColliderIndex != B.FirstColliderIndex)
	{
		return A.FirstColliderIndex < B.FirstColliderIndex;
	}
	return A.SecondColliderIndex < B.SecondColliderIndex;
}

Toolbox::size_t FindRoot_Internal(Toolbox::TVector<Toolbox::size_t>& Parents, Toolbox::size_t Index) noexcept
{
	Toolbox::size_t Root = Index;
	while (Parents[Root] != Root)
	{
		Root = Parents[Root];
	}
	while (Parents[Index] != Index)
	{
		const Toolbox::size_t Next = Parents[Index];
		Parents[Index] = Root;
		Index = Next;
	}
	return Root;
}

void Union_Internal(Toolbox::TVector<Toolbox::size_t>& Parents, Toolbox::size_t A, Toolbox::size_t B) noexcept
{
	Toolbox::size_t RootA = FindRoot_Internal(Parents, A);
	Toolbox::size_t RootB = FindRoot_Internal(Parents, B);
	if (RootA == RootB)
	{
		return;
	}
	// 常に小さいBody indexを根にして入力順に依存しない。
	if (RootB < RootA)
	{
		const Toolbox::size_t Temporary = RootA;
		RootA = RootB;
		RootB = Temporary;
	}
	Parents[RootB] = RootA;
}
} // namespace

void FBroadPhase::Generate(const Toolbox::TVector<FBroadPhaseEntry>& Entries, Toolbox::f32 Margin,
                           Toolbox::FJobSystem* Jobs, Toolbox::TVector<FBroadPhasePair>& Out)
{
	if (!Toolbox::IsFinite(Margin) || Margin < 0)
	{
		throw Toolbox::FException("Invalid broad phase margin");
	}
	Out.Clear();
	if (Entries.Size() < 2)
	{
		return;
	}
	Toolbox::TVector<FBroadPhaseEntry> Ordered(Entries);
	for (Toolbox::size_t Index = 0; Index < Ordered.Size(); ++Index)
	{
		if (!IsValidBounds_Internal(Ordered[Index].Bounds))
		{
			throw Toolbox::FException("Invalid broad phase bounds");
		}
	}
	Toolbox::StableSort(Ordered.Data(), Ordered.Data() + Ordered.Size(), EntryLess_Internal);
	const Toolbox::size_t ChunkCount = (Ordered.Size() + BroadPhaseChunkSize - 1) / BroadPhaseChunkSize;
	Toolbox::TVector<Toolbox::TVector<FBroadPhasePair>> ChunkPairs(ChunkCount);
	const Toolbox::f64 ExpandedMargin = Margin;
	auto ScanChunk = [&](Toolbox::size_t ChunkIndex)
	{
		const Toolbox::size_t Begin = ChunkIndex * BroadPhaseChunkSize;
		const Toolbox::size_t End = Toolbox::Min(Begin + BroadPhaseChunkSize, Ordered.Size());
		Toolbox::TVector<FBroadPhasePair>& Local = ChunkPairs[ChunkIndex];
		for (Toolbox::size_t First = Begin; First < End; ++First)
		{
			const FBroadPhaseEntry& A = Ordered[First];
			for (Toolbox::size_t Second = First + 1; Second < Ordered.Size(); ++Second)
			{
				const FBroadPhaseEntry& B = Ordered[Second];
				if (Toolbox::f64(B.Bounds.MinX) - ExpandedMargin > Toolbox::f64(A.Bounds.MaxX) + ExpandedMargin)
				{
					break;
				}
				if (IsSameBody_Internal(A, B) || (!A.bDynamic && !B.bDynamic))
				{
					continue;
				}
				if (!Overlaps_Internal(A.Bounds, B.Bounds, ExpandedMargin))
				{
					continue;
				}
				FBroadPhasePair Pair;
				if (A.ColliderIndex < B.ColliderIndex)
				{
					Pair.FirstColliderIndex = A.ColliderIndex;
					Pair.SecondColliderIndex = B.ColliderIndex;
				}
				else
				{
					Pair.FirstColliderIndex = B.ColliderIndex;
					Pair.SecondColliderIndex = A.ColliderIndex;
				}
				Local.PushBack(Pair);
			}
		}
	};
	if (Jobs != nullptr && Jobs->GetExecutionThreadCount() > 1)
	{
		if (!Toolbox::ParallelFor(*Jobs, ChunkCount, ScanChunk, 1))
		{
			throw Toolbox::FException("Broad phase job execution failed");
		}
	}
	else
	{
		for (Toolbox::size_t Chunk = 0; Chunk < ChunkCount; ++Chunk)
		{
			ScanChunk(Chunk);
		}
	}
	Toolbox::size_t PairCount = 0;
	for (Toolbox::size_t Chunk = 0; Chunk < ChunkCount; ++Chunk)
	{
		PairCount += ChunkPairs[Chunk].Size();
	}
	Out.Reserve(PairCount);
	for (Toolbox::size_t Chunk = 0; Chunk < ChunkCount; ++Chunk)
	{
		for (Toolbox::size_t Index = 0; Index < ChunkPairs[Chunk].Size(); ++Index)
		{
			Out.PushBack(ChunkPairs[Chunk][Index]);
		}
	}
	if (Out.Size() > 1)
	{
		Toolbox::StableSort(Out.Data(), Out.Data() + Out.Size(), PairLess_Internal);
	}
}

void FIslandManager::Build(Toolbox::size_t BodyCount, const Toolbox::TVector<FIslandEdge>& Edges,
                           Toolbox::TVector<FPhysicsIsland>& Out)
{
	Out.Clear();
	if (BodyCount == 0 || Edges.IsEmpty())
	{
		return;
	}
	Toolbox::TVector<Toolbox::size_t> Parents(BodyCount);
	Toolbox::TVector<Toolbox::uint8> Active(BodyCount);
	for (Toolbox::size_t Index = 0; Index < BodyCount; ++Index)
	{
		Parents[Index] = InvalidIndex;
		Active[Index] = 0;
	}
	for (Toolbox::size_t EdgeIndex = 0; EdgeIndex < Edges.Size(); ++EdgeIndex)
	{
		const FIslandEdge& Edge = Edges[EdgeIndex];
		if (Edge.BodyA >= BodyCount || Edge.BodyB >= BodyCount)
		{
			throw Toolbox::FException("Island edge body index out of range");
		}
		if (Edge.bDynamicA)
		{
			Active[Edge.BodyA] = 1;
			Parents[Edge.BodyA] = Edge.BodyA;
		}
		if (Edge.bDynamicB)
		{
			Active[Edge.BodyB] = 1;
			Parents[Edge.BodyB] = Edge.BodyB;
		}
	}
	for (Toolbox::size_t EdgeIndex = 0; EdgeIndex < Edges.Size(); ++EdgeIndex)
	{
		const FIslandEdge& Edge = Edges[EdgeIndex];
		if (Edge.bDynamicA && Edge.bDynamicB)
		{
			Union_Internal(Parents, Edge.BodyA, Edge.BodyB);
		}
	}
	Toolbox::TVector<Toolbox::size_t> RootToIsland(BodyCount);
	for (Toolbox::size_t Index = 0; Index < BodyCount; ++Index)
	{
		RootToIsland[Index] = InvalidIndex;
	}
	for (Toolbox::size_t BodyIndex = 0; BodyIndex < BodyCount; ++BodyIndex)
	{
		if (Active[BodyIndex] == 0)
		{
			continue;
		}
		const Toolbox::size_t Root = FindRoot_Internal(Parents, BodyIndex);
		if (RootToIsland[Root] == InvalidIndex)
		{
			RootToIsland[Root] = Out.Size();
			FPhysicsIsland Island;
			Island.RootBodyIndex = Root;
			Out.PushBack(Toolbox::Move(Island));
		}
		Out[RootToIsland[Root]].BodyIndices.PushBack(BodyIndex);
	}
	for (Toolbox::size_t EdgeIndex = 0; EdgeIndex < Edges.Size(); ++EdgeIndex)
	{
		const FIslandEdge& Edge = Edges[EdgeIndex];
		Toolbox::size_t DynamicBody = InvalidIndex;
		if (Edge.bDynamicA)
		{
			DynamicBody = Edge.BodyA;
		}
		else if (Edge.bDynamicB)
		{
			DynamicBody = Edge.BodyB;
		}
		if (DynamicBody == InvalidIndex)
		{
			continue;
		}
		const Toolbox::size_t Root = FindRoot_Internal(Parents, DynamicBody);
		const Toolbox::size_t IslandIndex = RootToIsland[Root];
		if (IslandIndex == InvalidIndex)
		{
			throw Toolbox::FException("Island root was not registered");
		}
		Out[IslandIndex].ConstraintIndices.PushBack(Edge.ConstraintIndex);
	}
}
} // namespace Dxf::PhysicsPrivate
