// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/RenderContext.h"
namespace
{
// 旧入口が残らないことをコンパイル時にも確認する。
template <typename T> constexpr bool HasLegacyDraw = requires(T& Render, Dxf::FTexture Texture)
{
	Render.Draw(Texture, Dxf::FVector2{});
};
template <typename T> constexpr bool HasLegacyText = requires(T& Render, Dxf::FFont Font)
{
	Render.DrawText(Font, Toolbox::FString{}, Dxf::FVector2{});
};
static_assert(!HasLegacyDraw<Dxf::FRenderContext>);
static_assert(!HasLegacyText<Dxf::FRenderContext>);
}
TEST("render exposes one typed entry for each dimension")
{
	Dxf::FRenderQueue2D Queue;
	Dxf::FRenderContext Render(Queue);
	Queue.SetAccepting_Internal(true);
	REQUIRE(Render.Get2D().FillRectangle({0, 0, 10, 10}));
	REQUIRE(Render.Get3D().DrawLine({0, 0, 0}, {1, 0, 0}));
}
#include "Dxf/RenderSystem.h"
namespace
{
using namespace Dxf;
using Toolbox::size_t;
// RendererとNative境界の順序、設定、後始末を記録する。
class FViewBackend final : public IRenderBackend
{
public:
	Toolbox::TVector<Toolbox::int32> m_Order;
	Toolbox::TVector<FRenderView3D> m_Views;
	Toolbox::TVector<FPreparedGeometry3D> m_Packets;
	Toolbox::uint32 m_Ends = 0;
	Toolbox::uint32 m_Presentations = 0;
	bool m_bSupported = true;
	bool m_bFailBegin = false;
	bool m_bFailGeometry = false;
	bool m_bThrowGeometry = false;
	bool m_bThrowEnd = false;
	Toolbox::uint32 m_Resets = 0;
	Toolbox::TFunction<void()> m_OnRectangle;
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
		m_Order.PushBack(1);
		return {};
	}
	TResult<void> DrawText(const FTextCommand&) override
	{
		m_Order.PushBack(2);
		return {};
	}
	TResult<void> DrawRectangle(const FRectangleCommand& C) override
	{
		if (m_OnRectangle)
		{
			m_OnRectangle();
		}
		m_Order.PushBack(C.bFilled ? 3 : 4);
		return {};
	}
	bool SupportsShapes2D() const noexcept override
	{
		return m_bSupported;
	}
	bool SupportsViewports3D() const noexcept override
	{
		return true;
	}
	bool SupportsGeometry3D() const noexcept override
	{
		return m_bSupported;
	}
	TResult<void> DrawLine2D(const FLineCommand2D&) override
	{
		m_Order.PushBack(5);
		return {};
	}
	TResult<void> DrawCircle2D(const FCircleCommand2D& C) override
	{
		m_Order.PushBack(C.bFilled ? 7 : 6);
		return {};
	}
	TResult<void> DrawTriangle2D(const FTriangleCommand2D& C) override
	{
		m_Order.PushBack(C.bFilled ? 9 : 8);
		return {};
	}
	TResult<void> BeginView3D(const FRenderView3D& View) override
	{
		m_Views.PushBack(View);
		return m_bFailBegin ? TResult<void>::Failure(EErrorCode::BackendFailure, "begin failure") : TResult<void>{};
	}
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D& P) override
	{
		m_Order.PushBack(30);
		m_Packets.PushBack(P);
		if (m_bThrowGeometry)
		{
			throw Toolbox::FException("geometry exception");
		}
		return m_bFailGeometry ? TResult<void>::Failure(EErrorCode::BackendFailure, "geometry failure") : TResult<void>{};
	}
	TResult<void> EndView3D() override
	{
		++m_Ends;
		if (m_bThrowEnd)
		{
			throw Toolbox::FException("cleanup exception");
		}
		return {};
	}
	TResult<void> Present() override
	{
		++m_Presentations;
		return {};
	}
};
FGeometryCommand3D Triangle_Internal()
{
	FGeometryCommand3D Command;
	Command.Geometry.Triangles.PushBack({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}});
	Command.Options.Color = {80, 160, 240, 255};
	return Command;
}
}
TEST("2D shapes cover lines circle triangle and rectangle without legacy paths")
{
	FViewBackend Backend;
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	auto& Draw = Renderer.GetContext().Get2D();
	REQUIRE(Draw.DrawRectangle({1, 2, 9, 10}));
	REQUIRE(Draw.FillRectangle({1, 2, 9, 10}));
	REQUIRE(Draw.DrawLine({0, 0}, {1, 2}));
	REQUIRE(Draw.DrawCircle({0, 0}, 5));
	REQUIRE(Draw.FillCircle({0, 0}, 5));
	REQUIRE(Draw.DrawTriangle({0, 0}, {1, 0}, {0, 1}));
	REQUIRE(Draw.FillTriangle({0, 0}, {1, 0}, {0, 1}));
	REQUIRE(Renderer.EndFrame());
	const Toolbox::int32 Expected[] = {4, 3, 5, 6, 7, 8, 9};
	REQUIRE(Backend.m_Order.Size() == 7);
	for (size_t I = 0; I < 7; ++I)
	{
		REQUIRE(Backend.m_Order[I] == Expected[I]);
	}
}
TEST("invalid 2D shape values are rejected before queued commands change")
{
	FRenderQueue2D Queue;
	FRenderContext Render(Queue);
	Queue.SetAccepting_Internal(true);
	REQUIRE(!Render.Get2D().DrawCircle({0, 0}, -1));
	REQUIRE(!Render.Get2D().DrawCircle({2e9f, 0}, 1e9f));
	REQUIRE(!Render.Get2D().DrawLine({1e30f, 0}, {0, 0}));
	REQUIRE(!Render.Get2D().DrawRectangle({5, 0, 1, 2}));
}
TEST("box topology uses twelve outward faces and twelve geometric edges")
{
	Toolbox::FOBB Box;
	auto Result = BuildBoxGeometry3D(Box);
	REQUIRE(Result);
	REQUIRE(Result.Value().Triangles.Size() == 12);
	REQUIRE(Result.Value().Lines.Size() == 12);
	for (const auto& T : Result.Value().Triangles)
	{
		const Toolbox::f64 Ux = T.B.X - T.A.X;
		const Toolbox::f64 Uy = T.B.Y - T.A.Y;
		const Toolbox::f64 Uz = T.B.Z - T.A.Z;
		const Toolbox::f64 Vx = T.C.X - T.A.X;
		const Toolbox::f64 Vy = T.C.Y - T.A.Y;
		const Toolbox::f64 Vz = T.C.Z - T.A.Z;
		REQUIRE((Uy * Vz - Uz * Vy) * T.A.X + (Uz * Vx - Ux * Vz) * T.A.Y + (Ux * Vy - Uy * Vx) * T.A.Z > 0);
	}
}
TEST("wireframe overlay and solid are separate geometry interpretations")
{
	auto Geometry = BuildBoxGeometry3D(Toolbox::FOBB{});
	REQUIRE(Geometry);
	FGeometryCommand3D Command{Toolbox::Move(Geometry).Value(), {}};
	FRenderView3D View;
	auto Solid = PrepareGeometry3D(Command, View);
	REQUIRE(Solid && Solid.Value().Triangles.Size() == 12 && Solid.Value().Lines.IsEmpty());
	View.Debug.Surface = ESurfaceMode3D::Wireframe;
	auto Wire = PrepareGeometry3D(Command, View);
	REQUIRE(Wire && Wire.Value().Triangles.IsEmpty() && Wire.Value().Lines.Size() == 12);
	View.Debug.Surface = ESurfaceMode3D::SolidWithEdges;
	auto Overlay = PrepareGeometry3D(Command, View);
	REQUIRE(Overlay && Overlay.Value().Triangles.Size() == 12 && Overlay.Value().Lines.Size() == 12);
	REQUIRE(Overlay.Value().Lines[0].Depth == EDepthMode3D::TestOnly);
	REQUIRE(Command.Geometry.Triangles.Size() == 12);
}
TEST("unlit and lights off do not mutate the base material or light")
{
	FGeometryCommand3D Command = Triangle_Internal();
	FRenderView3D View;
	View.LightDirection = {0, 0, -1};
	View.LightColor = {255, 255, 255, 255};
	View.AmbientColor = {0, 0, 0, 255};
	auto Lit = PrepareGeometry3D(Command, View);
	REQUIRE(Lit && Lit.Value().Triangles[0].Color.G == 160);
	View.Debug.Lighting = ELightingMode3D::LightsOff;
	auto Off = PrepareGeometry3D(Command, View);
	REQUIRE(Off && Off.Value().Triangles[0].Color.G == 0);
	View.Debug.Lighting = ELightingMode3D::Unlit;
	View.bLightEnabled = false;
	auto Unlit = PrepareGeometry3D(Command, View);
	REQUIRE(Unlit && Unlit.Value().Triangles[0].Color.G == 160);
	REQUIRE(Command.Options.Color.G == 160);
	REQUIRE(View.LightColor.G == 255);
}
TEST("camera and debug setting validation rejects degeneracy and unknown modes")
{
	FRenderView3D View;
	REQUIRE(IsValidRenderView3D(View));
	View.Eye = View.Target;
	REQUIRE(!IsValidRenderView3D(View));
	View = {};
	View.Up = {0, 0, 1};
	REQUIRE(!IsValidRenderView3D(View));
	View = {};
	View.FarPlane = View.NearPlane;
	REQUIRE(!IsValidRenderView3D(View));
	View = {};
	View.Debug.Surface = static_cast<ESurfaceMode3D>(127);
	REQUIRE(!IsValidRenderView3D(View));
}
TEST("sphere tessellation has no degenerate pole triangles")
{
	auto Geometry = BuildSphereGeometry3D({{0, 0, 0}, 2}, 16);
	REQUIRE(Geometry);
	REQUIRE(Geometry.Value().Triangles.Size() == 224);
	FGeometryCommand3D Command{Toolbox::Move(Geometry).Value(), {}};
	REQUIRE(IsValidGeometry3D(Command));
	for (const auto& Triangle : Command.Geometry.Triangles)
	{
		const Toolbox::f64 Squared = static_cast<Toolbox::f64>(Triangle.A.X) * Triangle.A.X +
		static_cast<Toolbox::f64>(Triangle.A.Y) * Triangle.A.Y + static_cast<Toolbox::f64>(Triangle.A.Z) * Triangle.A.Z;
		REQUIRE(Toolbox::Abs(Squared - 4) < 0.00001);
	}
	REQUIRE(!BuildSphereGeometry3D({{0, 0, 0}, -1}));
	REQUIRE(!BuildSphereGeometry3D({{0, 0, 0}, 1}, 3));
	REQUIRE(!BuildSphereGeometry3D({{0, 0, 0}, 1}, 1024));
}
TEST("geometry builders reject collapsed representations at huge coordinates")
{
	Toolbox::FOBB Box;
	Box.Center = {1e30f, 1e30f, 1e30f};
	REQUIRE(!BuildBoxGeometry3D(Box));
	REQUIRE(!BuildSphereGeometry3D({{1e30f, 1e30f, 1e30f}, 1}));
}
TEST("view settings are copied per pass and 2D stays after 3D")
{
	FViewBackend Backend;
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	auto& Render = Renderer.GetContext();
	REQUIRE(Render.Get2D().FillRectangle({0, 0, 1, 1}));
	FRenderView3D View;
	View.Id = 7;
	View.Debug.Surface = ESurfaceMode3D::Wireframe;
	REQUIRE(Render.Get3D().SetView(View));
	REQUIRE(Render.Get3D().Submit(Triangle_Internal()));
	View.Id = 8;
	View.Debug.Surface = ESurfaceMode3D::Solid;
	REQUIRE(Render.Get3D().SetView(View));
	REQUIRE(Render.Get3D().Submit(Triangle_Internal()));
	View.Id = 100;
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Views.Size() == 2);
	REQUIRE(Backend.m_Views[0].Id == 7 && Backend.m_Views[1].Id == 8);
	REQUIRE(Backend.m_Packets[0].Triangles.IsEmpty() && Backend.m_Packets[0].Lines.Size() == 3);
	REQUIRE(Backend.m_Packets[1].Triangles.Size() == 1);
	REQUIRE(Backend.m_Order[0] == 30 && Backend.m_Order[1] == 30 && Backend.m_Order[2] == 3);
	REQUIRE(Backend.m_Ends == 2);
}
TEST("3D begin and draw failure close the view and suppress presentation")
{
	for (Toolbox::uint32 Kind = 0; Kind < 2; ++Kind)
	{
		FViewBackend Backend;
		Backend.m_bFailBegin = Kind == 0;
		Backend.m_bFailGeometry = Kind == 1;
		FRenderSystem Renderer(Backend);
		REQUIRE(Renderer.BeginFrame(640, 480));
		REQUIRE(Renderer.GetContext().Get3D().Submit(Triangle_Internal()));
		REQUIRE(!Renderer.EndFrame());
		REQUIRE(Backend.m_Ends == 1);
		REQUIRE(Backend.m_Presentations == 0);
		Backend.m_bFailBegin = false;
		Backend.m_bFailGeometry = false;
		REQUIRE(Renderer.BeginFrame(640, 480));
		REQUIRE(Renderer.EndFrame());
		REQUIRE(Backend.m_Presentations == 1);
	}
}
TEST("unsupported 3D is an explicit failure before Native view calls")
{
	FViewBackend Backend;
	Backend.m_bSupported = false;
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get3D().Submit(Triangle_Internal()));
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Backend.m_Views.IsEmpty() && Backend.m_Presentations == 0);
}
TEST("3D cannot be mutated from a job including the synchronous lane")
{
	for (Toolbox::uint32 Lanes : {1u, 4u})
	{
		FViewBackend Backend;
		FRenderSystem Renderer(Backend);
		Toolbox::FJobSystem Jobs(Lanes);
		REQUIRE(Renderer.BeginFrame(640, 480));
		Toolbox::FJobFence Fence;
		bool Rejected = false;
		REQUIRE(Jobs.TrySubmit([&]()
		{
			Rejected = !Renderer.GetContext().Get3D().SetView({}) && !Renderer.GetContext().Get3D().DrawLine({0,0,0},{1,0,0});
		}, &Fence));
		REQUIRE(Jobs.Wait(Fence));
		REQUIRE(Rejected);
		REQUIRE(Renderer.EndFrame());
	}
}
TEST("3D generation cannot reenter 2D pass control or view settings")
{
	FViewBackend Backend;
	FRenderSystem Renderer(Backend);
	Toolbox::FJobSystem Jobs(1);
	REQUIRE(Renderer.SetExecutionJobs(Jobs));
	REQUIRE(Renderer.BeginFrame(640, 480));
	auto& Render = Renderer.GetContext();
	bool Rejected = false;
	REQUIRE(Render.Get3D().SubmitGenerated(1, [&](size_t, FGeometryCommand3D& Out)
	{
		Rejected = !Render.Get2D().FillRectangle({0,0,1,1}) && !Render.Get3D().SetView({}) && !Render.ClearTarget({});
		Out = Triangle_Internal();
		return TResult<void>{};
	}
	));
	REQUIRE(Rejected);
	REQUIRE(Renderer.EndFrame());
}
TEST("failed 3D batch preserves commands previously submitted")
{
	FViewBackend Backend;
	FRenderSystem Renderer(Backend);
	Toolbox::FJobSystem Jobs(4);
	REQUIRE(Renderer.SetExecutionJobs(Jobs));
	REQUIRE(Renderer.BeginFrame(640, 480));
	auto& Draw = Renderer.GetContext().Get3D();
	REQUIRE(Draw.Submit(Triangle_Internal()));
	REQUIRE(!Draw.SubmitGenerated(40, [&](size_t Index, FGeometryCommand3D& Out)
	{
		Out = Triangle_Internal();
		return Index == 39 ? TResult<void>::Failure(EErrorCode::UserException, "tail") : TResult<void>{};
	}, 1));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Packets.Size() == 1);
}
TEST("generated 3D commands keep input order across execution lane counts")
{
	for (Toolbox::uint32 Lanes : {1u, 4u})
	{
		FViewBackend Backend;
		FRenderSystem Renderer(Backend);
		Toolbox::FJobSystem Jobs(Lanes);
		REQUIRE(Renderer.SetExecutionJobs(Jobs));
		REQUIRE(Renderer.BeginFrame(640, 480));
		REQUIRE(Renderer.GetContext().Get3D().SubmitGenerated(64, [&](size_t Index, FGeometryCommand3D& Out)
		{
			for (size_t Spin = 0; Spin < 64 - Index; ++Spin)
			{
				Toolbox::FThread::Yield();
			}
			Out.Geometry.Lines.PushBack({{static_cast<Toolbox::f32>(Index),0,0},{0,1,0}});
			return TResult<void>{};
		}, 1));
		REQUIRE(Renderer.EndFrame());
		REQUIRE(Backend.m_Packets.Size() == 64);
		for (size_t Index = 0; Index < 64; ++Index)
		{
			REQUIRE(Backend.m_Packets[Index].Lines[0].Line.Start.X == static_cast<Toolbox::f32>(Index));
		}
	}
}
TEST("2D backend callback cannot mutate 3D view during a flush")
{
	FViewBackend Backend;
	FRenderSystem Renderer(Backend);
	bool Rejected = false;
	Backend.m_OnRectangle = [&]()
	{
		Rejected = !Renderer.GetContext().Get3D().SetView({});
	};
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get2D().FillRectangle({0,0,1,1}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Rejected);
}
TEST("3D draw failure remains primary when cleanup also throws")
{
	FViewBackend Backend;
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get3D().Submit(Triangle_Internal()));
	Backend.m_bFailGeometry = true;
	Backend.m_bThrowEnd = true;
	auto Result = Renderer.EndFrame();
	REQUIRE(!Result);
	REQUIRE(Result.Error().Message == "geometry failure");
	REQUIRE(Backend.m_Ends == 1);
	REQUIRE(Backend.m_Resets == 2);
	REQUIRE(Backend.m_Presentations == 0);
}
TEST("3D exception still restores 2D state and never presents partial output")
{
	FViewBackend Backend;
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get3D().Submit(Triangle_Internal()));
	Backend.m_bThrowGeometry = true;
	auto Result = Renderer.EndFrame();
	REQUIRE(!Result);
	REQUIRE(Backend.m_Ends == 1);
	REQUIRE(Backend.m_Resets == 2);
	REQUIRE(Backend.m_Presentations == 0);
}
TEST("3D frame primitive budget rejects batches without partially appending")
{
	FViewBackend Backend;
	FRenderSystem Renderer(Backend);
	Toolbox::FJobSystem Jobs(4);
	REQUIRE(Renderer.SetExecutionJobs(Jobs));
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get3D().DrawLine({0,0,0},{1,0,0}));
	auto Result = Renderer.GetContext().Get3D().SubmitGenerated(65,
	[](Toolbox::size_t, FGeometryCommand3D& Command) -> TResult<void>
	{
		Command.Geometry.Lines.Resize(1024);
		return {};
	}
	);
	REQUIRE(!Result);
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Packets.Size() == 1);
	REQUIRE(Backend.m_Packets[0].Lines.Size() == 1);
}
TEST("container rejects unaddressable allocation with its own failure contract")
{
	Toolbox::TVector<Toolbox::uint8> Values;
	Values.PushBack(9);
	bool Rejected = false;
	try
	{
		Values.Reserve(Toolbox::TNumericLimits<Toolbox::size_t>::Max() / 2 + 1);
	}
	catch (const Toolbox::FException&)
	{
		Rejected = true;
	}
	catch (...)
	{
	}
	REQUIRE(Rejected);
	REQUIRE(Values.Size() == 1);
	REQUIRE(Values[0] == 9);
}
TEST("legacy backend cannot silently fill an unsupported outline rectangle")
{
	FViewBackend Backend;
	Backend.m_bSupported = false;
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480));
	REQUIRE(Renderer.GetContext().Get2D().DrawRectangle({0, 0, 10, 10}));
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Backend.m_Order.IsEmpty());
	REQUIRE(Backend.m_Presentations == 0);
}

