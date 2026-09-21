// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/DebugCamera3D.h"
#include "Dxf/DebugStepController.h"
#include "Dxf/PhysicsDebugSnapshot3D.h"
#include "Dxf/DebugSnapshotHistory.h"
#include "Dxf/PhysicsDebugDisplay3D.h"
#include "Dxf/RenderContext.h"
using namespace Dxf;
using namespace Toolbox;
namespace
{
bool Near_Internal(f64 A, f64 B, f64 Epsilon = 1e-5)
{
	return Abs(A - B) <= Epsilon;
}
}
TEST("debug camera produces a valid view without modifying lighting")
{
	FDebugCamera3D Camera;
	FRenderView3D Base;
	Base.Id = 17;
	Base.Debug.Lighting = ELightingMode3D::LightsOff;
	Base.Debug.Surface = ESurfaceMode3D::SolidWithEdges;
	const auto View = Camera.MakeView(Base);
	REQUIRE(View);
	REQUIRE(IsValidRenderView3D(View.Value()));
	REQUIRE(View.Value().Id == 17);
	REQUIRE(View.Value().Debug.Lighting == ELightingMode3D::LightsOff);
	REQUIRE(View.Value().Debug.Surface == ESurfaceMode3D::SolidWithEdges);
}
TEST("orbit camera clamps poles and rejects invalid input transactionally")
{
	FDebugCamera3D Camera;
	REQUIRE(Camera.Orbit(0, 100));
	REQUIRE(IsValidRenderView3D(Camera.MakeView({}).Value()));
	const auto Before = Camera.GetPose();
	REQUIRE(!Camera.Zoom(-1));
	REQUIRE(Camera.GetPose().Distance == Before.Distance);
	REQUIRE(!Camera.MoveLocal({1, 0, 0}, -0.1, 3));
	REQUIRE(Camera.GetPose().Focus == Before.Focus);
}
TEST("local camera movement is normalized for diagonals")
{
	FDebugCamera3D Straight;
	FDebugCamera3D Diagonal;
	const auto Origin = Straight.GetPose().Focus;
	REQUIRE(Straight.MoveLocal({1, 0, 0}, 0.1, 4));
	REQUIRE(Diagonal.MoveLocal({1, 0, 1}, 0.1, 4));
	REQUIRE(Near_Internal(Length(Straight.GetPose().Focus - Origin), 0.4));
	REQUIRE(Near_Internal(Length(Diagonal.GetPose().Focus - Origin), 0.4));
}
TEST("paused stepping is exactly one fixed tick and resume does not replay time")
{
	FDebugStepController Controller;
	Controller.SetPaused(true);
	REQUIRE(Controller.Plan(0.2).Value().StepCount == 0);
	REQUIRE(Controller.RequestSingleStep());
	const auto One = Controller.Plan(0.2);
	REQUIRE(One);
	REQUIRE(One.Value().StepCount == 1);
	REQUIRE(Controller.Plan(0.2).Value().StepCount == 0);
	Controller.SetPaused(false);
	REQUIRE(!Controller.RequestSingleStep());
	REQUIRE(Controller.Plan(0).Value().StepCount == 0);
}
TEST("clock rejects invalid delta without losing a queued manual step")
{
	FDebugStepController Controller;
	Controller.SetPaused(true);
	REQUIRE(Controller.RequestSingleStep());
	REQUIRE(!Controller.Plan(-1));
	REQUIRE(Controller.Plan(0).Value().StepCount == 1);
}
TEST("clock budgets catch-up and reports discarded time")
{
	FDebugStepController Controller;
	const auto Plan = Controller.Plan(1.0);
	REQUIRE(Plan);
	REQUIRE(Plan.Value().StepCount == 8);
	REQUIRE(Plan.Value().DroppedSeconds > 0.8);
	REQUIRE(Controller.Plan(0).Value().StepCount == 0);
}
TEST("snapshot history is bounded and returned values survive eviction")
{
	FDebugSnapshotHistory History(2);
	FPhysicsDebugSnapshot3D Frame;
	Frame.World = 1;
	Frame.Step = 1;
	REQUIRE(History.Push(Frame));
	Frame.Step = 2;
	REQUIRE(History.Push(Frame));
	auto Saved = History.ReadAge(1);
	REQUIRE(Saved);
	Frame.Step = 3;
	REQUIRE(History.Push(Frame));
	REQUIRE(History.GetCount() == 2);
	REQUIRE(History.ReadAge(1).Value().Step == 2);
	REQUIRE(Saved.Value().Step == 1);
	REQUIRE(!History.ReadAge(2));
}
TEST("snapshot history rejects mixed worlds and nonmonotonic ticks")
{
	FDebugSnapshotHistory History(3);
	FPhysicsDebugSnapshot3D Frame;
	Frame.World = 7;
	Frame.Step = 2;
	REQUIRE(History.Push(Frame));
	REQUIRE(!History.Push(Frame));
	Frame.World = 8;
	Frame.Step = 3;
	REQUIRE(!History.Push(Frame));
	REQUIRE(History.GetCount() == 1);
	History.Clear();
	REQUIRE(History.Push(Frame));
}
TEST("camera rejects poses whose float rounding collapses the view basis")
{
	FDebugCamera3D Camera;
	const auto Before = Camera.GetPose();
	FDebugCameraPose3D Pose;
	Pose.Focus = {100000, 100000, 100000};
	Pose.Distance = 0.1;
	Pose.Yaw = 0;
	Pose.Pitch = 1.5533430342749532;
	REQUIRE(!Camera.SetPose(Pose));
	REQUIRE(Camera.GetPose().Focus == Before.Focus);
}
namespace
{
struct FSampleBodyId
{
	uint64 World = 3;
	size_t Index = 1;
	uint64 Generation = 1;
	bool operator==(const FSampleBodyId&) const = default;
};
struct FSampleColliderId
{
	FSampleBodyId Body;
	size_t Index = 1;
	uint64 Generation = 1;
	bool operator==(const FSampleColliderId&) const = default;
};
// 採取契約のテストダブル。物理ソルバーの正しさを検証するものではない。
struct FSampleRotation
{
	FVector3 Rotate(FVector3 Value) const
	{
		return {-Value.Y, Value.X, Value.Z};
	}
};
struct FSampleWorld
{
	bool Alive = true;
	FVector3 Position{10, 20, 30};
	FVector3 Velocity{1, 2, 3};
	bool IsColliderAlive(FSampleColliderId) const
	{
		return Alive;
	}
	FVector3 GetPosition(FSampleBodyId) const
	{
		return Position;
	}
	FSampleRotation GetOrientation(FSampleBodyId) const
	{
		return {};
	}
	FVector3 GetVelocity(FSampleBodyId) const
	{
		return Velocity;
	}
	FVector3 GetAngularVelocity(FSampleBodyId) const
	{
		return {0, 0, 1};
	}
	bool IsSleeping(FSampleBodyId) const
	{
		return false;
	}
};
FPhysicsDebugSnapshot3D MakeSnapshot_Internal()
{
	FSampleWorld World;
	TVector<TPhysicsDebugWatch3D<FSampleColliderId>> Watches;
	TPhysicsDebugWatch3D<FSampleColliderId> Watch;
	Watch.LocalShape = FOBB{};
	Watches.PushBack(Watch);
	return CapturePhysicsDebugSnapshot3D(World, Watches, 1, 1.0 / 60.0).Value();
}
class FDebugTraceBackend final : public IRenderBackend
{
public:
	TVector<FRenderView3D> m_Views;
	TVector<FPreparedGeometry3D> m_Packets;
	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}
	TResult<void> SetTarget(int32, int32, int32) override
	{
		return {};
	}
	TResult<void> Clear(FColor) override
	{
		return {};
	}
	TResult<void> ResetState(int32, int32) override
	{
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
	TResult<void> Present() override
	{
		return {};
	}
	TResult<void> BeginView3D(const FRenderView3D& View) override
	{
		m_Views.PushBack(View);
		return {};
	}
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D& Packet) override
	{
		m_Packets.PushBack(Packet);
		return {};
	}
};
}
TEST("capture applies body rotation to collider offset and box axes")
{
	FSampleWorld World;
	TVector<TPhysicsDebugWatch3D<FSampleColliderId>> Watches;
	TPhysicsDebugWatch3D<FSampleColliderId> Watch;
	FOBB Local;
	Local.Center = {2, 0, 0};
	Watch.LocalShape = Local;
	Watches.PushBack(Watch);
	const auto Captured = CapturePhysicsDebugSnapshot3D(World, Watches, 42, 0.7);
	REQUIRE(Captured);
	const auto& Item = Captured.Value().Items[0];
	REQUIRE(Get<FOBB>(Item.Shape).Center == FVector3(10, 22, 30));
	REQUIRE(Get<FOBB>(Item.Shape).Axes[0] == FVector3(0, 1, 0));
	REQUIRE(Item.CenterOfMass == FVector3(10, 20, 30));
	REQUIRE(Item.Velocity == FVector3(1, 2, 3));
	REQUIRE(Captured.Value().Step == 42);
	World.Position = {99, 99, 99};
	Watches.Clear();
	REQUIRE(Item.CenterOfMass == FVector3(10, 20, 30));
}
TEST("capture counts stale colliders and never queries their body state")
{
	FSampleWorld World;
	World.Alive = false;
	TVector<TPhysicsDebugWatch3D<FSampleColliderId>> Watches(1);
	auto Frame = CapturePhysicsDebugSnapshot3D(World, Watches, 4, 0.1);
	REQUIRE(Frame);
	REQUIRE(Frame.Value().Items.IsEmpty());
	REQUIRE(Frame.Value().SkippedCount == 1);
	REQUIRE(Frame.Value().World == 3);
}
TEST("capture rejects duplicate watches and mixed world identifiers")
{
	FSampleWorld World;
	TVector<TPhysicsDebugWatch3D<FSampleColliderId>> Watches(2);
	REQUIRE(!CapturePhysicsDebugSnapshot3D(World, Watches, 1, 0.1));
	Watches[1].Collider.Body.World = 9;
	REQUIRE(!CapturePhysicsDebugSnapshot3D(World, Watches, 1, 0.1));
}
TEST("capture handles empty inputs and refuses invalid state or unbounded watches")
{
	FSampleWorld World;
	TVector<TPhysicsDebugWatch3D<FSampleColliderId>> Watches;
	REQUIRE(CapturePhysicsDebugSnapshot3D(World, Watches, 0, 0));
	Watches.Resize(257);
	REQUIRE(!CapturePhysicsDebugSnapshot3D(World, Watches, 0, 0));
	Watches.Resize(1);
	World.Velocity.X = Toolbox::Sqrt(-1.0f);
	REQUIRE(!CapturePhysicsDebugSnapshot3D(World, Watches, 0, 0));
}
TEST("physics overlay contains only edges centers and measured velocity")
{
	const auto Frame = MakeSnapshot_Internal();
	FPhysicsDebugDisplaySettings3D Settings;
	const auto Geometry = BuildPhysicsDebugGeometry3D(Frame.Items[0], Settings);
	REQUIRE(Geometry);
	REQUIRE(Geometry.Value().Geometry.Triangles.IsEmpty());
	REQUIRE(Geometry.Value().Geometry.Lines.Size() == 16);
	REQUIRE(Geometry.Value().Options.Depth == EDepthMode3D::TestOnly);
	const auto& Velocity = Geometry.Value().Geometry.Lines[12];
	REQUIRE(Velocity.Start == Frame.Items[0].CenterOfMass);
	REQUIRE(Velocity.End == Frame.Items[0].CenterOfMass + Frame.Items[0].Velocity * 0.25f);
}
TEST("overlay is visible without lighting and does not open another view")
{
	FRenderQueue2D Queue;
	FJobSystem Jobs(4);
	FRenderContext Render(Queue, nullptr, &Jobs);
	Queue.SetAccepting_Internal(true);
	FRenderView3D View;
	View.Id = 25;
	View.Debug.Lighting = ELightingMode3D::LightsOff;
	REQUIRE(Render.Get3D().SetView(View));
	REQUIRE(Render.Get3D().DrawBox({}));
	REQUIRE(SubmitPhysicsDebugSnapshot3D(MakeSnapshot_Internal(), {}, Render.Get3D()));
	FDebugTraceBackend Backend;
	REQUIRE(Render.Execute3D_Internal(Backend));
	REQUIRE(Backend.m_Views.Size() == 1);
	REQUIRE(Backend.m_Views[0].Id == 25);
	REQUIRE(Backend.m_Packets.Size() == 2);
	REQUIRE(Backend.m_Packets[1].Triangles.IsEmpty());
	REQUIRE(Backend.m_Packets[1].Lines[0].Color.G == 255);
}
TEST("overlay produces identical packets in one and four lanes")
{
	const auto Frame = MakeSnapshot_Internal();
	FDebugTraceBackend Backends[2];
	for (size_t Run = 0; Run < 2; ++Run)
	{
		FRenderQueue2D Queue;
		FJobSystem Jobs(Run == 0 ? 1 : 4);
		FRenderContext Render(Queue, nullptr, &Jobs);
		Queue.SetAccepting_Internal(true);
		REQUIRE(SubmitPhysicsDebugSnapshot3D(Frame, {}, Render.Get3D()));
		REQUIRE(Render.Execute3D_Internal(Backends[Run]));
	}
	const auto& A = Backends[0].m_Packets[0].Lines;
	const auto& B = Backends[1].m_Packets[0].Lines;
	REQUIRE(A.Size() == B.Size());
	for (size_t Index = 0; Index < A.Size(); ++Index)
	{
		REQUIRE(A[Index].Line.Start == B[Index].Line.Start);
		REQUIRE(A[Index].Line.End == B[Index].Line.End);
		REQUIRE(A[Index].Depth == B[Index].Depth);
	}
}
TEST("invalid overlay tail leaves existing commands unchanged")
{
	FRenderQueue2D Queue;
	FJobSystem Jobs(4);
	FRenderContext Render(Queue, nullptr, &Jobs);
	Queue.SetAccepting_Internal(true);
	REQUIRE(Render.Get3D().DrawLine({0, 0, 0}, {1, 0, 0}));
	auto Frame = MakeSnapshot_Internal();
	Frame.Items.PushBack(Frame.Items[0]);
	Frame.Items[1].Velocity.Y = Toolbox::Sqrt(-1.0f);
	REQUIRE(!SubmitPhysicsDebugSnapshot3D(Frame, {}, Render.Get3D()));
	FDebugTraceBackend Backend;
	REQUIRE(Render.Execute3D_Internal(Backend));
	REQUIRE(Backend.m_Packets.Size() == 1);
	REQUIRE(Backend.m_Packets[0].Lines.Size() == 1);
}
TEST("all disabled overlays still reject invalid settings")
{
	FRenderQueue2D Queue;
	FJobSystem Jobs(1);
	FRenderContext Render(Queue, nullptr, &Jobs);
	Queue.SetAccepting_Internal(true);
	FPhysicsDebugDisplaySettings3D Settings;
	Settings.bColliders = false;
	Settings.bCenters = false;
	Settings.bVelocities = false;
	Settings.VelocitySeconds = -1;
	REQUIRE(!SubmitPhysicsDebugSnapshot3D(MakeSnapshot_Internal(), Settings, Render.Get3D()));
}
TEST("clock count is independent of render cadence for an equal duration")
{
	const uint32 Rates[] = {30, 60, 120, 144, 240};
	for (const uint32 Rate : Rates)
	{
		FDebugStepController Clock;
		uint32 Total = 0;
		for (uint32 Frame = 0; Frame < Rate * 10; ++Frame)
		{
			const auto Plan = Clock.Plan(1.0 / Rate);
			REQUIRE(Plan);
			Total += Plan.Value().StepCount;
			REQUIRE(Plan.Value().DroppedSeconds == 0);
		}
		REQUIRE(Total == 600);
	}
}
TEST("history and camera limits reject NaN and out of bounds inputs")
{
	FDebugCamera3D Camera;
	REQUIRE(!Camera.Orbit(Toolbox::Sqrt(-1.0), 0));
	REQUIRE(!Camera.Zoom(0));
	REQUIRE(!Camera.MoveLocal({2, 0, 0}, 0.1, 1));
	REQUIRE(Camera.Zoom(0.00001));
	REQUIRE(Near_Internal(Camera.GetPose().Distance, 0.1));
	FDebugStepController Clock;
	REQUIRE(!Clock.SetTimeScale(0));
	REQUIRE(!Clock.Plan(Toolbox::Sqrt(-1.0)));
	FDebugSnapshotHistory History;
	auto Frame = MakeSnapshot_Internal();
	Frame.SimulationSeconds = -1;
	REQUIRE(!History.Push(Frame));
	REQUIRE(History.GetCount() == 0);
}
TEST("clock reset discards fractional time while preserving pause and scale")
{
	FDebugStepController Clock;
	REQUIRE(Clock.SetTimeScale(0.25));
	REQUIRE(Clock.Plan(0.04).Value().StepCount == 0);
	Clock.Reset();
	REQUIRE(Clock.Plan(0.04).Value().StepCount == 0);
	Clock.SetPaused(true);
	REQUIRE(Clock.RequestSingleStep());
	Clock.Reset();
	REQUIRE(Clock.IsPaused());
	REQUIRE(Clock.Plan(0).Value().StepCount == 0);
}
