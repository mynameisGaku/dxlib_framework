// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_PROGRESS_BAR_H
#define DXF_UI_PROGRESS_BAR_H
#include "Dxf/UiElement.h"
#include "Dxf/UiRange.h"
namespace Dxf
{
/**
 * 操作を受けない進捗表示。同じ範囲規則をSliderと共有する。
 */
class DUiProgressBar : public DUiElement
{
public:
	DUiProgressBar();
	/**
	 * 有限の範囲と刻みを設定する。最大値は最小値より大きくなければならない。
	 * @param Minimum 最小値。
	 * @param Maximum 最大値。
	 * @param Step 刻み。0は連続値、正の値は最小値を基準とする。
	 */
	void SetRange(Toolbox::f64 Minimum, Toolbox::f64 Maximum);
	void SetValue(Toolbox::f64 Value);
	/**
	 * 現在の表示値を返す。
	 */
	FORCEINLINE Toolbox::f64 GetValue() const noexcept
	{
		return m_Range.Value;
	}

protected:
	/**
	 * 現在の表示値から描画命令を記録する。
	 * @param Context 論理座標の描画窓口。
	 */
	void OnDraw(FUiDrawContext& Context) const override;

private:
	/**
	 * 表示する割合。
	 */
	FUiRange m_Range;
};
} // namespace Dxf
// namespace Dxf
#endif
