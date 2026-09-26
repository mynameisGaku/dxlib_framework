// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/DxLibRenderBackend.h"
#include "Dxf/RenderSystem.h"
#include "DxLib.h"
namespace
{
// テスト用の値ハンドル。実GPU資源ではない。
void Release_Internal(void*, Toolbox::int32) noexcept
{
}

Dxf::FTexture MakeTexture_Internal()
{
	return Dxf::FTexture(Toolbox::MakeShared<Dxf::FTextureResource>(Dxf::FNativeHandle(401, nullptr, &Release_Internal),
	                                                                Dxf::FTextureMetadata{64, 32, false}));
}

Dxf::FTexturedQuad3D Quad_Internal()
{
	Dxf::FTexturedQuad3D Quad;
	Quad.Texture = MakeTexture_Internal();
	Quad.Corners = {Toolbox::FVector3{-1, 1, 0}, Toolbox::FVector3{1, 1, 0}, Toolbox::FVector3{1, -1, 0},
	                Toolbox::FVector3{-1, -1, 0}};
	Quad.bDoubleSided = false;
	Quad.Tint = {180, 140, 100, 128};
	return Quad;
}
} // namespace

TEST("native UI quad front side UV and opacity match world panel input convention")
{
	DxLib::ViewTrace = {};
	DxLib::TestPolygons3D = 0;
	Dxf::FDxLibRenderBackend Backend;
	Dxf::FRenderView3D View;
	View.Eye = {0, 0, -5};
	View.Target = {0, 0, 0};
	REQUIRE(Backend.BeginView3D(View));
	REQUIRE(Backend.DrawTexturedQuad3D(Quad_Internal()));
	REQUIRE(DxLib::TestPolygons3D == 2);
	REQUIRE(DxLib::TestQuadVertices[0].u == 0 && DxLib::TestQuadVertices[0].v == 0);
	REQUIRE(DxLib::TestQuadVertices[2].u == 1 && DxLib::TestQuadVertices[2].v == 1);
	REQUIRE(DxLib::TestQuadVertices[5].u == 0 && DxLib::TestQuadVertices[5].v == 1);
	REQUIRE(DxLib::TestQuadVertices[0].norm.z == -1);
	REQUIRE(DxLib::TestQuadVertices[0].dif.r == 180);
	REQUIRE(DxLib::TestQuadVertices[0].dif.a == 255);
	REQUIRE(DxLib::TestBlendAlpha == 128);
	REQUIRE(DxLib::ViewTrace.Lighting == FALSE);
	REQUIRE(Backend.EndView3D());
	REQUIRE(DxLib::ViewTrace.Lighting == TRUE);
}

TEST("native UI quad rejects backface and respects explicit double sided mode")
{
	DxLib::ViewTrace = {};
	DxLib::TestPolygons3D = 0;
	Dxf::FDxLibRenderBackend Backend;
	Dxf::FRenderView3D View;
	View.Eye = {0, 0, 5};
	View.Target = {0, 0, 0};
	REQUIRE(Backend.BeginView3D(View));
	auto Quad = Quad_Internal();
	REQUIRE(Backend.DrawTexturedQuad3D(Quad));
	REQUIRE(DxLib::TestPolygons3D == 0);
	Quad.bDoubleSided = true;
	REQUIRE(Backend.DrawTexturedQuad3D(Quad));
	REQUIRE(DxLib::TestPolygons3D == 2);
	REQUIRE(Backend.EndView3D());
}

TEST("native UI quad failures never present and restore managed view state")
{
	Toolbox::int32 Calls = 0;
	for (Toolbox::int32 Failure = 0; Failure <= Calls; ++Failure)
	{
		DxLib::ViewTrace = {};
		DxLib::TestPresentCount = 0;
		Dxf::FDxLibRenderBackend Backend;
		Dxf::FRenderSystem Render(Backend);
		REQUIRE(Render.BeginFrame(640, 480));
		Dxf::FRenderView3D View;
		View.Eye = {0, 0, -5};
		View.Target = {0, 0, 0};
		REQUIRE(Render.GetContext().Get3D().SetView(View));
		REQUIRE(Render.GetContext().Get3D().DrawTexturedQuad(Quad_Internal()));
		DxLib::ViewTrace.FailAt = Failure;
		const auto Result = Render.EndFrame();
		if (Failure == 0)
		{
			REQUIRE(Result);
			Calls = DxLib::ViewTrace.Calls;
			REQUIRE(Calls > 10);
		}
		else
		{
			REQUIRE(!Result);
			REQUIRE(DxLib::TestPresentCount == 0);
		}
		REQUIRE(DxLib::ViewTrace.Z3D == 0);
		REQUIRE(DxLib::ViewTrace.WriteZ3D == 0);
	}
}
