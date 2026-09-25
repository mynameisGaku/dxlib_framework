// SPDX-License-Identifier: NOASSERTION
// DxLibSupportだけを使う外部の利用者（dxf::support）。終了コード0が成功。
#include "Dxf/RenderQueue2D.h"
#include "Dxf/ModelImport.h"
#include "Dxf/ViewCoordinates.h"
#include "Toolbox/SegmentIntersection.h"
int main()
{
	if (Dxf::ImportFbxModel(nullptr, 0))
	{
		return 2;
	}
	Dxf::FRenderView3D View;
	const auto Point = Dxf::ProjectWorldToScreen(View, 640, 480, {0, 0, 0});
	const auto Ray = Dxf::MakeViewPickSegment(View, 640, 480, {320, 240});
	if (!Point || !Point.Value().bInsideView || Point.Value().Screen.X != 320 || !Ray || !Ray.Value())
	{
		return 3;
	}
	if (!Toolbox::IntersectSegment(Ray.Value()->Start, Ray.Value()->End, Toolbox::FSphere{{0, 0, 0}, 1}))
	{
		return 4;
	}
	if (!Toolbox::IntersectSegment(Ray.Value()->Start, Ray.Value()->End, Toolbox::FOBB{{0, 0, 0}, {1, 1, 1}}))
	{
		return 5;
	}
	Dxf::FRenderQueue2D Queue;
	return Queue.Submit(Dxf::FRectangleCommand{}) ? 1 : 0;
}
