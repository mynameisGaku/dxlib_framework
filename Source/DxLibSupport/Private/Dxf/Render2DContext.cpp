// SPDX-License-Identifier: NOASSERTION
#include "Dxf/Render2DContext.h"
namespace Dxf
{
TResult<void> FRender2DContext::DrawRectangle(FIntRect Rectangle, const FDrawStyle& Options)
{
	FRectangleCommand Command{Rectangle, Options};
	Command.bFilled = false;
	return Submit(Toolbox::Move(Command));
}
}
