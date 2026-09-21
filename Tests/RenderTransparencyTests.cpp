// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/RenderSystem.h"
namespace
{
using namespace Dxf;
// 実際にBackendへ渡ったプリミティブ順と深度を観察する値。
struct FObservedPrimitive
{
	Toolbox::uint8 Tag = 0;
	Toolbox::f32 Z = 0;
	EDepthMode3D Depth = EDepthMode3D::TestAndWrite;
	Toolbox::uint64 View = 0;
	bool bLine = false;
};
// Rendererと計画器は本体、画面出力だけ記録用に置き換える。
class FTransparencyBackend final : public IRenderBackend
{
public:
	Toolbox::TVector<FObservedPrimitive> m_Primitives;
	Toolbox::uint64 m_View = 0;
	Toolbox::uint32 m_Begins = 0;
	Toolbox::uint32 m_Ends = 0;
	Toolbox::uint32 m_Presentations = 0;
	Toolbox::uint32 m_Resets = 0;
	Toolbox::int32 m_FailDraw = -1;
	Toolbox::int32 m_Draws = 0;
	TResult<void> SetTarget(Toolbox::int32, Toolbox::int32, Toolbox::int32) override
	{
		return {};
	}
	TResult<void> Clear(FColor) override
	{
		return {};
	}
	TResult<void> ResetState(Toolbox::int32, Toolbox::int32) override
	{
		++m_Resets;
		return {};
	}
	TResult<void> DrawSprite(const FSpriteCommand&) override
	{
		return {};
	}
	TResult<void> DrawText(const FTextCommand&) override
	{
		return {};
	}
	TResult<void> DrawRectangle(const FRectangleCommand&) override
	{
		return {};
	}
	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}
	TResult<void> BeginView3D(const FRenderView3D& View) override
	{
		m_View = View.Id;
		++m_Begins;
		return {};
	}
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D& Geometry) override
	{
		if (m_Draws++ == m_FailDraw)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "expected draw failure");
		}
		for (const auto& Triangle : Geometry.Triangles)
		{
			m_Primitives.PushBack({Triangle.Color.R, Triangle.Triangle.A.Z, Triangle.Depth, m_View, false});
		}
		for (const auto& Line : Geometry.Lines)
		{
			m_Primitives.PushBack({Line.Color.R, Line.Line.Start.Z, Line.Depth, m_View, true});
		}
		return {};
	}
	TResult<void> EndView3D() override
	{
		++m_Ends;
		return {};
	}
	TResult<void> Present() override
	{
		++m_Presentations;
		return {};
	}
};
// 遠さの期待値が手で追える、カメラ正面を向いた非縮退三角形。
FGeometryCommand3D MakeTriangle_Internal(Toolbox::f32 Z, Toolbox::uint8 Tag, Toolbox::uint8 Alpha = 128,
	Toolbox::f32 X = 0)
{
	FGeometryCommand3D Command;
	Command.Geometry.Triangles.PushBack({{X, 0, Z}, {X + 1, 0, Z}, {X, 1, Z}});
	Command.Options.Color = {Tag, 80, 100, Alpha};
	return Command;
}
void Begin_Internal(FRenderSystem& Renderer)
{
	REQUIRE(Renderer.BeginFrame(640, 480));
	FRenderView3D View;
	View.Id = 1;
	View.Eye = {0, 0, 0};
	View.Target = {0, 0, 1};
	View.Debug.Lighting = ELightingMode3D::Unlit;
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
}
}
TEST("opaque scene runs before transparency regardless of submission order")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	auto& Draw = Renderer.GetContext().Get3D();
	REQUIRE(Draw.Submit(MakeTriangle_Internal(3, 1)));
	REQUIRE(Draw.Submit(MakeTriangle_Internal(4, 2, 255)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives.Size() == 2);
	REQUIRE(Backend.m_Primitives[0].Tag == 2);
	REQUIRE(Backend.m_Primitives[1].Tag == 1);
}
TEST("transparency sorts across commands and within a mesh from back to front")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	auto Near = MakeTriangle_Internal(2, 1);
	Near.Geometry.Triangles.PushBack({{0,0,9}, {1,0,9}, {0,1,9}});
	REQUIRE(Renderer.GetContext().Get3D().Submit(Toolbox::Move(Near)));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(5, 2)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives.Size() == 3);
	REQUIRE(Backend.m_Primitives[0].Z == 9);
	REQUIRE(Backend.m_Primitives[1].Z == 5);
	REQUIRE(Backend.m_Primitives[2].Z == 2);
}
TEST("transparent default depth tests without writing and zero alpha never draws")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(2, 1, 128)));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(1, 2, 0)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives.Size() == 1);
	REQUIRE(Backend.m_Primitives[0].Depth == EDepthMode3D::TestOnly);
}
TEST("sorting uses camera forward depth rather than radial distance")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(2, 1, 128, 100)));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(5, 2)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives[0].Tag == 2);
	REQUIRE(Backend.m_Primitives[1].Tag == 1);
}
TEST("equal transparent depths keep original input order")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	for (Toolbox::uint8 Tag = 1; Tag <= 5; ++Tag)
	{
		REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(3, Tag)));
	}
	REQUIRE(Renderer.EndFrame());
	for (Toolbox::size_t Index = 0; Index < 5; ++Index)
	{
		REQUIRE(Backend.m_Primitives[Index].Tag == Index + 1);
	}
}
TEST("transparent lines and triangles share one depth order")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	FDrawStyle3D Style;
	Style.Color = {2, 80, 100, 128};
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(2, 1)));
	REQUIRE(Renderer.GetContext().Get3D().DrawLine({0,0,8}, {1,0,8}, Style));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(5, 3)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives.Size() == 3);
	REQUIRE(Backend.m_Primitives[0].bLine);
	REQUIRE(Backend.m_Primitives[1].Z == 5);
	REQUIRE(Backend.m_Primitives[2].Z == 2);
}
TEST("view changes and explicit flushes are sorting barriers")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(2, 1)));
	REQUIRE(Renderer.Flush());
	FRenderView3D View;
	View.Id = 2;
	View.Debug.Lighting = ELightingMode3D::Unlit;
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(8, 2)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives[0].Tag == 1);
	REQUIRE(Backend.m_Primitives[1].Tag == 2);
	REQUIRE(Backend.m_Primitives[0].View == 1);
	REQUIRE(Backend.m_Primitives[1].View == 2);
	REQUIRE(Backend.m_Begins == 2);
	REQUIRE(Backend.m_Ends == 2);
}
TEST("overlay is last and never writes depth even with opaque color")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	auto Overlay = MakeTriangle_Internal(4, 3, 255);
	Overlay.Options.Layer = ERenderLayer3D::Overlay;
	REQUIRE(Renderer.GetContext().Get3D().Submit(Toolbox::Move(Overlay)));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(3, 2)));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(5, 1, 255)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives.Size() == 3);
	REQUIRE(Backend.m_Primitives[0].Tag == 1);
	REQUIRE(Backend.m_Primitives[1].Tag == 2);
	REQUIRE(Backend.m_Primitives[2].Tag == 3);
	REQUIRE(Backend.m_Primitives[2].Depth == EDepthMode3D::TestOnly);
}
TEST("Always depth is an input ordered overlay not camera sorted")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	for (Toolbox::uint8 Tag = 1; Tag <= 3; ++Tag)
	{
		auto Command = MakeTriangle_Internal(static_cast<Toolbox::f32>(Tag), Tag);
		Command.Options.Depth = EDepthMode3D::Always;
		REQUIRE(Renderer.GetContext().Get3D().Submit(Toolbox::Move(Command)));
	}
	REQUIRE(Renderer.EndFrame());
	for (Toolbox::size_t Index = 0; Index < 3; ++Index)
	{
		REQUIRE(Backend.m_Primitives[Index].Tag == Index + 1);
		REQUIRE(Backend.m_Primitives[Index].Depth == EDepthMode3D::Always);
	}
}
TEST("solid plus edges waits for scene transparency then draws all edges")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	FRenderView3D View;
	View.Debug.Lighting = ELightingMode3D::Unlit;
	View.Debug.Surface = ESurfaceMode3D::SolidWithEdges;
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(2, 1)));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(5, 2)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives.Size() == 8);
	REQUIRE(!Backend.m_Primitives[0].bLine && !Backend.m_Primitives[1].bLine);
	REQUIRE(Backend.m_Primitives[0].Z == 5);
	for (Toolbox::size_t Index = 2; Index < 8; ++Index)
	{
		REQUIRE(Backend.m_Primitives[Index].bLine);
		REQUIRE(Backend.m_Primitives[Index].Depth == EDepthMode3D::TestOnly);
	}
}
TEST("zero alpha also suppresses generated edges without activating the view")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	FRenderView3D View;
	View.Debug.Surface = ESurfaceMode3D::SolidWithEdges;
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(2, 1, 0)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives.IsEmpty());
	REQUIRE(Backend.m_Begins == 0 && Backend.m_Ends == 0);
}
TEST("invalid layer and invisible malformed geometry do not corrupt accepted work")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(3, 1)));
	auto Invalid = MakeTriangle_Internal(5, 2);
	Invalid.Options.Layer = static_cast<ERenderLayer3D>(999);
	REQUIRE(!Renderer.GetContext().Get3D().Submit(Toolbox::Move(Invalid)));
	auto Invisible = MakeTriangle_Internal(6, 3, 0);
	Invisible.Geometry.Triangles[0].B = Invisible.Geometry.Triangles[0].A;
	REQUIRE(!Renderer.GetContext().Get3D().Submit(Toolbox::Move(Invisible)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives.Size() == 1 && Backend.m_Primitives[0].Tag == 1);
}
TEST("camera direction is captured per serial even when view id repeats")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(1, 1)));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(9, 2)));
	FRenderView3D View;
	View.Id = 1;
	View.Eye = {0,0,10};
	View.Target = {0,0,0};
	View.Debug.Lighting = ELightingMode3D::Unlit;
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(1, 3)));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(9, 4)));
	REQUIRE(Renderer.EndFrame());
	const Toolbox::uint8 Expected[] = {2, 1, 3, 4};
	REQUIRE(Backend.m_Begins == 2);
	for (Toolbox::size_t Index = 0; Index < 4; ++Index)
	{
		REQUIRE(Backend.m_Primitives[Index].Tag == Expected[Index]);
	}
}
TEST("large f32 camera positions use finite f64 sort keys")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	FRenderView3D View;
	View.Eye = {1e30f,0,0};
	View.Target = {2e30f,0,0};
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	FDrawStyle3D Style;
	Style.Color = {1,2,3,128};
	REQUIRE(Renderer.GetContext().Get3D().DrawLine({2e30f,0,0},{2e30f,1,0}, Style));
	Style.Color.R = 2;
	REQUIRE(Renderer.GetContext().Get3D().DrawLine({3e30f,0,0},{3e30f,1,0}, Style));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives[0].Tag == 2 && Backend.m_Primitives[1].Tag == 1);
}
TEST("parallel generation and serial generation have the same transparency order")
{
	Toolbox::TVector<FObservedPrimitive> Reference;
	for (Toolbox::uint32 Lanes = 1; Lanes <= 4; Lanes *= 2)
	{
		Toolbox::FJobSystem Jobs(Lanes);
		FTransparencyBackend Backend;
		FRenderSystem Renderer(Backend);
		REQUIRE(Renderer.GetContext().SetExecutionJobs_Internal(&Jobs));
		Begin_Internal(Renderer);
		REQUIRE(Renderer.GetContext().Get3D().SubmitGenerated(200,
			[](Toolbox::size_t Index, FGeometryCommand3D& Command) -> TResult<void>
			{
				Toolbox::FThread::Yield();
				Command = MakeTriangle_Internal(static_cast<Toolbox::f32>(Index % 7),
					static_cast<Toolbox::uint8>(Index), Index % 3 == 0 ? 255 : 128);
				return {};
			}, 1));
		REQUIRE(Renderer.EndFrame());
		if (Lanes == 1)
		{
			Reference = Backend.m_Primitives;
		}
		else
		{
			REQUIRE(Backend.m_Primitives.Size() == Reference.Size());
			for (Toolbox::size_t Index = 0; Index < Reference.Size(); ++Index)
			{
				REQUIRE(Backend.m_Primitives[Index].Tag == Reference[Index].Tag);
				REQUIRE(Backend.m_Primitives[Index].Depth == Reference[Index].Depth);
			}
		}
	}
}
TEST("failed draw closes the view restores state and suppresses presentation")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(2, 1)));
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(3, 2, 255)));
	Backend.m_FailDraw = 1;
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Backend.m_Ends == 1);
	REQUIRE(Backend.m_Resets >= 2);
	REQUIRE(Backend.m_Presentations == 0);
	Backend.m_FailDraw = -1;
	Begin_Internal(Renderer);
	REQUIRE(Renderer.GetContext().Get3D().Submit(MakeTriangle_Internal(1, 3, 255)));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives.Back().Depth == EDepthMode3D::TestAndWrite);
	REQUIRE(Backend.m_Presentations == 1);
}
#include "../Examples/RenderDebug/TransparencyDemo.h"
TEST("example uses real three dimensional entry and orders all panel triangles")
{
	FTransparencyBackend Backend;
	FRenderSystem Renderer(Backend);
	Begin_Internal(Renderer);
	REQUIRE(Dxf::RenderDebug::SubmitTransparencyDemo(Renderer.GetContext().Get3D()));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Primitives.Size() == 6);
	for (Toolbox::size_t Index = 0; Index < 6; ++Index)
	{
		REQUIRE(Backend.m_Primitives[Index].Depth == EDepthMode3D::TestOnly);
		if (Index > 0)
		{
			REQUIRE(Backend.m_Primitives[Index - 1].Z >= Backend.m_Primitives[Index].Z);
		}
	}
}
