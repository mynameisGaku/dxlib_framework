// SPDX-License-Identifier: NOASSERTION
#include "InteractionScenarios.h"
#include "BenchmarkSupport.h"
#include "Dxf/AssetService.h"
#include "Dxf/ContactListenerComponent2D.h"
#include "Dxf/ContactListenerComponent3D.h"
#include "Dxf/InputStateTracker.h"
#include "Toolbox/Platform.h"
#include "../../Tests/Support/FakeBackend.h"
#include <stdio.h>
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
#include "../../Source/Physics/Private/Dxf/WorldInteractionProbe.h"
#endif

namespace Dxf::Benchmark
{
namespace
{
// 配置と機能の有効状態。既存の系列と結果を混ぜない。
enum class EInteractionCase
{
	// 疎なSensor配置でイベントを生成しない基準。
	SensorOff,
	// 同じ疎なSensor配置でイベントを生成・配送する。
	SensorOn,
	// Solid同士の接触だけを生成・配送する。
	SolidOn,
	// 全形状が重なるSensor配置。真の組数を削減できない条件。
	SensorDense,
	// 動く床を使わないキャラクター経路の基準。
	StaticFloor,
	// 移動床と値のキャラクター計算、イベントなし。
	MovingFloorOff,
	// 同じ移動床とキャラクター計算、イベントあり。
	MovingFloorOn
};

// 配置名を機械処理しやすいCSVの値として返す。
const char* CaseName(EInteractionCase Case)
{
	switch (Case)
	{
	case EInteractionCase::SensorOff:
		return "sensor-off";
	case EInteractionCase::SensorOn:
		return "sensor-on";
	case EInteractionCase::SolidOn:
		return "solid-only";
	case EInteractionCase::SensorDense:
		return "sensor-dense";
	case EInteractionCase::StaticFloor:
		return "static-floor";
	case EInteractionCase::MovingFloorOff:
		return "moving-floor-off";
	case EInteractionCase::MovingFloorOn:
		return "moving-floor-on";
	}
	throw Toolbox::FException("Unknown interaction benchmark case");
}

// 時間合否を設けず、計測条件が壊れていた場合だけ失敗する。
void CheckCondition(bool bCondition, const char* Message)
{
	if (!bCondition)
	{
		throw Toolbox::FException(Message);
	}
}

#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
// 自己検査だけで使う決定論的な時計。実測ではNowNanosecondsを使う。
Toolbox::uint64 ProbeCheckNanoseconds = 0;
// 同じ検査だけで使う累計の確保件数。
Toolbox::uint64 ProbeCheckAllocations = 0;
// 入れ子区間の計算を実行時間の揺れなしで確かめる。
Toolbox::uint64 ReadProbeCheckNanoseconds()
{
	return ProbeCheckNanoseconds;
}
// 確保数を二重計上しないことを確かめる。
Toolbox::uint64 ReadProbeCheckAllocations()
{
	return ProbeCheckAllocations;
}
// 候補→詳細→候補の切替と明示終了が、既知の差分だけを各区間へ記録するか。
void CheckProbeIntervals()
{
	// 計測器自身の確保も、既存の本物の累計から確認する。
	const Toolbox::uint64 AllocationsBefore = TotalAllocations();
	ProbeCheckNanoseconds = 0;
	ProbeCheckAllocations = 0;
	PhysicsPrivate::FWorldInteractionProbe Probe(ReadProbeCheckNanoseconds, ReadProbeCheckAllocations);
	{
		PhysicsPrivate::FWorldInteractionProbe::FRegion Candidate(
		    PhysicsPrivate::FWorldInteractionProbe::EPhase::Candidate);
		ProbeCheckNanoseconds = 10;
		ProbeCheckAllocations = 1;
		PhysicsPrivate::FWorldInteractionProbe::CountCandidate_Internal();
		{
			PhysicsPrivate::FWorldInteractionProbe::FRegion Exact(
			    PhysicsPrivate::FWorldInteractionProbe::EPhase::Exact);
			ProbeCheckNanoseconds = 17;
			ProbeCheckAllocations = 3;
		}
		ProbeCheckNanoseconds = 22;
		ProbeCheckAllocations = 6;
		Candidate.Stop();
		// 終了後の値と二度目のStop・デストラクターは合計を変えない。
		ProbeCheckNanoseconds = 100;
		ProbeCheckAllocations = 100;
		Candidate.Stop();
	}
	CheckCondition(Probe.m_Values[0].Nanoseconds == 15 && Probe.m_Values[0].Allocations == 4 &&
	                   Probe.m_Values[0].Calls == 1,
	               "interaction probe candidate exclusion");
	CheckCondition(Probe.m_Values[1].Nanoseconds == 7 && Probe.m_Values[1].Allocations == 2 &&
	                   Probe.m_Values[1].Calls == 1,
	               "interaction probe exact interval");
	CheckCondition(Probe.m_Candidates == 1 && Probe.m_ClockReads == 4 && TotalAllocations() == AllocationsBefore,
	               "interaction probe count or allocation");
}
#endif

// 2Dの型とWorld接続。計測の処理順は次元間で共有する。
struct FInteraction2D
{
	using FWorld = FPhysicsWorld2D;
	using FBody = FBodyId2D;
	using FBodyDescription = FBodyDescription2D;
	using FColliderDescription = FColliderDescription2D;
	using FState = FCharacterState2D;
	using FSettings = FCharacterMoveSettings2D;
	using FInput = FCharacterMoveInput2D;
	using FListener = DContactListener2DComponent;
	using FNotice = FContactNotice2D;
	using FVector = Toolbox::FVector2;
	// 2Dの配置位置。
	static FVector Point(Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {X, Y};
	}
	// 接触・Sensorに使う半径0.5の形状。
	static FColliderDescription Ball()
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FCircle2D{{}, 0.5f};
		return Description;
	}
	// 上面をBody中心より0.25高くした床。
	static FColliderDescription Floor()
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOrientedBox2D{{}, {2, 0.25f}, 0};
		return Description;
	}
	// 正規の配送Componentが使う固定更新のWorld。
	static void Connect(FFixedTickContext& Context, FWorld& World)
	{
		Context.Physics2D = &World;
	}
};

