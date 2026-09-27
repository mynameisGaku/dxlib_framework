// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_WORLD_INTERACTION_PROBE_H
#define DXF_PRIVATE_WORLD_INTERACTION_PROBE_H
#include "Toolbox/Utility.h"

namespace Dxf::PhysicsPrivate
{
/**
 * 専用benchmarkビルドだけで使う、呼出しスレッドの固定長の区間集計。
 * 時計と確保数は既存benchmarkから受け取り、公開Worldや通常ビルドに計測APIを増やさない。
 */
class FWorldInteractionProbe
{
public:
	/**
	 * 入れ子の詳細判定を除外して集計する区間。
	 */
	enum class EPhase : Toolbox::uint8
	{
		/**
		 * 境界の作成・整列・候補走査・事前の絞り込み。
		 */
		Candidate,
		/**
		 * EventTouchによる実形状の判定。
		 */
		Exact,
		/**
		 * 確定した組の整列とBegin／Stay／Endの生成。
		 */
		Difference,
		/**
		 * 床追従の可否・運動予測・経路検査・離地速度の継承。
		 */
		Carry,
		/**
		 * Solver（直列の経路）の組の候補を、索引を現在の姿勢へ合わせて集め、並べるまで（BroadPhase）。
		 */
		SolverPairs,
		/**
		 * 集計中の区間がない状態と配列の要素数。
		 */
		Count
	};
	/**
	 * 一つの区間の合計。時間はナノ秒、確保は件数。
	 */
	struct FValues
	{
		/**
		 * 入れ子区間を除いた時計の差分の合計。
		 */
		Toolbox::uint64 Nanoseconds = 0;
		/**
		 * 同じ区間の確保の累計差分。
		 */
		Toolbox::uint64 Allocations = 0;
		/**
		 * 区間へ入った回数。
		 */
		Toolbox::uint64 Calls = 0;
	};
	/**
	 * 区間を開始し、終了時には外側の区間の時計を再開する。
	 */
	class FRegion
	{
	public:
		/**
		 * @param Phase 計測する区間。現在のprobeがない場合は何もしない。
		 */
		explicit FRegion(EPhase Phase) noexcept;
		/**
		 * 例外で抜けた場合も区間を閉じる。
		 */
		~FRegion();
		/**
		 * 区間を一度だけ閉じる。早期に終了点を指定する場合にも使う。
		 */
		void Stop() noexcept;
		FRegion(const FRegion&) = delete;
		FRegion& operator=(const FRegion&) = delete;

	private:
		/**
		 * この区間を記録する呼出し元のprobe。
		 */
		FWorldInteractionProbe* m_pProbe = nullptr;
		/**
		 * この区間を始める前に記録していた区間。
		 */
		EPhase m_Previous = EPhase::Count;
	};
	/**
	 * このスコープを呼出しスレッドの計測先にする。入れ子のprobeは元へ戻す。
	 * @param ReadNanoseconds 無確保で単調時計を読む既存benchmarkの関数。
	 * @param ReadAllocations 無確保で累計の確保件数を読む既存benchmarkの関数。
	 */
	FWorldInteractionProbe(Toolbox::uint64 (*ReadNanoseconds)(), Toolbox::uint64 (*ReadAllocations)()) noexcept;
	/**
	 * 呼出しスレッドの以前の計測先へ戻す。
	 */
	~FWorldInteractionProbe();
	FWorldInteractionProbe(const FWorldInteractionProbe&) = delete;
	FWorldInteractionProbe& operator=(const FWorldInteractionProbe&) = delete;
	/**
	 * 詳細判定へ進む前の、境界が重なった候補を一件数える。
	 */
	static void CountCandidate_Internal() noexcept;
	/**
	 * 区間番号ごとの合計。
	 */
	FValues m_Values[static_cast<Toolbox::size_t>(EPhase::Count)]{};
	/**
	 * 同Body・Static同士・境界非重複を除いた候補の総数。
	 */
	Toolbox::uint64 m_Candidates = 0;
	/**
	 * 計測に要した時計の読出し回数。補正で時間を引くためには使わない。
	 */
	Toolbox::uint64 m_ClockReads = 0;

private:
	/**
	 * 現区間を閉じて次の区間を始める。入れ子の時間・確保を外側に二重計上しない。
	 * @param Next 次の区間、または計測しないCount。
	 */
	void Switch_Internal(EPhase Next) noexcept;
	/**
	 * 以前のスコープの計測先。
	 */
	FWorldInteractionProbe* m_pPrevious = nullptr;
	/**
	 * 時計の読出し関数。
	 */
	Toolbox::uint64 (*m_pReadNanoseconds)() = nullptr;
	/**
	 * 確保の累計の読出し関数。
	 */
	Toolbox::uint64 (*m_pReadAllocations)() = nullptr;
	/**
	 * 現在記録している区間。
	 */
	EPhase m_Phase = EPhase::Count;
	/**
	 * 現区間を開始・再開した時刻。
	 */
	Toolbox::uint64 m_StartNanoseconds = 0;
	/**
	 * 現区間を開始・再開した時点の累計確保数。
	 */
	Toolbox::uint64 m_StartAllocations = 0;
};
} // namespace Dxf::PhysicsPrivate
#endif
