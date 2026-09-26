// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_LIST_VIEW_H
#define DXF_UI_LIST_VIEW_H
#include "Dxf/UiScrollView.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiListItem.h"
#include "Toolbox/Optional.h"
namespace Dxf
{
/**
 * 固定行高の仮想化リスト。表示行＋上下の余裕だけ部品を保持する。
 * 項目は値所有、選択は行番号でなく安定キー。SetItems／SetSelectedKeyでは操作を通知しない。
 */
class DUiListView : public DUiScrollView
{
public:
	/**
	 * 可視行を再利用する縦方向の一覧を作る。
	 */
	DUiListView();
	/**
	 * 項目を値として置き換える。キー0・重複キーは拒否し、既存の選択キーは生存する場合だけ維持する。
	 * @param Items 所有する項目一覧。表示反映だけでは選択イベントを起こさない。
	 */
	void SetItems(Toolbox::TVector<FUiListItem> Items);
	/**
	 * 各行の論理高さを設定する。有限の正の値だけを受ける。
	 * @param Height 行の高さ。
	 */
	void SetRowHeight(Toolbox::f32 Height);
	/**
	 * 既存の項目キーを表示選択にする。空は選択解除。操作イベントは発生しない。
	 * @param Key 存在する安定キー、または空。
	 */
	void SetSelectedKey(Toolbox::TOptional<Toolbox::uint64> Key);
	/**
	 * 選択中の項目キーを値で返す。行番号とは異なる。
	 */
	FORCEINLINE Toolbox::TOptional<Toolbox::uint64> GetSelectedKey() const noexcept
	{
		return m_Selected;
	}

	/**
	 * 保持するデータ項目の総数を返す。
	 */
	FORCEINLINE Toolbox::size_t GetItemCount() const noexcept
	{
		return m_Items.Size();
	}

	/**
	 * 実体を持つ行部品の数を返す。項目の総数とは異なる。
	 */
	FORCEINLINE Toolbox::size_t GetMaterializedRowCount() const noexcept
	{
		return m_Rows.Size();
	}

	/**
	 * 前後の再利用余裕を含む、先頭の行データ位置を返す。
	 */
	FORCEINLINE Toolbox::size_t GetFirstVisibleIndex() const noexcept
	{
		return m_First;
	}

	/**
	 * 利用者の選択操作だけを通知する発行元を借用する。
	 */
	FORCEINLINE TUiSignal<Toolbox::uint64>& OnSelectionChanged() noexcept
	{
		return m_SelectionChanged;
	}

protected:
	/**
	 * 可視範囲に必要な行を準備し、再利用した行へ現在の項目を結び付ける。
	 */
	void OnPrepareLayout() override;
	/**
	 * スクロール内容の望む大きさを求める。
	 * @param Context 測定の窓口。
	 * @param Available 割り当て可能な論理寸法。
	 */
	FUiSize MeasureScrollable(FUiLayoutContext& Context, FUiSize Available) override;
	/**
	 * スクロール位置を反映して内容を配置する。
	 * @param Context 配置の窓口。
	 * @param View 表示領域の論理矩形。
	 */
	void ArrangeScrollable(FUiLayoutContext& Context, const FUiRect& View) override;
	/**
	 * Rootから届いた方向・決定操作を処理する。
	 * @param Event 処理済みの状態を返すイベント。
	 */
	void OnNavigationEvent(FUiNavigationEvent& Event) override;

private:
	/**
	 * 再利用する行。購読はキーではなくこの行のIDから現在の項目を解決する。
	 */
	struct FRow
	{
		TUiRef<DUiButton> Element;
		FUiSubscription Click;
		Toolbox::uint64 Key = 0;
	};
	Toolbox::TVector<FUiListItem> m_Items;
	Toolbox::TVector<FRow> m_Rows;
	Toolbox::TOptional<Toolbox::uint64> m_Selected;
	Toolbox::f32 m_RowHeight = 28;
	Toolbox::size_t m_First = 0;
	TUiSignal<Toolbox::uint64> m_SelectionChanged;
	void ActivateRow_Internal(Toolbox::uint64 Id);
	void RefreshStyles_Internal();
};
} // namespace Dxf
// namespace Dxf
#endif