// 3Dの型とWorld接続。球と箱の寸法は2Dに合わせる。
struct FInteraction3D
{
	using FWorld = FPhysicsWorld3D;
	using FBody = FBodyId3D;
	using FBodyDescription = FBodyDescription3D;
	using FColliderDescription = FColliderDescription3D;
	using FState = FCharacterState3D;
	using FSettings = FCharacterMoveSettings3D;
	using FInput = FCharacterMoveInput3D;
	using FListener = DContactListener3DComponent;
	using FNotice = FContactNotice3D;
	using FVector = Toolbox::FVector3;
	// 2Dと同じ平面上の位置。
	static FVector Point(Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {X, Y, 0};
	}
	// 接触・Sensorに使う半径0.5の球。
	static FColliderDescription Ball()
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FSphere{{}, 0.5f};
		return Description;
	}
	// キャラクターの周辺に十分な奥行きのある床。
	static FColliderDescription Floor()
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOBB{{}, {2, 0.25f, 2}};
		return Description;
	}
	// 正規の配送Componentが使う固定更新のWorld。
	static void Connect(FFixedTickContext& Context, FWorld& World)
	{
		Context.Physics3D = &World;
	}
};

// 一つの固定更新、または連続する固定更新の実測値。時間はナノ秒。
struct FInteractionTotals
{
	// Bodyへの速度設定と配送予約。
	Toolbox::uint64 PrepareNs = 0;
	// 床追従を含むStepCharacter全体。追従部分だけの時間ではない。
	Toolbox::uint64 CharacterNs = 0;
	// イベント生成を含むWorld.Step全体。
	Toolbox::uint64 WorldNs = 0;
	// FPostPhysicsStepQueueから実ContactListenerへの配送と受信関数。
	Toolbox::uint64 DeliveryNs = 0;
	// 診断の集計を除く固定更新全体。
	Toolbox::uint64 FixedNs = 0;
	// 速度設定・配送予約中の確保件数。
	Toolbox::uint64 PrepareAllocations = 0;
	// StepCharacter中の確保件数。
	Toolbox::uint64 CharacterAllocations = 0;
	// World.Step中の確保件数。
	Toolbox::uint64 WorldAllocations = 0;
	// 実配送中の確保件数。
	Toolbox::uint64 DeliveryAllocations = 0;
	// 実行した固定更新数。
	Toolbox::uint64 Steps = 0;
	// 床追従を行ったキャラクター更新数。
	Toolbox::uint64 Carried = 0;
	// 阻害・拒否で追従できなかった更新数。
	Toolbox::uint64 CarryStops = 0;
	// 全Stepで確定した組数の合計（平均を出すため）。
	Toolbox::uint64 Pairs = 0;
	// 確定した組数の最大。
	Toolbox::uint64 PeakPairs = 0;
	// 容量に関係なく観測された必要組数の最大。
	Toolbox::uint64 PeakRequiredPairs = 0;
	// 組容量に達したStep数。ちょうど上限の場合も含む。
	Toolbox::uint64 CapacityReached = 0;
	// 組容量を超えたStep数。
	Toolbox::uint64 Overflowed = 0;
	// 実際に受信関数を呼んだ回数。
	Toolbox::uint64 Delivered = 0;
	// Solver側の候補組数。イベント側の候補数とは異なる。
	Toolbox::uint64 SolverCandidates = 0;
	// 候補・詳細・差分・床追従の順の内部計測。通常版では未取得として出力する。
	Toolbox::uint64 PhaseNs[4]{};
	// 同じ各区間の確保件数。
	Toolbox::uint64 PhaseAllocations[4]{};
	// 境界が重なり候補として通知された組数。
	Toolbox::uint64 EventCandidates = 0;
	// 実際にEventTouchへ入った組数。
	Toolbox::uint64 EventExactTests = 0;
	// 専用計測に要した時計読出し回数。時間から差し引かない。
	Toolbox::uint64 ProbeClockReads = 0;
};

