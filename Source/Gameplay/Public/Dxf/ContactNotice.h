#pragma once
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
namespace Dxf
{
/**
 * 配送先から見た接触・Triggerの状態遷移。Worldのイベント（ColliderA／B）を、受け取る側（Self）と相手（Other）へ並べ替えた値。
 * 相手のIDは値として残り、現在のWorldで解決できるとは限らない（削除済みの相手のEndなど）。
 */
template <typename TColliderId, typename TVector> struct TContactNotice
{
	/**
	 * 種類（Contact／Trigger）。
	 */
	EWorldEventKind Kind = EWorldEventKind::Contact;
	/**
	 * 状態遷移（Begin／Stay／End）。
	 */
	EWorldEventPhase Phase = EWorldEventPhase::Begin;
	/**
	 * Endの理由（Begin／StayはNone）。
	 */
	EWorldEventEndReason EndReason = EWorldEventEndReason::None;
	/**
	 * 受け取る側のCollider。
	 */
	TColliderId Self;
	/**
	 * 相手のCollider。
	 */
	TColliderId Other;
	/**
	 * ContactのBegin／Stayで求められた場合だけの、相手から受け取る側へ向く単位法線。Trigger・Endでは空。
	 */
	Toolbox::TOptional<TVector> Normal;
	/**
	 * 元のイベントを確定した成功したStepの通算番号。
	 */
	Toolbox::uint64 StepIndex = 0;
	/**
	 * 元のイベントのバッチの通算番号。
	 */
	Toolbox::uint64 BatchId = 0;
};
/**
 * 2Dの配送先から見た状態遷移。
 */
using FContactNotice2D = TContactNotice<FColliderId2D, Toolbox::FVector2>;
/**
 * 3Dの配送先から見た状態遷移。
 */
using FContactNotice3D = TContactNotice<FColliderId3D, Toolbox::FVector3>;
/**
 * Worldのイベントを、受け取る側から見た値へ並べ替える。法線は受け取る側へ向くように符号を変える。
 * @param Event Worldのイベント。
 * @param bSelfIsA 受け取る側がColliderAか。
 * @param StepIndex 元のバッチのStepの通算番号。
 * @param BatchId 元のバッチの通算番号。
 */
template <typename TColliderId, typename TVector>
TContactNotice<TColliderId, TVector> MakeContactNotice(const TWorldEvent<TColliderId, TVector>& Event, bool bSelfIsA,
                                                       Toolbox::uint64 StepIndex, Toolbox::uint64 BatchId)
{
	TContactNotice<TColliderId, TVector> Notice;
	Notice.Kind = Event.Kind;
	Notice.Phase = Event.Phase;
	Notice.EndReason = Event.EndReason;
	Notice.Self = bSelfIsA ? Event.ColliderA : Event.ColliderB;
	Notice.Other = bSelfIsA ? Event.ColliderB : Event.ColliderA;
	// Worldの法線はBからAへ向く。受け取る側がAならそのまま、Bなら逆向き。
	if (Event.Normal)
	{
		Notice.Normal = bSelfIsA ? *Event.Normal : -*Event.Normal;
	}
	Notice.StepIndex = StepIndex;
	Notice.BatchId = BatchId;
	return Notice;
}
} // namespace Dxf
