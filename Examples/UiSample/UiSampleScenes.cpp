// SPDX-License-Identifier: NOASSERTION
#include "UiSampleScenes.h"
#include "UiTitleScene.h"
#include "UiPlay2DScene.h"
#include "UiPlay3DScene.h"
namespace Dxf::UiSample
{
Toolbox::TUniquePtr<DScene> MakeTitleScene(Toolbox::TSharedPtr<FUiSampleState> State)
{
	return Toolbox::MakeUnique<DUiTitleScene>(Toolbox::Move(State));
}

Toolbox::TUniquePtr<DScene> MakePlayScene(bool b3D, Toolbox::TSharedPtr<FUiSampleState> State)
{
	if (b3D)
	{
		return Toolbox::MakeUnique<DUiPlay3DScene>(Toolbox::Move(State));
	}
	return Toolbox::MakeUnique<DUiPlay2DScene>(Toolbox::Move(State));
}
} // namespace Dxf::UiSample
