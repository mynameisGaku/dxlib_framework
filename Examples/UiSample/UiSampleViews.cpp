// SPDX-License-Identifier: NOASSERTION
#include "UiSampleViews.h"
namespace Dxf::UiSample
{
Toolbox::TVector<FRenderView3D> MakeSampleViews(bool bSplit)
{
	Toolbox::TVector<FRenderView3D> Views;
	for (Toolbox::int32 I = 0; I < (bSplit ? 2 : 1); ++I)
	{
		FRenderView3D View;
		View.Eye = {6, 7, -18};
		View.Target = {6, 1, 0};
		View.Id = 1;
		View.bViewport = bSplit;
		View.Viewport = {I * 640, 0, (I + 1) * 640, 720};
		View.LightDirection = {0, -1, 1};
		Views.PushBack(View);
	}
	return Views;
}

FUiWorldToScreen2D MakeSampleTransform2D(bool bSplit, Toolbox::int32 Index)
{
	FUiWorldToScreen2D Transform;
	Transform.Origin = {static_cast<Toolbox::f32>(bSplit ? Index * 640 + 120 : 220), 580};
	Transform.PixelsPerUnit = bSplit ? 35.0f : 60.0f;
	return Transform;
}
} // namespace Dxf::UiSample
