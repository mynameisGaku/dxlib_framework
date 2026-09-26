// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SCROLL_VIEW_H
#define DXF_UI_SCROLL_VIEW_H
#include "Dxf/UiElement.h"
namespace Dxf
{
/**
 * 縦スクロールの入れ物。内容の配置・クリップ・残りホイール・つまみ操作を一か所で扱う。
 * 慣性は使用しない。ホイールの1ノッチは表示高の割合、または指定した論理距離。
 */
class DUiScrollView : public DUiElement
{
public:
	/**
	 * 縦方向のスクロール領域を作る。
	 */
	DUiScrollView();
	/**
	 * 未接続で同じRootの内容を設定する。古い内容は切断し、破棄はしない。
	 */
	void SetContent(const TUiRef<DUiElement>& Content);
	/**
	 * 論理距離の位置。配置済みなら端へ制限する。
	 */
	void SetScrollOffset(Toolbox::f64 Offset);
	/**
	 * 表示開始位置を、内容の先頭からの論理距離で返す。
	 */
	FORCEINLINE Toolbox::f64 GetScrollOffset() const noexcept
	{
		return m_Offset;
	}

	/**
	 * 内容と表示領域から求めた最大スクロール距離を返す。
	 */
	FORCEINLINE Toolbox::f64 GetScrollMaximum() const noexcept
	{
		return Toolbox::Max(0.0, m_ContentHeight - m_ViewHeight);
	}
	/**
	 * 1ノッチの送り。bRelative=trueなら表示高に対する割合。
	 */
	void SetWheelStep(Toolbox::f64 Amount, bool bRelative = true);
	/**
	 * スクロールバーの論理幅を返す。
	 */
	FORCEINLINE Toolbox::f32 GetScrollBarWidth() const noexcept
	{
		return m_BarWidth;
	}

	/**
	 * スクロール位置に対応するつまみの論理矩形を返す。
	 */
	FUiRect GetScrollThumbRect() const noexcept;

protected:
	/**
	 * 内容の測定結果を親へ返す。
	 * @param Context 測定の窓口。
	 * @param Available 割り当て可能な寸法。
	 */
	FUiSize OnMeasure(FUiLayoutContext& Context, FUiSize Available) override;
	/**
	 * 内容と操作部品を割り当て領域へ配置する。
	 * @param Context 配置の窓口。
	 * @param Content 内側余白を除く内容矩形。
	 */
	void OnArrange(FUiLayoutContext& Context, const FUiRect& Content) override;
	/**
	 * 内容より手前のつまみ・チェック等を記録する。
	 * @param Context 論理座標の描画窓口。
	 */
	void OnDrawOverlay(FUiDrawContext& Context) const override;
	/**
	 * Rootから届いたポインター操作を処理する。
	 * @param Event 処理済み・キャプチャ等を反映するイベント。
	 */
	void OnPointerEvent(FUiPointerEvent& Event) override;
	/**
	 * Rootから届いた方向・決定操作を処理する。
	 * @param Event 処理済みの状態を返すイベント。
	 */
	void OnNavigationEvent(FUiNavigationEvent& Event) override;
	/**
	 * サブクラスは同じスクロール操作へ仮想化した内容を接続する。
	 */
	virtual FUiSize MeasureScrollable(FUiLayoutContext& Context, FUiSize Available);
	/**
	 * スクロール位置を反映して内容を配置する。
	 * @param Context 配置の窓口。
	 * @param View 表示領域の論理矩形。
	 */
	virtual void ArrangeScrollable(FUiLayoutContext& Context, const FUiRect& View);
	FORCEINLINE void SetContentHeight_Internal(Toolbox::f64 Height) noexcept
	{
		m_ContentHeight = Height;
	}

private:
	TUiRef<DUiElement> m_Content;
	Toolbox::f64 m_Offset = 0;
	Toolbox::f64 m_ContentHeight = 0;
	Toolbox::f64 m_ViewHeight = 0;
	Toolbox::f64 m_WheelStep = 0.25;
	Toolbox::f32 m_BarWidth = 12;
	Toolbox::f32 m_GrabOffset = 0;
	bool m_bRelativeWheel = true;
	void DragTo_Internal(Toolbox::f32 Y);
};
} // namespace Dxf
// namespace Dxf
#endif
