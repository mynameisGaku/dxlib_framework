// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_WORLD_EVENT_H
#define DXF_PHYSICS_WORLD_EVENT_H
#include "Toolbox/Optional.h"
#include "Toolbox/Utility.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 接触イベントの種類。2D／3D共通。
 */
enum class EWorldEventKind : Toolbox::uint8
{
	/**
	 * Solid同士の物理的な接触（少なくとも一方がDynamic）。
	 */
	Contact,
	/**
	 * Sensorを含む組の重なり（少なくとも一方がStatic以外）。
	 */
	Trigger
};
/**
 * 組の状態遷移。成功した固定更新（Step）単位の、前回と今回の確定集合の差。
 */
enum class EWorldEventPhase : Toolbox::uint8
{
	/**
	 * 前回の確定集合になく、今回ある。
	 */
	Begin,
	/**
	 * 前回も今回もある（同じ種類）。
	 */
	Stay,
	/**
	 * 前回あり、今回ない。
	 */
	End
};
/**
 * Endの理由。Begin／StayではNone。
 */
enum class EWorldEventEndReason : Toolbox::uint8
{
	/**
	 * Endではない。
	 */
	None,
	/**
	 * 形状が離れた。
	 */
	Separated,
	/**
	 * どちらかのCollider（またはBody）が削除された。削除後の最初の成功したStepで発行する。
	 */
	Removed,
	/**
	 * Solid／Sensorの区分・衝突フィルターの変更で、組でなくなったか種類が変わった（種類が変わる場合は直後に新しい種類のBegin）。
	 */
	FilterChanged
};
/**
 * Worldの接触イベントの生成の設定。既定は無効（生成・保持の費用を持たない）。
 */
struct FWorldEventSettings
{
	/**
	 * 接触・Triggerのイベントを生成するか。
	 */
	bool bEnabled = false;
	/**
	 * 一回のStepで保持する組の最大数（1〜1048576）。有効化の時点で全ての領域を確保し、以後のStepでは確保しない。
	 * 超えたStepはイベントを発行せず、バッチにbOverflowedと必要な組数を記録する（一部だけを黙って落とさない）。
	 */
	Toolbox::uint32 MaxPairs = 1024;
	/**
	 * Contactとみなす形状間の最大距離（メートル、有限な0〜10）。Solverの貫通許容幅より大きくし、静止接触で途切れないようにする。
	 * Triggerは許容幅0（接触を含む重なり）。
	 */
	Toolbox::f32 ContactMargin = 0.01f;
};
/**
 * 一つの組の状態遷移。値だけを持ち、World・GameObjectへの参照は持たない。
 * ColliderAはColliderのスロット番号が小さい方（同じ組は常に同じ順序）。Body・WorldはColliderのIDに含まれる。
 */
template <typename TColliderId, typename TVector> struct TWorldEvent
{
	/**
	 * 種類。
	 */
	EWorldEventKind Kind = EWorldEventKind::Contact;
	/**
	 * 状態遷移。
	 */
	EWorldEventPhase Phase = EWorldEventPhase::Begin;
	/**
	 * Endの理由（Begin／StayはNone）。
	 */
	EWorldEventEndReason EndReason = EWorldEventEndReason::None;
	/**
	 * 一つ目の当事者（スロット番号が小さい方）。
	 */
	TColliderId ColliderA;
	/**
	 * 二つ目の当事者。
	 */
	TColliderId ColliderB;
	/**
	 * ContactのBegin／Stayで求められた場合だけの、ColliderBからColliderAへ向く単位法線。Trigger・Endでは空。
	 */
	Toolbox::TOptional<TVector> Normal;
};
/**
 * 一回の成功したStepのイベントの集まり。Worldが所有し、次のStep・設定の変更・Worldの破棄まで有効。
 * 読んでも消費しない（複数の読者が同じバッチを読める）。
 */
template <typename TEvent> struct TWorldEventBatch
{
	/**
	 * 直前のStepが成功し、イベントが有効だったか。途中で失敗したStep・無効の間はfalseで、Eventsは空。
	 */
	bool bPublished = false;
	/**
	 * 有効化・設定の変更の後の最初のバッチか（以前の組の記録がないため、今ある組がすべてBeginになる）。
	 */
	bool bReset = false;
	/**
	 * 組の数がMaxPairsを超え、イベントを発行しなかったか。記録済みの組は保持し、次に上限内のStepで差を発行する。
	 */
	bool bOverflowed = false;
	/**
	 * 成功したStepの通算番号（CaptureSnapshotのStepIndexと同じ）。
	 */
	Toolbox::uint64 StepIndex = 0;
	/**
	 * 発行したバッチの通算番号（1から。読む回数では増えない）。
	 */
	Toolbox::uint64 BatchId = 0;
	/**
	 * このStepで確定した組の数（上限内なら保持している組の数）。
	 */
	Toolbox::uint32 PairCount = 0;
	/**
	 * このStepで見つけた組の総数（bOverflowedならMaxPairsを超える）。
	 */
	Toolbox::uint32 RequiredPairs = 0;
	/**
	 * 組の順（ColliderA、ColliderBのスロット番号の昇順）の状態遷移。種類が変わった組はEnd→Beginの順。
	 */
	Toolbox::TVector<TEvent> Events;
};
} // namespace Dxf
#endif
