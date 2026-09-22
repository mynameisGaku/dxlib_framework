// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/DebugCamera3D.h"
#include "Dxf/DebugStepController.h"
#include "Dxf/PhysicsDebugSnapshot3D.h"
#include "Dxf/DebugSnapshotHistory.h"
#include "Dxf/PhysicsDebugDisplay3D.h"
#include "Dxf/PhysicsDebugDisplay2D.h"
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
// 実Worldが返す値型と同じFPhysicsSnapshot3Dを手で組み、変換だけを検証する。
FPhysicsSnapshot3D MakeSource_Internal()
{
	FPhysicsSnapshot3D Source;
	Source.World = 3;
	Source.StepIndex = 1;
	Source.LastDeltaSeconds = 1.0 / 60.0;
	Source.LastSubSteps = 1;
	FPhysicsSnapshot3D::FBody Body;
	Body.Id = {3, 1, 1};
	Body.Position = {10, 20, 30};
	Body.Velocity = {1, 2, 3};
	Body.AngularVelocity = {0, 0, 1};
	Source.Bodies.PushBack(Body);
	FPhysicsSnapshot3D::FCollider Collider;
	Collider.Id = {Body.Id, 1, 1};
	Collider.LocalShape = FOBB{};
	Source.Colliders.PushBack(Collider);
	return Source;
}
FPhysicsDebugSnapshot3D MakeSnapshot_Internal()
{
	return BuildPhysicsDebugSnapshot3D(MakeSource_Internal(), 1.0 / 60.0).Value();
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
TEST("conversion applies body rotation to collider offset and box axes once")
{
	auto Source = MakeSource_Internal();
	// Z軸回り90度。重心相対(2,0,0)はワールドで(0,2,0)だけずれる。
	Source.Bodies[0].Rotation = FQuaternion::FromAxisAngle({0, 0, 1}, 1.57079632679f);
	FOBB Local;
	Local.Center = {2, 0, 0};
	Source.Colliders[0].LocalShape = Local;
	const auto Converted = BuildPhysicsDebugSnapshot3D(Source, 0.7);
	REQUIRE(Converted);
	const auto& Item = Converted.Value().Items[0];
	const auto& Box = Get<FOBB>(Item.Shape);
	REQUIRE(Near_Internal(Box.Center.X, 10) && Near_Internal(Box.Center.Y, 22) && Near_Internal(Box.Center.Z, 30));
	REQUIRE(Near_Internal(Box.Axes[0].X, 0) && Near_Internal(Box.Axes[0].Y, 1));
	REQUIRE(Near_Internal(Box.Axes[1].X, -1) && Near_Internal(Box.Axes[1].Y, 0));
	REQUIRE(Item.CenterOfMass == FVector3(10, 20, 30));
	REQUIRE(Item.Velocity == FVector3(1, 2, 3));
	REQUIRE(Item.Collider == Source.Colliders[0].Id);
	REQUIRE(Converted.Value().Step == 1 && Converted.Value().SimulationSeconds == 0.7);
	REQUIRE(Converted.Value().BodyCount == 1);
	// 変換結果は入力を参照しない。
	Source.Bodies[0].Position = {99, 99, 99};
	Source.Colliders.Clear();
	REQUIRE(Item.CenterOfMass == FVector3(10, 20, 30));
}
TEST("conversion rejects colliders whose body generation or world does not match")
{
	auto Stale = MakeSource_Internal();
	Stale.Colliders[0].Id.Body.Generation = 2;
	REQUIRE(!BuildPhysicsDebugSnapshot3D(Stale, 0.1));
	auto Foreign = MakeSource_Internal();
	Foreign.Colliders[0].Id.Body.World = 9;
	REQUIRE(!BuildPhysicsDebugSnapshot3D(Foreign, 0.1));
	auto Missing = MakeSource_Internal();
	Missing.Colliders[0].Id.Body.Index = 7;
	REQUIRE(!BuildPhysicsDebugSnapshot3D(Missing, 0.1));
}
TEST("conversion keeps bodies without colliders counted and never truncates")
{
	auto Source = MakeSource_Internal();
	FPhysicsSnapshot3D::FBody Bare;
	Bare.Id = {3, 4, 2};
	Source.Bodies.PushBack(Bare);
	const auto Converted = BuildPhysicsDebugSnapshot3D(Source, 0);
	REQUIRE(Converted);
	REQUIRE(Converted.Value().BodyCount == 2 && Converted.Value().Items.Size() == 1);
	// 表示上限を超える場合は、先頭だけを残さず失敗する。
	auto Many = MakeSource_Internal();
	while (Many.Colliders.Size() <= MaxPhysicsDebugColliders3D)
	{
		auto Extra = Many.Colliders[0];
		Extra.Id.Index = Many.Colliders.Size() + 1;
		Many.Colliders.PushBack(Extra);
	}
	REQUIRE(!BuildPhysicsDebugSnapshot3D(Many, 0));
}
TEST("conversion handles empty worlds and refuses invalid state or time")
{
	FPhysicsSnapshot3D Empty;
	Empty.World = 5;
	const auto Converted = BuildPhysicsDebugSnapshot3D(Empty, 0);
	REQUIRE(Converted && Converted.Value().Items.IsEmpty() && Converted.Value().World == 5);
	REQUIRE(!BuildPhysicsDebugSnapshot3D(FPhysicsSnapshot3D{}, 0));
	REQUIRE(!BuildPhysicsDebugSnapshot3D(MakeSource_Internal(), -1));
	REQUIRE(!BuildPhysicsDebugSnapshot3D(MakeSource_Internal(), Toolbox::Sqrt(-1.0)));
	auto Broken = MakeSource_Internal();
	Broken.Bodies[0].Velocity.X = Toolbox::Sqrt(-1.0f);
	REQUIRE(!BuildPhysicsDebugSnapshot3D(Broken, 0));
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
namespace
{
// 実Worldが返す値型と同じFPhysicsSnapshot2Dを手で組み、変換だけを検証する。
FPhysicsSnapshot2D MakeSource2D_Internal()
{
	FPhysicsSnapshot2D Source;
	Source.World = 6;
	Source.StepIndex = 3;
	FPhysicsSnapshot2D::FBody Body;
	Body.Id = {6, 0, 1};
	Body.Position = {1, 2};
	Body.Rotation = 1.57079632679f;
	Body.Velocity = {4, 0};
	Source.Bodies.PushBack(Body);
	FOrientedBox2D Box;
	Box.Center = {1, 0};
	Box.HalfExtents = {0.5f, 0.25f};
	Box.Angle = 0.25f;
	FPhysicsSnapshot2D::FCollider Collider;
	Collider.Id = {Body.Id, 0, 1};
	Collider.LocalShape = Box;
	Source.Colliders.PushBack(Collider);
	FCircle2D Circle;
	Circle.Center = {0, 1};
	Circle.Radius = 0.5f;
	Collider.Id = {Body.Id, 1, 1};
	Collider.LocalShape = Circle;
	Source.Colliders.PushBack(Collider);
	return Source;
}
// 2D線・円の実行順を記録する診断用Backend。
class FDebugTraceBackend2D final : public IRenderBackend
{
public:
	TVector<FLineCommand2D> m_Lines;
	TVector<FCircleCommand2D> m_Circles;
	bool SupportsShapes2D() const noexcept override
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
	TResult<void> DrawLine2D(const FLineCommand2D& Command) override
	{
		m_Lines.PushBack(Command);
		return {};
	}
	TResult<void> DrawCircle2D(const FCircleCommand2D& Command) override
	{
		m_Circles.PushBack(Command);
		return {};
	}
	TResult<void> Present() override
	{
		return {};
	}
};
}
TEST("2d conversion rotates local centers and adds body angle to boxes once")
{
	const auto Converted = BuildPhysicsDebugSnapshot2D(MakeSource2D_Internal(), 0.05);
	REQUIRE(Converted);
	const auto& Frame = Converted.Value();
	REQUIRE(Frame.Items.Size() == 2 && Frame.Step == 3 && Frame.BodyCount == 1);
	// 90度回転で重心相対(1,0)は(0,1)、(0,1)は(-1,0)だけずれる。
	const auto& Box = Get<FOrientedBox2D>(Frame.Items[0].Shape);
	REQUIRE(Near_Internal(Box.Center.X, 1) && Near_Internal(Box.Center.Y, 3));
	REQUIRE(Near_Internal(Box.Angle, 1.57079632679 + 0.25));
	REQUIRE(Box.HalfExtents.X == 0.5f && Box.HalfExtents.Y == 0.25f);
	const auto& Circle = Get<FCircle2D>(Frame.Items[1].Shape);
	REQUIRE(Near_Internal(Circle.Center.X, 0) && Near_Internal(Circle.Center.Y, 2));
	REQUIRE(Frame.Items[1].BodyAngle == 1.57079632679f);
	auto Stale = MakeSource2D_Internal();
	Stale.Colliders[1].Id.Body.Generation = 5;
	REQUIRE(!BuildPhysicsDebugSnapshot2D(Stale, 0));
}
TEST("2d overlay maps meters to screen pixels with the y axis flipped")
{
	const auto Frame = BuildPhysicsDebugSnapshot2D(MakeSource2D_Internal(), 0).Value();
	FPhysicsDebugView2D View;
	View.ScreenOrigin = {100, 200};
	View.PixelsPerMeter = 10;
	FPhysicsDebugDisplaySettings2D Settings;
	Settings.bVelocities = false;
	Settings.bCenters = false;
	TVector<FRenderCommand> Commands;
	REQUIRE(BuildPhysicsDebugCommands2D(Frame.Items[1], View, Settings, Commands));
	// 円一つと、Body角の方向を示す半径線一本。
	REQUIRE(Commands.Size() == 2);
	const auto& Circle = Get<FCircleCommand2D>(Commands[0]);
	REQUIRE(Near_Internal(Circle.Center.X, 100) && Near_Internal(Circle.Center.Y, 180));
	REQUIRE(Near_Internal(Circle.Radius, 5));
	const auto& Marker = Get<FLineCommand2D>(Commands[1]);
	REQUIRE(Near_Internal(Marker.End.X, 100, 1e-4) && Near_Internal(Marker.End.Y, 175, 1e-4));
	Commands.Clear();
	REQUIRE(BuildPhysicsDebugCommands2D(Frame.Items[0], View, Settings, Commands));
	REQUIRE(Commands.Size() == 4);
	View.PixelsPerMeter = 0;
	REQUIRE(!BuildPhysicsDebugCommands2D(Frame.Items[0], View, Settings, Commands));
	REQUIRE(Commands.Size() == 4);
}
TEST("2d overlay submits existing line and circle commands through the 2d context")
{
	const auto Frame = BuildPhysicsDebugSnapshot2D(MakeSource2D_Internal(), 0).Value();
	FRenderQueue2D Queue;
	FJobSystem Jobs(4);
	FRenderContext Render(Queue, nullptr, &Jobs);
	Queue.SetAccepting_Internal(true);
	REQUIRE(SubmitPhysicsDebugSnapshot2D(Frame, {}, {}, Render.Get2D()));
	FDebugTraceBackend2D Backend;
	REQUIRE(Queue.Execute_Internal(Backend));
	// 箱4辺+円1+半径線1、速度線2本、重心十字2組。
	REQUIRE(Backend.m_Circles.Size() == 1);
	REQUIRE(Backend.m_Lines.Size() == 4 + 1 + 2 + 4);
	auto Broken = Frame;
	Broken.Items[1].Velocity.X = Toolbox::Sqrt(-1.0f);
	REQUIRE(!SubmitPhysicsDebugSnapshot2D(Broken, {}, {}, Render.Get2D()));
	FDebugTraceBackend2D Empty;
	REQUIRE(Queue.Execute_Internal(Empty));
	REQUIRE(Empty.m_Lines.IsEmpty() && Empty.m_Circles.IsEmpty());
}
