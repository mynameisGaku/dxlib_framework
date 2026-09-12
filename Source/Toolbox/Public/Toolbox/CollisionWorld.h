// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_COLLISION_WORLD_H
#define TOOLBOX_COLLISION_WORLD_H
#include "Toolbox/CollisionShapes.h"
#include "Toolbox/UniquePtr.h"
namespace Toolbox
{
/**
 * 登録した衝突形状を識別する、世代付きの非所有ハンドル。
 */
struct FColliderId
{
	/**
	 * 登録先ワールドの識別子。
	 */
	uint64 World = 0;
	/**
	 * 登録スロットの番号。
	 */
	size_t Index = 0;
	/**
	 * 同じスロットを再使用した際の世代。
	 */
	uint64 Generation = 0;
	/**
	 * 同じ登録を指すか調べる。
	 */
	bool operator==(const FColliderId&) const = default;
};
/**
 * 両側のカテゴリとマスクが一致した組だけを判定するフィルター。
 */
struct FCollisionFilter
{
	/**
	 * 自分が属するカテゴリのビット。
	 */
	uint32 Category = 1;
	/**
	 * 判定を許可する相手カテゴリのビット。
	 */
	uint32 Mask = 0xffffffffU;
};
/**
 * 衝突している二つの登録。順序違いの重複は返さない。
 */
struct FCollisionPair
{
	/**
	 * 一方の登録。
	 */
	FColliderId First;
	/**
	 * もう一方の登録。
	 */
	FColliderId Second;
};
/**
 * 内部で自動選択された空間分割方式。
 */
enum class ESpatialIndex
{
	/**
	 * 登録が少ない場合の直接比較。
	 */
	Direct,
	/**
	 * 平面的な分布をXY平面で四分割する。
	 */
	Quadtree,
	/**
	 * 立体的な分布を三軸で八分割する。
	 */
	Octree
};
/**
 * 問い合わせの候補数と、実際に行った詳細判定の数。
 */
struct FCollisionStats
{
	/**
	 * 選択された分割方式。
	 */
	ESpatialIndex Index = ESpatialIndex::Direct;
	/**
	 * 構築済みノード数。
	 */
	size_t Nodes = 0;
	/**
	 * 境界箱まで一致した候補数。
	 */
	size_t Candidates = 0;
	/**
	 * 実形状で比較した回数。
	 */
	size_t NarrowTests = 0;
};
/**
 * 衝突形状と空間分割を所有する。追加・更新・削除は次の問い合わせへ自動反映する。
 * 単一スレッドで使用し、形状はワールド座標で渡す。剛体応答は行わない。
 */
class FCollisionWorld
{
public:
	/**
	 * 空のワールドを作る。
	 */
	FCollisionWorld();
	/**
	 * 登録形状と空間分割を破棄する。
	 */
	~FCollisionWorld();
	/**
	 * 登録IDの混同を避けるためコピーを禁止する。
	 */
	FCollisionWorld(const FCollisionWorld&) = delete;
	/**
	 * 登録IDの混同を避けるためコピー代入を禁止する。
	 */
	FCollisionWorld& operator=(const FCollisionWorld&) = delete;
	/**
	 * 形状を所有して登録する。不正な形状は例外で通知する。
	 * @param Shape 対象の衝突形状。
	 * @param Filter 双方向に適用するカテゴリ条件。
	 */
	FColliderId Add(FCollisionShape Shape, FCollisionFilter Filter = {});
	/**
	 * 形状とフィルターを置き換える。期限切れIDはfalseを返す。
	 * @param Id 登録を識別する世代付きID。
	 * @param Shape 対象の衝突形状。
	 * @param Filter 双方向に適用するカテゴリ条件。
	 */
	bool Update(FColliderId Id, FCollisionShape Shape, FCollisionFilter Filter = {});
	/**
	 * 登録を削除し、そのIDを失効させる。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool Remove(FColliderId Id) noexcept;
	/**
	 * 指定形状と交差する登録を返す。結果に重複はない。
	 * @param Shape 対象の衝突形状。
	 * @param Filter 双方向に適用するカテゴリ条件。
	 */
	TVector<FColliderId> Query(const FCollisionShape& Shape, FCollisionFilter Filter = {});
	/**
	 * 全登録の衝突ペアを、空間分割で絞り込んで返す。
	 */
	TVector<FCollisionPair> FindPairs();
	/**
	 * 直前の問い合わせ統計を返す。
	 */
	const FCollisionStats& GetStats() const noexcept;

private:
	/**
	 * 実装とキャッシュの所有領域。
	 */
	struct FImpl;
	/**
	 * 公開ヘッダーから隠した登録データ。
	 */
	TUniquePtr<FImpl> m_pImpl;
};
} // namespace Toolbox
#endif
