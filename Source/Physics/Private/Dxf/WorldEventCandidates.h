// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_WORLD_EVENT_CANDIDATES_H
#define DXF_PRIVATE_WORLD_EVENT_CANDIDATES_H
#include "ParallelPhysicsCore.h"
#include "QueryResultOrder.h"

namespace Dxf::PhysicsPrivate
{
/**
 * Worldが登録時に確保した境界列を並べ替え、イベント候補を中間配列へ保存せず通知する。
 * 同じBodyとStatic同士を除外し、Collider番号が小さい方を先に渡す。候補間の通知順は保証しない。
 * @param Entries 有効Colliderの境界。処理中はX方向の最小値順へ並べ替える。
 * @param Margin 各境界へ加える有限・非負の距離。
 * @param Visit 候補のColliderスロット二つを受ける関数。Worldを変更しないこと。
 */
template <typename TVisit>
void VisitWorldEventCandidates_Internal(Toolbox::TVector<FBroadPhaseEntry>& Entries, Toolbox::f32 Margin,
                                        TVisit&& Visit)
{
	if (!Toolbox::IsFinite(Margin) || Margin < 0)
	{
		throw Toolbox::FException("Invalid event broad phase margin");
	}
	for (const auto& Entry : Entries)
	{
		// SolverのBroadPhaseと同じ境界の妥当性を要求する。
		const auto& Bounds = Entry.Bounds;
		if (!Toolbox::IsFinite(Bounds.MinX) || !Toolbox::IsFinite(Bounds.MinY) || !Toolbox::IsFinite(Bounds.MaxX) ||
		    !Toolbox::IsFinite(Bounds.MaxY) || Bounds.MinX > Bounds.MaxX || Bounds.MinY > Bounds.MaxY ||
		    (Bounds.bUseZ &&
		     (!Toolbox::IsFinite(Bounds.MinZ) || !Toolbox::IsFinite(Bounds.MaxZ) || Bounds.MinZ > Bounds.MaxZ)))
		{
			throw Toolbox::FException("Invalid event broad phase bounds");
		}
	}
	// 同順位もCollider番号で決めるため、安定ソートの作業配列は必要ない。
	HeapSort_Internal(Entries.Data(), Entries.Size(),
	                  [](const FBroadPhaseEntry& A, const FBroadPhaseEntry& B)
	                  {
		                  return A.Bounds.MinX == B.Bounds.MinX ? A.ColliderIndex < B.ColliderIndex
		                                                        : A.Bounds.MinX < B.Bounds.MinX;
	                  });
	const Toolbox::f64 Expanded = Margin;
	for (Toolbox::size_t First = 0; First < Entries.Size(); ++First)
	{
		const auto& A = Entries[First];
		for (Toolbox::size_t Second = First + 1; Second < Entries.Size(); ++Second)
		{
			const auto& B = Entries[Second];
			if (B.Bounds.MinX - Expanded > A.Bounds.MaxX + Expanded)
			{
				break;
			}
			if ((A.BodyIndex == B.BodyIndex && A.BodyGeneration == B.BodyGeneration) || (!A.bDynamic && !B.bDynamic))
			{
				continue;
			}
			if (A.Bounds.MinY - Expanded > B.Bounds.MaxY + Expanded ||
			    B.Bounds.MinY - Expanded > A.Bounds.MaxY + Expanded)
			{
				continue;
			}
			if (A.Bounds.bUseZ || B.Bounds.bUseZ)
			{
				if (!(A.Bounds.bUseZ && B.Bounds.bUseZ) || A.Bounds.MinZ - Expanded > B.Bounds.MaxZ + Expanded ||
				    B.Bounds.MinZ - Expanded > A.Bounds.MaxZ + Expanded)
				{
					continue;
				}
			}
			Visit(Toolbox::Min(A.ColliderIndex, B.ColliderIndex), Toolbox::Max(A.ColliderIndex, B.ColliderIndex));
		}
	}
}
} // namespace Dxf::PhysicsPrivate
#endif
