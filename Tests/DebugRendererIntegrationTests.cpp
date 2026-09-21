// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/RenderSystem.h"
using FCheckedRenderer = Dxf::FRenderSystem;
namespace
{
class FOrderProbe final : public Dxf::IRenderBackend
{
public:
	Toolbox::TVector<Toolbox::int32> m_Trace;
	Dxf::TResult<void> SetTarget(Toolbox::int32, Toolbox::int32, Toolbox::int32) override
	{
		return {};
	}
	Dxf::TResult<void> Clear(Dxf::FColor) override
	{
		return {};
	}
	Dxf::TResult<void> ResetState(Toolbox::int32, Toolbox::int32) override
	{
		return {};
	}
	Dxf::TResult<void> DrawSprite(const Dxf::FSpriteCommand&) override
	{
		return {};
	}
	Dxf::TResult<void> DrawText(const Dxf::FTextCommand&) override
	{
		return {};
	}
	Dxf::TResult<void> DrawRectangle(const Dxf::FRectangleCommand&) override
	{
		m_Trace.PushBack(2);
		return {};
	}
	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}
	Dxf::TResult<void> BeginView3D(const Dxf::FRenderView3D&) override
	{
		return {};
	}
	Dxf::TResult<void> DrawGeometry3D(const Dxf::FPreparedGeometry3D&) override
	{
		m_Trace.PushBack(3);
		return {};
	}
	Dxf::TResult<void> Present() override
	{
		m_Trace.PushBack(9);
		return {};
	}
};
}
TEST("actual frame owner executes 3d before 2d and presentation")
{
	FOrderProbe Backend;
	FCheckedRenderer Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(1280, 720));
	REQUIRE(Renderer.GetContext().Get3D().DrawBox({}));
	REQUIRE(Renderer.GetContext().Get2D().FillRectangle({0, 0, 10, 10}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Trace.Size() == 3);
	REQUIRE(Backend.m_Trace[0] == 3);
	REQUIRE(Backend.m_Trace[1] == 2);
	REQUIRE(Backend.m_Trace[2] == 9);
}
TEST("application style job binding enables both generation entry points")
{
	FOrderProbe Backend;
	FCheckedRenderer Renderer(Backend);
	Toolbox::FJobSystem Jobs(4);
	REQUIRE(Renderer.GetContext().SetExecutionJobs_Internal(&Jobs));
	REQUIRE(Renderer.BeginFrame(1280, 720));
	REQUIRE(Renderer.GetContext().Get3D().SubmitGenerated(2, [](Toolbox::size_t Index, Dxf::FGeometryCommand3D& Output)
	{
		const Toolbox::f32 X = static_cast<Toolbox::f32>(Index);
		Output.Geometry.Lines.PushBack({{X, 0, 0}, {X, 1, 0}});
		return Dxf::TResult<void>{};
	}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.m_Trace.Size() == 3);
}
