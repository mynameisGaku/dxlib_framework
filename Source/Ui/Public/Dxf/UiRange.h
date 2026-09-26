// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_RANGE_H
#define DXF_UI_RANGE_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 有限区間の表示値。値の設定自体は操作イベントを発行しない。
 */
struct FUiRange
{
	/**
	 * 入力できる最小値。
	 */
	Toolbox::f64 Minimum = 0;
	/**
	 * 入力できる最大値（Minimumより大きい）。
	 */
	Toolbox::f64 Maximum = 1;
	/**
	 * 刻み。0は連続値。
	 */
	Toolbox::f64 Step = 0;
	/**
	 * 範囲内へ制限した現在値。
	 */
	Toolbox::f64 Value = 0;
	/**
	 * 範囲・刻みを検証して値を丸める。Step=0は連続値。
	 */
	void Configure(Toolbox::f64 Min, Toolbox::f64 Max, Toolbox::f64 Increment);
	/**
	 * 有限値を範囲内へ制限する。
	 */
	void Set(Toolbox::f64 Next);
	/**
	 * 表示に使う0〜1の割合。
	 */
	FORCEINLINE Toolbox::f64 Fraction() const noexcept
	{
		return (Value - Minimum) / (Maximum - Minimum);
	}
};
} // namespace Dxf
// namespace Dxf
#endif
