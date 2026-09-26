// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SLIDER_H
#define DXF_UI_SLIDER_H
#include "Dxf/UiElement.h"
#include "Dxf/UiSignal.h"
#include "Dxf/UiRange.h"
namespace Dxf
{
/**
 * 水平方向の値入力。ドラッグはRootのポインターキャプチャへ接続する。
 */
class DUiSlider : public DUiElement
{
public:
	/**
	 * 既定の0〜1の値を入力する横スライダーを作る。
	 */
	DUiSlider();
	/**
	 * 有限の範囲と刻みを設定する。最大値は最小値より大きくなければならない。
	 * @param Minimum 最小値。
	 * @param Maximum 最大値。
	 * @param Step 刻み。0は連続値、正の値は最小値を基準とする。
	 */
	void SetRange(Toolbox::f64 Minimum, Toolbox::f64 Maximum, Toolbox::f64 Step = 0);
	/**
	 * 表示用の設定ではイベントを起こさない。
	 */
	void SetValue(Toolbox::f64 Value);
	/**
	 * 現在の表示値を返す。
	 */
	FORCEINLINE Toolbox::f64 GetValue() const noexcept
	{
		return m_Range.Value;
	}

	/**
	 * 現在の範囲・刻み・値を読み取り専用で借用する。
	 */
	FORCEINLINE const FUiRange& GetRange() const noexcept
	{
		return m_Range;
	}

	/**
	 * 利用者操作による値の変更通知を借用する。SetValueからは発火しない。
	 */
	FORCEINLINE TUiSignal<Toolbox::f64>& OnValueChanged() noexcept
	{
		return m_Changed;
	}

	/**
	 * 現在の値に対応するつまみの論理矩形を返す。
	 */
	FUiRect GetThumbRect() const noexcept;

protected:
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
	 * 現在の表示値から描画命令を記録する。
	 * @param Context 論理座標の描画窓口。
	 */
	void OnDraw(FUiDrawContext& Context) const override;

private:
	void SetFromPointer_Internal(Toolbox::f32 X);
	void SetFromUser_Internal(Toolbox::f64 Value);
	/**
	 * 範囲と現在値。
	 */
	FUiRange m_Range;
	/**
	 * 利用者による変更の通知。
	 */
	TUiSignal<Toolbox::f64> m_Changed;
};
} // namespace Dxf
// namespace Dxf
#endif
