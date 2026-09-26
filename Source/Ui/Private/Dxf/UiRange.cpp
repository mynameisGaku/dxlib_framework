// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiRange.h"
namespace Dxf
{
// 新しい範囲はすべて検証してから反映する。
void FUiRange::Configure(Toolbox::f64 Min, Toolbox::f64 Max, Toolbox::f64 Increment)
{
	if (!Toolbox::IsFinite(Min) || !Toolbox::IsFinite(Max) || !(Max > Min) || !Toolbox::IsFinite(Max - Min) ||
	    !Toolbox::IsFinite(Increment) || Increment < 0)
	{
		throw Toolbox::FException("Invalid UI value range");
	}
	FUiRange Next{Min, Max, Increment, Value};
	Next.Set(Value);
	*this = Next;
}
// 刻みは最小値を基点とする。範囲の端も必ず選択できる。
void FUiRange::Set(Toolbox::f64 Next)
{
	if (!Toolbox::IsFinite(Next))
	{
		throw Toolbox::FException("Invalid UI value");
	}
	Next = Toolbox::Clamp(Next, Minimum, Maximum);
	if (Step > 0 && Next != Maximum && Next != Minimum)
	{
		const Toolbox::f64 Units = (Next - Minimum) / Step;
		if (Toolbox::IsFinite(Units))
		{
			Next = Minimum + Toolbox::Floor(Units + 0.5) * Step;
		}
	}
	Value = Toolbox::Clamp(Next, Minimum, Maximum);
}
} // namespace Dxf
// namespace Dxf
