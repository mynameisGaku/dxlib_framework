// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/RenderSystem.h"
#include "Dxf/DxLibRenderBackend.h"
#include "DxLib.h"
TEST("native new 2D primitives preserve outline and fill flags")
{
	DxLib::ViewTrace = {};
	Dxf::FDxLibRenderBackend Backend;
	Dxf::FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	auto& Draw = Renderer.GetContext().Get2D();
	REQUIRE(Draw.DrawRectangle({1,2,3,4}));
	REQUIRE(Renderer.Flush());
	REQUIRE(DxLib::TestBoxFill == FALSE);
	REQUIRE(Draw.DrawCircle({10, 10}, 3));
	REQUIRE(Renderer.Flush());
	REQUIRE(DxLib::ViewTrace.LastFill == FALSE);
	REQUIRE(Draw.FillCircle({10, 10}, 3));
	REQUIRE(Draw.FillTriangle({0,0},{1,0},{0,1}));
	REQUIRE(Draw.DrawLine({1,1},{2,2}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(DxLib::ViewTrace.LastFill == TRUE);
	REQUIRE(DxLib::ViewTrace.Circles2D == 2);
	REQUIRE(DxLib::ViewTrace.Triangles2D == 1);
	REQUIRE(DxLib::ViewTrace.Lines2D == 1);
}
TEST("native wire box draws twelve edges before 2D and restores managed state")
{
	DxLib::ViewTrace = {};
	Dxf::FDxLibRenderBackend Backend;
	Dxf::FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	Dxf::FRenderView3D View;
	View.Debug.Surface = Dxf::ESurfaceMode3D::Wireframe;
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	REQUIRE(Renderer.GetContext().Get2D().DrawLine({0,0},{20,20}));
	REQUIRE(Renderer.GetContext().Get3D().DrawBox(Toolbox::FOBB{}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(DxLib::ViewTrace.Lines3D == 12);
	REQUIRE(DxLib::ViewTrace.Triangles3D == 0);
	REQUIRE(DxLib::ViewTrace.DepthClears == 1);
	REQUIRE(DxLib::ViewTrace.Order.Back() == 2);
	REQUIRE(DxLib::ViewTrace.Z3D == 0);
	REQUIRE(DxLib::ViewTrace.WriteZ3D == 0);
	REQUIRE(DxLib::ViewTrace.Lighting == TRUE);
}
TEST("native view setup failures run cleanup and reject presentation")
{
	for (Toolbox::int32 Failure = 1; Failure <= 6; ++Failure)
	{
		DxLib::ViewTrace = {};
		DxLib::TestPresentCount = 0;
		Dxf::FDxLibRenderBackend Backend;
		Dxf::FRenderSystem Renderer(Backend);
		REQUIRE(Renderer.BeginFrame(640, 480));
		REQUIRE(Renderer.GetContext().Get3D().DrawBox(Toolbox::FOBB{}));
		DxLib::ViewTrace.FailAt = Failure;
		REQUIRE(!Renderer.EndFrame());
		REQUIRE(DxLib::TestPresentCount == 0);
		REQUIRE(DxLib::ViewTrace.Lighting == TRUE);
		REQUIRE(DxLib::ViewTrace.Z3D == 0);
	}
}
TEST("native solid with edges renders both complete box topology sets")
{
	DxLib::ViewTrace = {};
	Dxf::FDxLibRenderBackend Backend;
	Dxf::FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	Dxf::FRenderView3D View;
	View.Debug.Surface = Dxf::ESurfaceMode3D::SolidWithEdges;
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	REQUIRE(Renderer.GetContext().Get3D().DrawBox(Toolbox::FOBB{}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(DxLib::ViewTrace.Triangles3D == 12);
	REQUIRE(DxLib::ViewTrace.Lines3D == 12);
}
