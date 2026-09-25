// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_QUERY_DIAGNOSTICS_H
#define DXF_WORLD_QUERY_DIAGNOSTICS_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * World問い合わせの種別ごとの集計（診断用）。問い合わせの集計は、診断を有効にした間だけ加算する。
 */
struct FWorldQueryTypeCounters
{
	/**
	 * 入力・状態・除外IDの検査を通った呼出しの回数。
	 */
	Toolbox::uint64 Queries = 0;
	/**
	 * 索引を使わず、全Colliderを順に調べた（総当たり）呼出しの回数。
	 */
	Toolbox::uint64 FallbackQueries = 0;
	/**
	 * 訪問した索引のノード数（内部ノードと葉）。
	 */
	Toolbox::uint64 NodesVisited = 0;
	/**
	 * 候補になったColliderの数（索引の葉、総当たりでは生存しているすべて）。
	 */
	Toolbox::uint64 Candidates = 0;
	/**
	 * カテゴリと自己除外を通り、形状の詳細判定をしたColliderの数。
	 */
	Toolbox::uint64 NarrowTests = 0;
};
/**
 * 2D／3DのWorld問い合わせの索引の状態と集計（診断用）。物理の状態・結果には影響しない。
 */
struct FWorldQueryDiagnostics
{
	/**
	 * Colliderのスロット数（削除済みの再利用待ちを含む）。
	 */
	Toolbox::uint64 ColliderSlots = 0;
	/**
	 * 生存しているColliderの数。
	 */
	Toolbox::uint64 AliveColliders = 0;
	/**
	 * 索引に入っているColliderの数。
	 */
	Toolbox::uint64 IndexedColliders = 0;
	/**
	 * 現在の姿勢のWorld形状が無効で索引に入れられないColliderの数（問い合わせの対象なら総当たりで処理する）。
	 */
	Toolbox::uint64 UnindexedColliders = 0;
	/**
	 * 使用中の索引ノード数。
	 */
	Toolbox::uint64 IndexNodes = 0;
	/**
	 * 索引の木の高さ（葉は0、空は0）。
	 */
	Toolbox::uint64 IndexHeight = 0;
	/**
	 * 索引が保持する領域のバイト数（確保済みの容量）。
	 */
	Toolbox::uint64 IndexMemoryBytes = 0;
	/**
	 * 索引への追加の累計。
	 */
	Toolbox::uint64 IndexInserts = 0;
	/**
	 * 索引からの削除の累計。
	 */
	Toolbox::uint64 IndexRemovals = 0;
	/**
	 * 姿勢の変更で境界を確認した回数の累計。
	 */
	Toolbox::uint64 IndexRefreshes = 0;
	/**
	 * 確認の結果、余裕を持たせた境界から出たため入れ直した回数の累計。
	 */
	Toolbox::uint64 IndexReinserts = 0;
	/**
	 * 総当たりになった理由: 索引の使用を無効にしている。
	 */
	Toolbox::uint64 FallbackDisabled = 0;
	/**
	 * 総当たりになった理由: 問い合わせ・登録の座標が安全に扱える範囲（絶対値2^100）を超える。
	 */
	Toolbox::uint64 FallbackRange = 0;
	/**
	 * 総当たりになった理由: 索引に入れられないColliderが、今回のカテゴリ・自己除外の対象に含まれる。
	 */
	Toolbox::uint64 FallbackUnindexed = 0;
	/**
	 * 線分の問い合わせ（RaycastClosest、半径0のSweepClosestを含む）。
	 */
	FWorldQueryTypeCounters Raycast;
	/**
	 * 円／球のスイープ（SweepClosest、SweepClosestIgnoringInitialContacts）。
	 */
	FWorldQueryTypeCounters Sweep;
	/**
	 * 接触の取得（QueryContacts）。
	 */
	FWorldQueryTypeCounters Contacts;
	/**
	 * 範囲の重なり（OverlapAll）。
	 */
	FWorldQueryTypeCounters Overlap;
};
} // namespace Dxf
#endif
