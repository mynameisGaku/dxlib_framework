// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_PHYSICS_WORLD_EVENT_TRACKER_H
#define DXF_PRIVATE_PHYSICS_WORLD_EVENT_TRACKER_H
#include "Dxf/WorldEvent.h"
#include "QueryResultOrder.h"
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
#include "WorldInteractionProbe.h"
#endif
#include "Toolbox/Utility.h"
namespace Dxf::PhysicsPrivate
{
/**
 * 前回と今回の確定集合からBegin／Stay／Endを作る、2D／3D共通の記録。
 * 各組のA＜Bを前提に、確定集合を無確保でCollider番号の辞書順へ並べてから、線形の併合で差を求める。
 * 領域は有効化の時点で確保し、Stepごとの追加・差の生成・発行では確保しない。
 */
template <typename TColliderId, typename TVector> class TWorldEventTracker
{
public:
	/**
	 * 確定集合の一つの組。
	 */
	struct FPair
	{
		/**
		 * スロット番号が小さい方。
		 */
		TColliderId A;
		/**
		 * スロット番号が大きい方。
		 */
		TColliderId B;
		/**
		 * 種類。
		 */
		EWorldEventKind Kind = EWorldEventKind::Contact;
		/**
		 * 求められた場合だけのBからAへ向く法線。
		 */
		Toolbox::TOptional<TVector> Normal;
	};
	/**
	 * 一つの状態遷移。
	 */
	using FEvent = TWorldEvent<TColliderId, TVector>;
	/**
	 * 設定を検証する。不正な値は例外で通知する。
	 * @param Settings 新しい設定。
	 */
	static void Validate(const FWorldEventSettings& Settings)
	{
		if (Settings.MaxPairs < 1 || Settings.MaxPairs > 1048576u)
		{
			throw Toolbox::FException("Invalid world event pair capacity");
		}
		if (!Toolbox::IsFinite(Settings.ContactMargin) || Settings.ContactMargin < 0 || Settings.ContactMargin > 10)
		{
			throw Toolbox::FException("Invalid world event contact margin");
		}
	}
	/**
	 * 設定を適用する。有効なら全ての領域を先に確保してから入れ替える（確保の失敗では以前の状態を保つ）。
	 * 適用後は以前の組の記録を捨て、次のバッチをbResetにする。無効なら領域を解放する。
	 * @param Settings 検証済みの設定。
	 */
	void Configure(const FWorldEventSettings& Settings)
	{
		Validate(Settings);
		Toolbox::TVector<FPair> Previous;
		Toolbox::TVector<FPair> Current;
		Toolbox::TVector<FEvent> Events;
		if (Settings.bEnabled)
		{
			Previous.Reserve(Settings.MaxPairs);
			Current.Reserve(Settings.MaxPairs);
			Events.Reserve(static_cast<Toolbox::size_t>(Settings.MaxPairs) * 2);
		}
		// ここから例外を出さない。
		m_Settings = Settings;
		m_Previous.Swap(Previous);
		m_Current.Swap(Current);
		m_Batch.Events.Swap(Events);
		m_Batch.bPublished = false;
		m_Batch.bOverflowed = false;
		m_Batch.PairCount = 0;
		m_Batch.RequiredPairs = 0;
		m_bResetPending = true;
		m_Required = 0;
	}
	/**
	 * 有効か。
	 */
	FORCEINLINE bool IsEnabled() const noexcept
	{
		return m_Settings.bEnabled;
	}
	/**
	 * 現在の設定。
	 */
	FORCEINLINE const FWorldEventSettings& GetSettings() const noexcept
	{
		return m_Settings;
	}
	/**
	 * 直前に発行したバッチ。
	 */
	FORCEINLINE const TWorldEventBatch<FEvent>& GetBatch() const noexcept
	{
		return m_Batch;
	}
	/**
	 * Stepの開始時に、前回のバッチを未発行にする（途中で失敗したStepの後に古いバッチを今回のものに見せない）。
	 */
	void BeginStep() noexcept
	{
		m_Batch.bPublished = false;
		m_Batch.bOverflowed = false;
		m_Batch.PairCount = 0;
		m_Batch.RequiredPairs = 0;
		m_Batch.Events.Clear();
		m_Current.Clear();
		m_Required = 0;
	}
	/**
	 * 今回の確定集合へ重複しない組を追加する。各組はA＜Bとし、組同士の順序は問わない。上限を超えた組は数だけ数える。
	 * @param Pair 追加する組。
	 */
	void Add(const FPair& Pair) noexcept
	{
		++m_Required;
		if (m_Current.Size() < m_Settings.MaxPairs)
		{
			m_Current.PushBack(Pair);
		}
	}
	/**
	 * 前回と今回の差をバッチとして発行し、今回の集合を次回の比較元にする。
	 * 上限を超えたStepはイベントを発行せず、比較元を保つ。
	 * @param StepIndex 成功したStepの通算番号。
	 * @param EndReason 前回の組が今回ない理由を返す関数（Removed／FilterChanged／Separated）。
	 */
	template <typename TReason> void Publish(Toolbox::uint64 StepIndex, TReason&& EndReason) noexcept
	{
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
		// 確定組の整列から差分バッチの発行までを一つの区間にする。
		FWorldInteractionProbe::FRegion DifferenceProbe(FWorldInteractionProbe::EPhase::Difference);
#endif
		m_Batch.bPublished = true;
		m_Batch.StepIndex = StepIndex;
		m_Batch.BatchId += 1;
		m_Batch.bReset = m_bResetPending;
		m_Batch.RequiredPairs = static_cast<Toolbox::uint32>(m_Required);
		if (m_Required > m_Settings.MaxPairs)
		{
			m_Batch.bOverflowed = true;
			m_Batch.PairCount = static_cast<Toolbox::uint32>(m_Previous.Size());
			return;
		}
		// 候補の走査順に依存しない正準順へ、予約済み領域の中だけで並べる。
		HeapSort_Internal(m_Current.Data(), m_Current.Size(),
		                  [](const FPair& A, const FPair& B)
		                  {
			                  return Compare_Internal(A, B) < 0;
		                  });
		m_bResetPending = false;
		m_Batch.PairCount = static_cast<Toolbox::uint32>(m_Current.Size());
		Toolbox::size_t Old = 0;
		Toolbox::size_t New = 0;
		while (Old < m_Previous.Size() || New < m_Current.Size())
		{
			const Toolbox::int32 Order = Old >= m_Previous.Size()  ? 1
			                             : New >= m_Current.Size() ? -1
			                                                       : Compare_Internal(m_Previous[Old], m_Current[New]);
			if (Order < 0)
			{
				Emit_Internal(m_Previous[Old], EWorldEventPhase::End, EndReason(m_Previous[Old]));
				++Old;
			}
			else if (Order > 0)
			{
				Emit_Internal(m_Current[New], EWorldEventPhase::Begin, EWorldEventEndReason::None);
				++New;
			}
			else
			{
				const FPair& Before = m_Previous[Old];
				const FPair& After = m_Current[New];
				if (Before.A == After.A && Before.B == After.B && Before.Kind == After.Kind)
				{
					Emit_Internal(After, EWorldEventPhase::Stay, EWorldEventEndReason::None);
				}
				else
				{
					// 同じスロットの組でも、世代が違えば別の組（削除・再生成）。種類の変更はEnd→Begin。
					const EWorldEventEndReason Reason = Before.A == After.A && Before.B == After.B
					                                        ? EWorldEventEndReason::FilterChanged
					                                        : EndReason(Before);
					Emit_Internal(Before, EWorldEventPhase::End, Reason);
					Emit_Internal(After, EWorldEventPhase::Begin, EWorldEventEndReason::None);
				}
				++Old;
				++New;
			}
		}
		m_Previous.Swap(m_Current);
	}

private:
	/**
	 * スロット番号の辞書順で比べる。
	 */
	static Toolbox::int32 Compare_Internal(const FPair& Left, const FPair& Right) noexcept
	{
		if (Left.A.Index != Right.A.Index)
		{
			return Left.A.Index < Right.A.Index ? -1 : 1;
		}
		if (Left.B.Index != Right.B.Index)
		{
			return Left.B.Index < Right.B.Index ? -1 : 1;
		}
		return 0;
	}
	/**
	 * イベントを予約済みの領域へ追加する（容量は上限の2倍で、超えない）。
	 */
	void Emit_Internal(const FPair& Pair, EWorldEventPhase Phase, EWorldEventEndReason Reason) noexcept
	{
		FEvent Event;
		Event.Kind = Pair.Kind;
		Event.Phase = Phase;
		Event.EndReason = Reason;
		Event.ColliderA = Pair.A;
		Event.ColliderB = Pair.B;
		if (Phase != EWorldEventPhase::End && Pair.Kind == EWorldEventKind::Contact)
		{
			Event.Normal = Pair.Normal;
		}
		m_Batch.Events.PushBack(Event);
	}
	/**
	 * 現在の設定。
	 */
	FWorldEventSettings m_Settings;
	/**
	 * 前回の確定集合（スロット番号の辞書順）。
	 */
	Toolbox::TVector<FPair> m_Previous;
	/**
	 * 今回の確定集合。
	 */
	Toolbox::TVector<FPair> m_Current;
	/**
	 * 直前に発行したバッチ。
	 */
	TWorldEventBatch<FEvent> m_Batch;
	/**
	 * 今回見つけた組の総数（上限を超えた分を含む）。
	 */
	Toolbox::size_t m_Required = 0;
	/**
	 * 次のバッチをbResetにするか。
	 */
	bool m_bResetPending = true;
};
} // namespace Dxf::PhysicsPrivate
#endif
