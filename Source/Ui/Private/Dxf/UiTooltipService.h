// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_TOOLTIP_SERVICE_H
#define DXF_UI_TOOLTIP_SERVICE_H
#include "Dxf/UiElement.h"
#include "Dxf/UiLabel.h"
namespace Dxf::Detail
{
struct FUiRootState;

/**
 * ツールチップの表示（ルートに一つ）。対象の文字を表示し、ツールチップ領域の中で表示面の端に収める。
 */
class DUiTooltipView final : public DUiLabel
{
public:
	DUiTooltipView();
	/**
	 * 基準点（ポインターの位置、ルートの論理座標）。
	 */
	FVector2 Anchor;
};

/**
 * ツールチップ領域の入れ物。子を基準点の右下に置き、はみ出す場合は左・上へ寄せる。
 */
class DUiTooltipLayer final : public DUiElement
{
public:
	DUiTooltipLayer();

protected:
	/**
	 * 基準点と表示面の範囲から位置を決める。
	 */
	void OnArrange(FUiLayoutContext& Context, const FUiRect& Content) override;
};

/**
 * ポインターが同じ要素に留まった時間でツールチップを出し、対象の消失・押下・移動で消す。
 */
class FUiTooltipService
{
public:
	/**
	 * 一フレームの更新。
	 * @param State ルートの状態。
	 * @param DeltaSeconds 前回からの秒数。
	 */
	void Update(FUiRootState& State, Toolbox::f64 DeltaSeconds);
	/**
	 * 消す（押下・キャプチャ等）。次に別の要素へ移るまで出さない。
	 * @param State ルートの状態。
	 */
	void Hide(FUiRootState& State) noexcept;
	/**
	 * 切断した要素が対象なら消す。
	 * @param State ルートの状態。
	 * @param Element 要素。
	 */
	void OnElementDetached(FUiRootState& State, DUiElement& Element) noexcept;
	/**
	 * 表示中の対象。
	 */
	FORCEINLINE DUiElement* GetTarget() const noexcept
	{
		return m_bShown ? m_Target.Get() : nullptr;
	}
	/**
	 * 表示に使う要素（なければnullptr）。
	 */
	FORCEINLINE DUiTooltipView* GetView() const noexcept
	{
		return m_View.Get();
	}

private:
	/**
	 * ツールチップの文字を持つ、ホバー中の最も近い要素（自身か祖先）。
	 * @param State ルートの状態。
	 */
	static DUiElement* FindCandidate_Internal(FUiRootState& State) noexcept;
	/**
	 * 対象。
	 */
	TUiRef<DUiElement> m_Target;
	/**
	 * 表示に使う要素。
	 */
	TUiRef<DUiTooltipView> m_View;
	/**
	 * 対象の上に留まった秒数。
	 */
	Toolbox::f64 m_Hover = 0;
	/**
	 * 表示中か。
	 */
	bool m_bShown = false;
	/**
	 * 消した後、別の要素へ移るまで出さないか。
	 */
	bool m_bSuppressed = false;
};
} // namespace Dxf::Detail
#endif