// 既存のWorldと実配送Componentを接続する局所的な測定場面。ゲーム用の基盤は追加しない。
template <typename T> class TInteractionScenario
{
public:
	TInteractionScenario(FAssetService& Assets, EInteractionCase Case, Toolbox::int32 Colliders, Toolbox::int32 Moving)
	    : m_Case(Case), m_ColliderCount(Colliders), m_MovingCount(Moving)
	{
		m_bFloor = Case == EInteractionCase::StaticFloor || Case == EInteractionCase::MovingFloorOff ||
		           Case == EInteractionCase::MovingFloorOn;
		m_bEvents = Case == EInteractionCase::SensorOn || Case == EInteractionCase::SolidOn ||
		            Case == EInteractionCase::SensorDense || Case == EInteractionCase::MovingFloorOn;
		// Solidの重力落下を負荷条件に混ぜない。地形と残りのBodyは明示的にStatic。
		m_World.SetGravity({});
		for (Toolbox::int32 Index = 0; Index < Colliders; ++Index)
		{
			// 指定数だけを動かす。Static床系列だけは移動数を0にする。
			typename T::FBodyDescription Body;
			Body.Type = Index < Moving && Case != EInteractionCase::StaticFloor
			                ? (Case == EInteractionCase::SolidOn ? EBodyType::Dynamic : EBodyType::Kinematic)
			                : EBodyType::Static;
			// 疎な配置は2Colliderずつの島、密集は全て原点。床の残りは遠方のStatic。
			const Toolbox::f32 X =
			    m_bFloor
			        ? (Index < Moving ? 6.0f * Index : 100000.0f + 6.0f * Index)
			        : (Case == EInteractionCase::SensorDense
			               ? 0.0f
			               : 4.0f * (Index / 2) + (Case == EInteractionCase::SolidOn ? 0.995f * (Index % 2) : 0.0f));
			Body.Position = T::Point(X, m_bFloor ? -0.25f : 0.0f);
			const auto Id = m_World.CreateBody(Body);
			auto Collider = m_bFloor ? T::Floor() : T::Ball();
			Collider.Response =
			    m_bFloor || Case == EInteractionCase::SolidOn ? EColliderResponse::Solid : EColliderResponse::Sensor;
			m_World.AttachCollider(Id, Collider);
			if (Index < Moving)
			{
				m_Moving.PushBack(Id);
				if (m_bFloor)
				{
					// キャラクターは値だけ。Collider総数を64／512／1024のまま保つ。
					typename T::FState State;
					State.Center = T::Point(X, 0.52f);
					m_Characters.PushBack(State);
				}
			}
		}
		// 密集条件の全組を保持できる容量。上限でイベントを削って速くしない。
		const Toolbox::uint32 DensePairs =
		    static_cast<Toolbox::uint32>(Moving * (Colliders - Moving) + Moving * (Moving - 1) / 2);
		m_MaxPairs = Case == EInteractionCase::SensorDense ? Toolbox::Max(1024u, DensePairs) : 1024u;
		FWorldEventSettings Events;
		Events.bEnabled = m_bEvents;
		Events.MaxPairs = m_MaxPairs;
		// イベント有効化の確保を、場面構築全体とは別に記録する。
		const Toolbox::uint64 Allocations = TotalAllocations();
		m_World.SetEventSettings(Events);
		m_EventSetupAllocations = TotalAllocations() - Allocations;
		if (m_bEvents)
		{
			// 一つの実Listenerが全移動Bodyを監視する。密集時は両当事者への通知も数える。
			auto Listener = m_ListenerOwner.template AddComponent<typename T::FListener>(false);
			CheckCondition(static_cast<bool>(Listener), "interaction benchmark listener creation");
			m_Listener = Listener.Value();
			for (const auto Body : m_Moving)
			{
				m_Listener.Get()->WatchBody(Body);
			}
			m_Listener.Get()->SetHandler(
			    [this](const typename T::FNotice& Notice)
			    {
				    // 配送された値を利用し、呼び出しを最適化で捨てられないようにする。
				    m_ReceivedStep = Notice.StepIndex;
				    ++m_Delivered;
			    });
			CheckCondition(static_cast<bool>(m_ListenerOwner.Initialize_Internal({Assets})),
			               "interaction benchmark listener initialization");
		}
	}
	~TInteractionScenario()
	{
		m_ListenerOwner.Shutdown_Internal();
	}
	// 固定更新の各公開境界を計測する。診断を読み出す時間は固定更新全体にも含めない。
	void Step(Toolbox::int64 StepIndex, FInteractionTotals& Totals)
	{
		// Component配送は正規の予約窓口を使い、World.Stepが成功してから呼ぶ。
		FFixedTickContext Context{m_Input.GetSnapshot(), m_Pending};
		Context.PostPhysicsStep = &m_Delivery;
		T::Connect(Context, m_World);
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
		// スタック上の固定長集計へ、既存benchmarkの読出し関数を接続する。
		PhysicsPrivate::FWorldInteractionProbe Probe(NowNanoseconds, TotalAllocations);
#endif
		const Toolbox::uint64 FixedBegin = NowNanoseconds();
		const Toolbox::uint64 PrepareAllocations = TotalAllocations();
		const Toolbox::uint64 PrepareBegin = NowNanoseconds();
		if (m_Case != EInteractionCase::StaticFloor)
		{
			// Sensorは小さく往復して重なりを保つ。床は常に2m/sで並進する。
			const Toolbox::f32 Speed =
			    m_bFloor ? 2.0f : static_cast<Toolbox::f32>(0.25 * Toolbox::Cos(0.1 * StepIndex));
			for (const auto Body : m_Moving)
			{
				m_World.SetVelocity(Body, T::Point(Speed, 0));
			}
		}
		if (m_bEvents)
		{
			CheckCondition(static_cast<bool>(m_ListenerOwner.FixedTick_Internal(Context)),
			               "interaction benchmark delivery reservation");
		}
		Totals.PrepareNs += NowNanoseconds() - PrepareBegin;
		Totals.PrepareAllocations += TotalAllocations() - PrepareAllocations;
		// 公開境界のStepCharacter全体と、専用計測の床追従だけの区間は分けて記録する。
		const Toolbox::uint64 CharacterAllocations = TotalAllocations();
		const Toolbox::uint64 CharacterBegin = NowNanoseconds();
		for (Toolbox::size_t Index = 0; Index < m_Characters.Size(); ++Index)
		{
			const typename T::FInput Input;
			const auto Result = StepCharacter(m_World, m_Settings, m_Characters[Index], Input, StepSeconds);
			m_Characters[Index] = Result.State;
			Totals.Carried += Result.bCarried ? 1 : 0;
			Totals.CarryStops += Result.bCarryBlocked || Result.bCarryRejected ? 1 : 0;
		}
		if (m_bFloor)
		{
			Totals.CharacterNs += NowNanoseconds() - CharacterBegin;
		}
		Totals.CharacterAllocations += TotalAllocations() - CharacterAllocations;
		const Toolbox::uint64 WorldAllocations = TotalAllocations();
		const Toolbox::uint64 WorldBegin = NowNanoseconds();
		m_World.Step(StepSeconds);
		Totals.WorldNs += NowNanoseconds() - WorldBegin;
		Totals.WorldAllocations += TotalAllocations() - WorldAllocations;
		const Toolbox::uint64 DeliveryAllocations = TotalAllocations();
		const Toolbox::uint64 DeliveredBefore = m_Delivered;
		const Toolbox::uint64 DeliveryBegin = NowNanoseconds();
		if (m_bEvents)
		{
			m_Delivery.Run_Internal(Context);
			Totals.DeliveryNs += NowNanoseconds() - DeliveryBegin;
		}
		Totals.DeliveryAllocations += TotalAllocations() - DeliveryAllocations;
		Totals.FixedNs += NowNanoseconds() - FixedBegin;
		++Totals.Steps;
		// 以下は計測区間の外。固定長集計と診断だけを読み、WorldのSnapshotは毎回作らない。
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
		for (Toolbox::size_t Phase = 0; Phase < 4; ++Phase)
		{
			Totals.PhaseNs[Phase] += Probe.m_Values[Phase].Nanoseconds;
			Totals.PhaseAllocations[Phase] += Probe.m_Values[Phase].Allocations;
		}
		Totals.EventCandidates += Probe.m_Candidates;
		Totals.EventExactTests += Probe.m_Values[1].Calls;
		Totals.ProbeClockReads += Probe.m_ClockReads;
		CheckCondition(Probe.m_Values[0].Calls == (m_bEvents ? 1u : 0u) &&
		                   Probe.m_Values[2].Calls == (m_bEvents ? 1u : 0u),
		               "interaction benchmark phase boundary count");
		CheckCondition(Probe.m_Values[1].Calls <= Probe.m_Candidates,
		               "interaction benchmark exact count exceeds candidates");
		CheckCondition(Probe.m_Values[3].Calls == m_Characters.Size(), "interaction benchmark carry boundary count");
#endif
		const auto& Batch = m_World.GetEventBatch();
		Totals.Pairs += Batch.PairCount;
		Totals.PeakPairs = Toolbox::Max(Totals.PeakPairs, static_cast<Toolbox::uint64>(Batch.PairCount));
		Totals.PeakRequiredPairs =
		    Toolbox::Max(Totals.PeakRequiredPairs, static_cast<Toolbox::uint64>(Batch.RequiredPairs));
		Totals.CapacityReached += m_bEvents && Batch.RequiredPairs >= m_MaxPairs ? 1 : 0;
		Totals.Overflowed += Batch.bOverflowed ? 1 : 0;
		Totals.Delivered += m_Delivered - DeliveredBefore;
		Totals.SolverCandidates += m_World.GetExecutionDiagnostics().CandidatePairCount;
		CheckCondition(!m_bEvents || Batch.bPublished, "interaction benchmark unpublished event batch");
		CheckCondition(!Batch.bOverflowed, "interaction benchmark pair capacity overflow");
		CheckCondition(m_Delivered == DeliveredBefore || m_ReceivedStep == Batch.StepIndex,
		               "interaction benchmark stale delivery");
		if (m_Case == EInteractionCase::SensorDense)
		{
			const Toolbox::uint32 ExpectedPairs = static_cast<Toolbox::uint32>(
			    m_MovingCount * (m_ColliderCount - m_MovingCount) + m_MovingCount * (m_MovingCount - 1) / 2);
			CheckCondition(Batch.PairCount == ExpectedPairs, "interaction benchmark dense pairs were lost");
			// 動く当事者には他の全Colliderから届く。配送が欠落した軽い計測を認めない。
			const Toolbox::uint64 ExpectedDeliveries =
			    static_cast<Toolbox::uint64>(m_MovingCount) * (m_ColliderCount - 1);
			CheckCondition(m_Delivered - DeliveredBefore == ExpectedDeliveries,
			               "interaction benchmark dense delivery was lost");
		}
		if (m_bFloor)
		{
			for (Toolbox::size_t Index = 0; Index < m_Characters.Size(); ++Index)
			{
				// 独立な解析値で床追従が続いていることを確認し、停止した軽い計測にしない。
				const Toolbox::f64 ExpectedX =
				    6.0 * Index + (m_Case == EInteractionCase::StaticFloor ? 0.0 : 2.0 * StepSeconds * (StepIndex + 1));
				CheckCondition(Toolbox::Abs(m_Characters[Index].Center.X - ExpectedX) < 0.03 &&
				                   m_Characters[Index].Ground.State == ECharacterGroundState::Walkable,
				               "interaction benchmark moving-ground trajectory");
			}
		}
	}
	// 生成時のイベント用領域の確保件数。
	Toolbox::uint64 EventSetupAllocations() const
	{
		return m_EventSetupAllocations;
	}
	// 表に記録する組の保持上限。
	Toolbox::uint32 MaxPairs() const
	{
		return m_MaxPairs;
	}
	// 無効時の組数は未観測としてNAを出力する。
	bool EventsEnabled() const
	{
		return m_bEvents;
	}
	// Colliderを登録しない値のキャラクター数。
	Toolbox::size_t CharacterCount() const
	{
		return m_Characters.Size();
	}

private:
	// 場面の種類。
	EInteractionCase m_Case;
	// 実際に登録したColliderの数。
	Toolbox::int32 m_ColliderCount;
	// 動く当事者または静止基準のキャラクター数。
	Toolbox::int32 m_MovingCount;
	// 床とキャラクターを使う配置か。
	bool m_bFloor = false;
	// イベント生成と実配送を有効にするか。
	bool m_bEvents = false;
	// イベントの保持上限。
	Toolbox::uint32 m_MaxPairs = 0;
	// 有効化時だけに要したイベントの確保件数。
	Toolbox::uint64 m_EventSetupAllocations = 0;
	// Body・Collider・イベントを所有するWorld。Listenerより先に構築し、後に破棄する。
	typename T::FWorld m_World;
	// 速度を設定するBody。Static床の基準では速度を設定しない。
	Toolbox::TVector<typename T::FBody> m_Moving;
	// キャラクターは値だけで計算し、追加のColliderは持たない。
	Toolbox::TVector<typename T::FState> m_Characters;
	// 既定の移動設定。
	typename T::FSettings m_Settings;
	// 実Listenerを所有するオブジェクト。
	DGameObject m_ListenerOwner;
	// 所有オブジェクト内の世代付き参照。
	TObjectHandle<typename T::FListener> m_Listener;
	// 成功したStepの後にだけ処理する既存の配送予約。
	FPostPhysicsStepQueue m_Delivery;
	// デバイスのない固定入力。
	FInputStateTracker m_Input;
	// 未配達入力は発生しない。
	Toolbox::TVector<FInputSnapshot> m_Pending;
	// 受信関数の実呼び出し回数。
	Toolbox::uint64 m_Delivered = 0;
	// 最後に配送されたStepの番号。
	Toolbox::uint64 m_ReceivedStep = 0;
};