TEST("viewport rejects invalid extents without replacing accepted view")
{
	FViewBackend Backend;
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(641, 480));
	auto& Draw = Renderer.GetContext().Get3D();
	FRenderView3D Left;
	Left.bViewport = true;
	Left.Viewport = {0, 0, 320, 480};
	REQUIRE(Draw.SetView(Left));
	REQUIRE(Draw.DrawBox(Toolbox::FOBB{}));
	const FIntRect Invalid[] = {{0, 0, 0, 480},   {-1, 0, 320, 480},       {0, 0, 642, 480},
	                            {0, 0, 320, 481}, {0, 0, 2147483647, 480}, {0, 3, 320, 2}};
	for (const auto& Rect : Invalid)
	{
		FRenderView3D Candidate = Left;
		Candidate.Viewport = Rect;
		REQUIRE(!Draw.SetView(Candidate));
	}
	REQUIRE(Draw.DrawBox(Toolbox::FOBB{}));
	FRenderView3D Right = Left;
	Right.Viewport = {320, 0, 641, 480};
	Right.Eye.X = 1;
	Right.LightColor = {80, 90, 100, 255};
	REQUIRE(Draw.SetView(Right));
	REQUIRE(Draw.DrawBox(Toolbox::FOBB{}));
	Right.Viewport = {};
	Right.Eye.X = 20;
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Views.Size() == 2);
	REQUIRE(Backend.m_Views[0].Viewport.Right == 320);
	REQUIRE(Backend.m_Views[1].Viewport.Left == 320);
	REQUIRE(Backend.m_Views[1].Viewport.Right == 641);
	REQUIRE(Backend.m_Views[1].Eye.X == 1);
	REQUIRE(Backend.m_Views[1].LightColor.R == 80);
	REQUIRE(Backend.m_Presentations == 1);
}

TEST("viewport retained across target size change rejects generated work before jobs")
{
	FViewBackend Backend;
	FRenderSystem Renderer(Backend);
	Toolbox::FJobSystem Jobs(1);
	REQUIRE(Renderer.SetExecutionJobs(Jobs));
	REQUIRE(Renderer.BeginFrame(640, 480));
	FRenderView3D View;
	View.bViewport = true;
	View.Viewport = {320, 0, 640, 480};
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Renderer.BeginFrame(320, 240));
	bool Generated = false;
	REQUIRE(!Renderer.GetContext().Get3D().SubmitGenerated(1,
	                                                       [&](Toolbox::size_t, FGeometryCommand3D&) -> TResult<void>
	                                                       {
		                                                       Generated = true;
		                                                       return {};
	                                                       }));
	REQUIRE(!Generated);
	REQUIRE(Backend.m_Views.IsEmpty());
	REQUIRE(Renderer.EndFrame());
}
