// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_PHYSICS_SNAPSHOT_BUILDER_H
#define DXF_PRIVATE_PHYSICS_SNAPSHOT_BUILDER_H
#include "Dxf/PhysicsSnapshot.h"
namespace Dxf::PhysicsPrivate
{
/**
 * Step完了の観測情報。Worldと同じ外部直列化契約で使用する。
 */
struct FSnapshotStepState
{
	/**
	 * 正常完了したStepの数。
	 */
	Toolbox::uint64 StepIndex = 0;
	/**
	 * 最終正常Stepの秒数。
	 */
	Toolbox::f64 LastDeltaSeconds = 0;
	/**
	 * 最終正常Stepの分割数。
	 */
	Toolbox::uint32 LastSubSteps = 0;
	/**
	 * Step実行中の再入を拒否するフラグ。スレッド同期用ではない。
	 */
	bool bInStep = false;
	/**
	 * 初期状態または直近の正常更新後なら採取を許可する。
	 */
	bool bCaptureAllowed = true;
};
/**
 * Step中断を正常完了と区別する。数値計算や状態の巻戻しは行わない。
 */
class FSnapshotStepGuard
{
public:
	/**
	 * 有効な引数の検証後、Worldを更新する前に観測を開始する。
	 * @param State World所有の観測情報。
	 * @param Seconds 今回の更新秒数。
	 * @param SubSteps 今回の分割数。
	 */
	FSnapshotStepGuard(FSnapshotStepState& State, Toolbox::f64 Seconds, Toolbox::uint32 SubSteps)
	    : m_pState(&State), m_Seconds(Seconds), m_SubSteps(SubSteps)
	{
		if (State.bInStep || State.StepIndex == Toolbox::TNumericLimits<Toolbox::uint64>::Max())
		{
			throw Toolbox::FException("Physics snapshot step overflow or reentrant Step");
		}
		State.bInStep = true;
		State.bCaptureAllowed = false;
	}
	/**
	 * 例外時にも実行中フラグを戻す。正常完了していなければ採取禁止を保持する。
	 */
	~FSnapshotStepGuard()
	{
		m_pState->bInStep = false;
	}
	/**
	 * 同じ観測情報の二重管理を禁止する。
	 */
	FSnapshotStepGuard(const FSnapshotStepGuard&) = delete;
	/**
	 * 同じ観測情報の二重管理を禁止する。
	 */
	FSnapshotStepGuard& operator=(const FSnapshotStepGuard&) = delete;
	/**
	 * 力・トルクの消去まで正常完了した時点で、一度だけ更新を確定する。
	 */
	void Complete() noexcept
	{
		if (!m_bCompleted)
		{
			++m_pState->StepIndex;
			m_pState->LastDeltaSeconds = m_Seconds;
			m_pState->LastSubSteps = m_SubSteps;
			m_pState->bCaptureAllowed = true;
			m_bCompleted = true;
		}
	}
private:
	/**
	 * Worldが所有し、このガードより長く生存する観測情報。
	 */
	FSnapshotStepState* m_pState;
	/**
	 * 呼出し元の更新秒数。
	 */
	Toolbox::f64 m_Seconds;
	/**
	 * 呼出し元の分割数。
	 */
	Toolbox::uint32 m_SubSteps;
	/**
	 * 完了を二度数えないための状態。
	 */
	bool m_bCompleted = false;
};
/**
 * 実Worldの登録配列から、生存値だけを二回の走査で複製する共通処理。
 * 元配列を変更せず、上限超過・不正な親参照・確保失敗は例外で通知する。
 * @param World 元Worldの識別子。
 * @param State 最終Stepの観測情報。
 * @param Bodies Bodyの登録領域。削除済みスロットを含めて渡す。
 * @param Colliders Colliderの登録領域。削除済みスロットを含めて渡す。
 * @param Limits 出力数の上限。スロット数ではなく生存件数を制限する。
 * @param CopyBody 次元ごとの姿勢表現を観察値へ複製する処理。
 */
template <typename TSnapshot, typename TBodyStore, typename TColliderStore, typename TCopyBody>
TSnapshot CaptureSnapshot_Internal(Toolbox::uint64 World, const FSnapshotStepState& State,
                                   const TBodyStore& Bodies, const TColliderStore& Colliders,
                                   const FPhysicsSnapshotLimits& Limits, TCopyBody CopyBody)
{
	if (World == 0 || State.bInStep || !State.bCaptureAllowed)
	{
		throw Toolbox::FException("Physics snapshot requires an idle World after a successful Step");
	}
	// 制限の検査中は配列を確保しない。
	Toolbox::size_t BodyCount = 0;
	Toolbox::size_t ColliderCount = 0;
	for (const auto& Body : Bodies)
	{
		if (Body.bAlive)
		{
			if (BodyCount >= Limits.MaxBodies)
			{
				throw Toolbox::FException("Physics snapshot body limit exceeded");
			}
			++BodyCount;
		}
	}
	for (const auto& Collider : Colliders)
	{
		if (!Collider.bAlive)
		{
			continue;
		}
		if (ColliderCount >= Limits.MaxColliders)
		{
			throw Toolbox::FException("Physics snapshot collider limit exceeded");
		}
		if (Collider.Body.World != World || Collider.Body.Index >= Bodies.Size() ||
		    !Bodies[Collider.Body.Index].bAlive ||
		    Bodies[Collider.Body.Index].Generation != Collider.Body.Generation)
		{
			throw Toolbox::FException("Physics snapshot collider has an invalid body");
		}
		++ColliderCount;
	}
	// 一時結果だけへ書き、例外では全体を破棄する。
	TSnapshot Snapshot;
	Snapshot.World = World;
	Snapshot.StepIndex = State.StepIndex;
	Snapshot.LastDeltaSeconds = State.LastDeltaSeconds;
	Snapshot.LastSubSteps = State.LastSubSteps;
	Snapshot.Bodies.Reserve(BodyCount);
	Snapshot.Colliders.Reserve(ColliderCount);
	for (Toolbox::size_t Index = 0; Index < Bodies.Size(); ++Index)
	{
		const auto& Body = Bodies[Index];
		if (!Body.bAlive)
		{
			continue;
		}
		typename TSnapshot::FBody Item;
		Item.Id = {World, Index, Body.Generation};
		Item.Type = Body.Type;
		Item.Position = Body.Position;
		Item.Velocity = Body.Velocity;
		Item.AngularVelocity = Body.AngularVelocity;
		Item.bSleeping = Body.bSleeping;
		Item.bUseContinuous = Body.bUseContinuous;
		CopyBody(Body, Item);
		Snapshot.Bodies.PushBack(Toolbox::Move(Item));
	}
	for (Toolbox::size_t Index = 0; Index < Colliders.Size(); ++Index)
	{
		const auto& Collider = Colliders[Index];
		if (!Collider.bAlive)
		{
			continue;
		}
		typename TSnapshot::FCollider Item;
		Item.Id = {Collider.Body, Index, Collider.Generation};
		Item.LocalShape = Collider.Shape;
		Item.Friction = Collider.Friction;
		Item.Restitution = Collider.Restitution;
		Snapshot.Colliders.PushBack(Toolbox::Move(Item));
	}
	return Snapshot;
}
} // namespace Dxf::PhysicsPrivate
#endif