// 区間の合計を更新する。時間の中央値は別途既存のSummarizeで計算する。
void AddTotals(FInteractionTotals& Total, const FInteractionTotals& Value)
{
	Total.PrepareNs += Value.PrepareNs;
	Total.CharacterNs += Value.CharacterNs;
	Total.WorldNs += Value.WorldNs;
	Total.DeliveryNs += Value.DeliveryNs;
	Total.FixedNs += Value.FixedNs;
	Total.PrepareAllocations += Value.PrepareAllocations;
	Total.CharacterAllocations += Value.CharacterAllocations;
	Total.WorldAllocations += Value.WorldAllocations;
	Total.DeliveryAllocations += Value.DeliveryAllocations;
	Total.Steps += Value.Steps;
	Total.Carried += Value.Carried;
	Total.CarryStops += Value.CarryStops;
	Total.Pairs += Value.Pairs;
	Total.PeakPairs = Toolbox::Max(Total.PeakPairs, Value.PeakPairs);
	Total.PeakRequiredPairs = Toolbox::Max(Total.PeakRequiredPairs, Value.PeakRequiredPairs);
	Total.CapacityReached += Value.CapacityReached;
	Total.Overflowed += Value.Overflowed;
	Total.Delivered += Value.Delivered;
	Total.SolverCandidates += Value.SolverCandidates;
	for (Toolbox::size_t Phase = 0; Phase < 4; ++Phase)
	{
		Total.PhaseNs[Phase] += Value.PhaseNs[Phase];
		Total.PhaseAllocations[Phase] += Value.PhaseAllocations[Phase];
	}
	Total.EventCandidates += Value.EventCandidates;
	Total.EventExactTests += Value.EventExactTests;
	Total.ProbeClockReads += Value.ProbeClockReads;
}

