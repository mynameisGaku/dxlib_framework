// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/DxLibRenderBackend.h"
#include "Dxf/RenderSystem.h"
#include "../Source/DxLibSupport/Private/Dxf/RenderPass3D.h"
#include "DxLib.h"
namespace
{
using namespace Dxf;
FGeometryCommand3D NativeTriangle_Internal(Toolbox::uint8 Alpha)
{
	FGeometryCommand3D Command;
	Command.Geometry.Triangles.PushBack({{0,0,1},{1,0,1},{0,1,1}});
	Command.Options.Color = {80,120,160,Alpha};
	return Command;
}
}
TEST("native opaque writes depth while planned transparency only tests depth")
{
	DxLib::ViewTrace = {};
	FRenderView3D View;
	View.Debug.Lighting = ELightingMode3D::Unlit;
	Toolbox::TVector<FPreparedGeometry3D> Prepared;
	Prepared.PushBack(PrepareGeometry3D(NativeTriangle_Internal(128), View).Value());
	Prepared.PushBack(PrepareGeometry3D(NativeTriangle_Internal(255), View).Value());
	auto Plan = Detail::BuildRenderPasses3D_Internal(Prepared, 0, Prepared.Size(), View);
	REQUIRE(Plan);
	REQUIRE(Plan.Value().Size() == 2);
	FDxLibRenderBackend Backend;
	REQUIRE(Backend.BeginView3D(View));
	REQUIRE(Backend.DrawGeometry3D(Plan.Value()[0]));
	REQUIRE(DxLib::ViewTrace.Z3D == TRUE);
	REQUIRE(DxLib::ViewTrace.WriteZ3D == TRUE);
	REQUIRE(Backend.DrawGeometry3D(Plan.Value()[1]));
	REQUIRE(DxLib::ViewTrace.Z3D == TRUE);
	REQUIRE(DxLib::ViewTrace.WriteZ3D == FALSE);
	REQUIRE(Backend.EndView3D());
	REQUIRE(DxLib::ViewTrace.Z3D == FALSE && DxLib::ViewTrace.WriteZ3D == FALSE);
}
TEST("native overlay Always neither tests nor writes depth")
{
	DxLib::ViewTrace = {};
	FRenderView3D View;
	auto Command = NativeTriangle_Internal(128);
	Command.Options.Depth = EDepthMode3D::Always;
	Toolbox::TVector<FPreparedGeometry3D> Prepared;
	Prepared.PushBack(PrepareGeometry3D(Command, View).Value());
	auto Plan = Detail::BuildRenderPasses3D_Internal(Prepared, 0, 1, View);
	REQUIRE(Plan && Plan.Value().Size() == 1);
	FDxLibRenderBackend Backend;
	REQUIRE(Backend.BeginView3D(View));
	REQUIRE(Backend.DrawGeometry3D(Plan.Value()[0]));
	REQUIRE(DxLib::ViewTrace.Z3D == FALSE && DxLib::ViewTrace.WriteZ3D == FALSE);
	REQUIRE(Backend.EndView3D());
}
TEST("native transparency frame returns to two dimensional rendering")
{
	DxLib::ViewTrace = {};
	DxLib::TestPresentCount = 0;
	FDxLibRenderBackend Backend;
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640,480));
	REQUIRE(Renderer.GetContext().Get3D().Submit(NativeTriangle_Internal(128)));
	REQUIRE(Renderer.GetContext().Get2D().DrawLine({0,0},{20,20}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(DxLib::ViewTrace.Triangles3D == 1);
	REQUIRE(DxLib::ViewTrace.Order.Back() == 2);
	REQUIRE(DxLib::ViewTrace.Z3D == FALSE && DxLib::ViewTrace.WriteZ3D == FALSE);
	REQUIRE(DxLib::TestPresentCount == 1);
}
