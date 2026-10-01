// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ContentView2D.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
bool IsValidContentView2D(const FContentView2D& View) noexcept
{
	return Toolbox::IsFinite(View.Origin.X) && Toolbox::IsFinite(View.Origin.Y) &&
	       Toolbox::IsFinite(View.PixelsPerMeter) && View.PixelsPerMeter > 0 &&
	       (!View.bClip || (View.ClipRect.Right > View.ClipRect.Left && View.ClipRect.Bottom > View.ClipRect.Top));
}
} // namespace Dxf