// 固定更新1回あたりへ正規化する。0は未取得の代用にしない。
Toolbox::f64 PerStep(Toolbox::uint64 Value, Toolbox::uint64 Steps)
{
	return static_cast<Toolbox::f64>(Value) / static_cast<Toolbox::f64>(Steps);
}

// FConsoleの既定の整数変換を避け、短い時間や平均確保数の小数部をCSVへ残す。
Toolbox::FString Decimal(Toolbox::f64 Value)
{
	// 書式変換は全ての計測区間が終わった後にだけ使う。
	char Text[64];
	snprintf(Text, sizeof(Text), "%.6f", Value);
	return Text;
}

// 1条件を初回・慣らし・5回の繰り返しで計り、CSVの1行にする。
template <typename T>
void RunCase(FAssetService& Assets, const char* Dimension, EInteractionCase Case, Toolbox::int32 Colliders,
             Toolbox::int32 Moving, Toolbox::int32 Warmup, Toolbox::int32 Measured)
{
	// World・Body・Collider・Listenerの初回構築。共通の疑似Backendは含めない。
	const Toolbox::uint64 BuildAllocations = TotalAllocations();
	const Toolbox::uint64 BuildBegin = NowNanoseconds();
	TInteractionScenario<T> Scenario(Assets, Case, Colliders, Moving);
	const Toolbox::uint64 BuildNs = NowNanoseconds() - BuildBegin;
	const Toolbox::uint64 BuildAllocated = TotalAllocations() - BuildAllocations;
	// 最初のStepの遅延確保も、慣らし後の値から分ける。
	FInteractionTotals Cold;
	Scenario.Step(0, Cold);
	Toolbox::int64 StepIndex = 1;
	FInteractionTotals Warm;
	for (Toolbox::int32 Index = 0; Index < Warmup; ++Index)
	{
		Scenario.Step(StepIndex++, Warm);
	}
	// 繰り返しの平均時間から、中央値と最小・最大を得る。
	Toolbox::f64 WorldUs[Repetitions]{};
	Toolbox::f64 CharacterUs[Repetitions]{};
	Toolbox::f64 DeliveryUs[Repetitions]{};
	Toolbox::f64 FixedUs[Repetitions]{};
	// 内部4区間も同じ5区間の中央値として記録する。
	Toolbox::f64 PhaseUs[4][Repetitions]{};
	FInteractionTotals All;
	for (Toolbox::int32 Repetition = 0; Repetition < Repetitions; ++Repetition)
	{
		FInteractionTotals Totals;
		for (Toolbox::int32 Index = 0; Index < Measured; ++Index)
		{
			Scenario.Step(StepIndex++, Totals);
		}
		WorldUs[Repetition] = PerStep(Totals.WorldNs, Totals.Steps) / 1000;
		CharacterUs[Repetition] = PerStep(Totals.CharacterNs, Totals.Steps) / 1000;
		DeliveryUs[Repetition] = PerStep(Totals.DeliveryNs, Totals.Steps) / 1000;
		FixedUs[Repetition] = PerStep(Totals.FixedNs, Totals.Steps) / 1000;
		for (Toolbox::size_t Phase = 0; Phase < 4; ++Phase)
		{
			PhaseUs[Phase][Repetition] = PerStep(Totals.PhaseNs[Phase], Totals.Steps) / 1000;
		}
		AddTotals(All, Totals);
	}
	// 既存benchmarkと同じ5回の集計関数を使う。
	const FSpread World = Summarize(WorldUs);
	const FSpread Character = Summarize(CharacterUs);
	const FSpread Delivery = Summarize(DeliveryUs);
	const FSpread Fixed = Summarize(FixedUs);
	Toolbox::Out << Dimension << ',' << CaseName(Case) << ',' << Colliders << ','
	             << (Case == EInteractionCase::StaticFloor ? 0 : Moving) << ',' << Scenario.CharacterCount() << ','
	             << (Scenario.EventsEnabled() ? "on" : "off") << ',' << Scenario.MaxPairs() << ',';
	Toolbox::Out << Decimal(static_cast<Toolbox::f64>(BuildNs) / 1000) << ',' << BuildAllocated << ','
	             << Scenario.EventSetupAllocations() << ',';
	Toolbox::Out << Decimal(static_cast<Toolbox::f64>(Cold.WorldNs) / 1000) << ',' << Cold.WorldAllocations << ','
	             << Decimal(static_cast<Toolbox::f64>(Cold.CharacterNs) / 1000) << ',' << Cold.CharacterAllocations
	             << ',' << Decimal(static_cast<Toolbox::f64>(Cold.DeliveryNs) / 1000) << ',' << Cold.DeliveryAllocations
	             << ',' << Cold.PrepareAllocations << ',';
	Toolbox::Out << Decimal(World.Median) << ',' << Decimal(World.Min) << ',' << Decimal(World.Max) << ','
	             << Decimal(PerStep(All.WorldAllocations, All.Steps)) << ',';
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
	for (Toolbox::size_t Phase = 0; Phase < 3; ++Phase)
	{
		Toolbox::Out << Decimal(Summarize(PhaseUs[Phase]).Median) << ','
		             << Decimal(PerStep(All.PhaseAllocations[Phase], All.Steps)) << ',';
	}
	Toolbox::Out << Decimal(PerStep(All.EventCandidates, All.Steps)) << ',';
#else
	Toolbox::Out << "NA,NA,NA,NA,NA,NA,NA,";
#endif
	Toolbox::Out << Decimal(Delivery.Median) << ',' << Decimal(Delivery.Min) << ',' << Decimal(Delivery.Max) << ','
	             << Decimal(PerStep(All.DeliveryAllocations, All.Steps)) << ',';
	Toolbox::Out << Decimal(Character.Median) << ',' << Decimal(Character.Min) << ',' << Decimal(Character.Max) << ','
	             << Decimal(PerStep(All.CharacterAllocations, All.Steps)) << ',';
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
	Toolbox::Out << Decimal(Summarize(PhaseUs[3]).Median) << ',' << Decimal(PerStep(All.PhaseAllocations[3], All.Steps))
	             << ',';
#else
	Toolbox::Out << "NA,NA,";
#endif
	Toolbox::Out << Decimal(Fixed.Median) << ',' << Decimal(PerStep(All.PrepareNs, All.Steps) / 1000) << ','
	             << Decimal(PerStep(All.PrepareAllocations, All.Steps)) << ',';
	if (Scenario.EventsEnabled())
	{
		Toolbox::Out << Decimal(PerStep(All.Pairs, All.Steps)) << ',' << All.PeakPairs << ',' << All.PeakRequiredPairs;
	}
	else
	{
		Toolbox::Out << "NA,NA,NA";
	}
	Toolbox::Out << ',' << All.CapacityReached << ',' << All.Overflowed << ','
	             << Decimal(PerStep(All.Delivered, All.Steps)) << ',' << Decimal(PerStep(All.Carried, All.Steps)) << ','
	             << All.CarryStops << ',' << Decimal(PerStep(All.SolverCandidates, All.Steps)) << ',';
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
	Toolbox::Out << Decimal(PerStep(All.EventExactTests, All.Steps)) << ','
	             << Decimal(PerStep(All.ProbeClockReads, All.Steps)) << ',';
	for (Toolbox::size_t Phase = 0; Phase < 4; ++Phase)
	{
		Toolbox::Out << Decimal(static_cast<Toolbox::f64>(Cold.PhaseNs[Phase]) / 1000) << ','
		             << Cold.PhaseAllocations[Phase] << ',';
	}
	Toolbox::Out << Cold.EventCandidates << ',' << Cold.EventExactTests << '\n';
#else
	Toolbox::Out << "NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA\n";
#endif
	fflush(stdout);
}
} // namespace

