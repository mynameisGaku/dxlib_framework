// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_TREE_H
#define DXF_UI_TREE_H
#include "Dxf/UiElement.h"
namespace Dxf::Detail
{
struct FUiRootState;

/**
 * 要素の木の操作（親子の付け外し、接続・切断のフック、破棄の要求と境界での解放）。
 */
class FUiTree
{
public:
	/**
	 * 子として加える。接続済みの親なら子の木を接続する。
	 * @param State ルートの状態。
	 * @param Parent 親。
	 * @param Child 子。
	 * @param Index 挿入位置。
	 */
	TResult<void> AddChild(FUiRootState& State, DUiElement& Parent, DUiElement& Child, Toolbox::size_t Index);
	/**
	 * 親から取り外す。接続済みなら子の木を切断する。
	 * @param State ルートの状態。
	 * @param Element 要素。
	 */
	TResult<void> Remove(FUiRootState& State, DUiElement& Element);
	/**
	 * 要素と子孫の破棄を要求する（参照の無効化と切断はすぐ、解放は境界で）。
	 * @param State ルートの状態。
	 * @param Element 要素。
	 */
	void Destroy(FUiRootState& State, DUiElement& Element) noexcept;
	/**
	 * 破棄を要求した要素を解放する（出来事の配布中・レイアウト中は何もしない）。
	 * @param State ルートの状態。
	 */
	void FlushDestroyed(FUiRootState& State) noexcept;
	/**
	 * 全要素を切断する（窓口の破棄時）。
	 * @param State ルートの状態。
	 */
	void DetachAll(FUiRootState& State) noexcept;
	/**
	 * 要素の深さ（重なり領域の入れ物を0とする。親のない要素は0）。
	 * @param Element 要素。
	 */
	static Toolbox::size_t Depth(const DUiElement& Element) noexcept;
	/**
	 * 子孫の最大の深さ（自身を0とする）。
	 * @param Element 要素。
	 */
	static Toolbox::size_t SubtreeHeight(const DUiElement& Element) noexcept;
	/**
	 * 破棄を要求して未解放の要素の数。
	 */
	FORCEINLINE Toolbox::size_t GetPendingDestroyCount() const noexcept
	{
		return m_PendingDestroy.Size();
	}

private:
	/**
	 * 子の木を接続する（前順）。失敗したら、この接続で呼んだフックの逆順に切断して失敗を返す。
	 * @param State ルートの状態。
	 * @param Element 子の木の根。
	 */
	TResult<void> AttachSubtree_Internal(FUiRootState& State, DUiElement& Element);
	/**
	 * 子の木を切断する（後順）。
	 * @param State ルートの状態。
	 * @param Element 子の木の根。
	 */
	void DetachSubtree_Internal(FUiRootState& State, DUiElement& Element) noexcept;
	/**
	 * 一つの要素を切断する（フック・購読・キャプチャ・フォーカス・ホバーの解除）。
	 * @param State ルートの状態。
	 * @param Element 要素。
	 */
	void DetachOne_Internal(FUiRootState& State, DUiElement& Element) noexcept;
	/**
	 * 親の子の一覧から外す。
	 * @param Element 要素。
	 */
	static void Unlink_Internal(DUiElement& Element) noexcept;
	/**
	 * 破棄の印を子孫へ付ける。
	 * @param Element 要素。
	 */
	void MarkDestroyed_Internal(DUiElement& Element) noexcept;
	/**
	 * 破棄を要求した要素（解放待ち）。
	 */
	Toolbox::TVector<TUiRef<DUiElement>> m_PendingDestroy;
	/** 解放中のデストラクタ再入を遅延する。 */
	bool m_bFlushing = false;
	/** 予約の確保失敗時も破棄済み要素を次の境界で回収する。 */
	bool m_bScanDestroyed = false;
	/** 子孫から解放する。再帰の深さは木の上限以下。 */
	void ReleaseSubtree_Internal(FUiRootState& State, DUiElement& Element) noexcept;
	/**
	 * 切断・接続のフックを呼んでいる深さ。
	 */
	Toolbox::int32 m_WalkDepth = 0;
};
} // namespace Dxf::Detail
#endif