void RunInteractionSeries(Toolbox::int32 Warmup, Toolbox::int32 Measured)
{
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
	CheckProbeIntervals();
#endif
	// 描画・音声を起動せず、実Componentの初期化にだけ既存の疑似Backendを使う。
	Testing::FFakeBackend Backend;
	FAssetService Assets(Backend, Backend, Backend);
#if defined(NDEBUG)
	Toolbox::Out << "# compiled_configuration=Release\n";
#else
	Toolbox::Out << "# compiled_configuration=Debug (use Release for performance acceptance)\n";
#endif
	Toolbox::Out << "# interaction: cold=1, warmup=" << Warmup << ", measured=" << Measured
	             << ", repetitions=" << Repetitions << ", dt=1/60; CPU only; no timing thresholds\n";
	Toolbox::Out << "# exact collider counts include level/moving bodies; floor characters are unregistered "
	                "StepCharacter values; terrain is Static\n";
	Toolbox::Out << "# sparse sensors: two colliders per isolated location; dense sensors: all overlap, true "
	                "pairs=M*(N-M)+M*(M-1)/2\n";
	Toolbox::Out << "# delivery: one real ContactListener watches every moving body; both recipients are delivered for "
	                "moving/moving pairs\n";
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
	Toolbox::Out << "# private_phase_probes=on; probe_self_check=passed; private intervals use existing "
	                "clock/allocation readers; no timing overhead is subtracted\n";
	Toolbox::Out << "# event_candidate includes bounds/sort/scan/filter/Add and excludes nested EventTouch; exact is "
	                "EventTouch; difference is Publish including canonical sort\n";
	Toolbox::Out << "# carry_only includes eligibility/prediction/path-check/inheritance; static-floor includes "
	                "eligibility only; event_candidates precede collision mask filtering\n";
#else
	Toolbox::Out << "# private_phase_probes=off; internal phase columns are NA; use separate "
	                "DXF_INTERACTION_BENCHMARK_PROBES=ON build for decomposition\n";
#endif
	Toolbox::Out << "# character_with_carry measures complete StepCharacter calls; solver candidates are not event "
	                "candidates; disabled events have unobserved pair counts\n";
	Toolbox::Out
	    << "dim,case,colliders,moving_bodies,characters,events,max_pairs,build_us,build_alloc,event_setup_alloc,cold_"
	       "world_us,cold_world_alloc,cold_character_us,cold_character_alloc,cold_delivery_us,cold_delivery_alloc,cold_"
	       "prepare_alloc,world_us_median,world_us_min,world_us_max,world_alloc_per_step,event_candidate_us,event_"
	       "candidate_alloc,event_exact_us,event_exact_alloc,event_difference_us,event_difference_alloc,event_"
	       "candidates,delivery_us_median,delivery_us_min,delivery_us_max,delivery_alloc_per_step,character_with_carry_"
	       "us_median,character_with_carry_us_min,character_with_carry_us_max,character_alloc_per_step,carry_only_us,"
	       "carry_only_alloc,fixed_us_median,prepare_us_mean,prepare_alloc_per_step,pairs_per_step,peak_pairs,peak_"
	       "required_pairs,capacity_reached_steps,overflowed_steps,delivered_per_step,carried_per_step,carry_stops,"
	       "solver_candidates_per_step,event_exact_tests_per_step,probe_clock_reads_per_step,cold_event_candidate_us,"
	       "cold_event_candidate_alloc,cold_event_exact_us,cold_event_exact_alloc,cold_event_difference_us,cold_event_"
	       "difference_alloc,cold_carry_only_us,cold_carry_only_alloc,cold_event_candidates,cold_event_exact_tests\n";
	// R7の第一系列。静止床の基準を加えて未使用経路とも比較できるようにする。
	const Toolbox::int32 ColliderCounts[] = {64, 512, 1024};
	const Toolbox::int32 MovingCounts[] = {1, 16, 64};
	const EInteractionCase Cases[] = {EInteractionCase::SensorOff,    EInteractionCase::SensorOn,
	                                  EInteractionCase::SolidOn,      EInteractionCase::SensorDense,
	                                  EInteractionCase::StaticFloor,  EInteractionCase::MovingFloorOff,
	                                  EInteractionCase::MovingFloorOn};
	for (const EInteractionCase Case : Cases)
	{
		for (const Toolbox::int32 Colliders : ColliderCounts)
		{
			for (const Toolbox::int32 Moving : MovingCounts)
			{
				RunCase<FInteraction2D>(Assets, "2D", Case, Colliders, Moving, Warmup, Measured);
				RunCase<FInteraction3D>(Assets, "3D", Case, Colliders, Moving, Warmup, Measured);
			}
		}
	}
}
} // namespace Dxf::Benchmark
