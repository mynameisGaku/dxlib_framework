// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Dxf/PhysicsDebugSnapshot3D.h"
#include "Dxf/PhysicsDebugSnapshot2D.h"
#include "Dxf/PhysicsDebugRecorder3D.h"
#include "Dxf/PhysicsDebugDisplay2D.h"
#include "Dxf/PhysicsDebugDisplay3D.h"
#include "Dxf/RenderContext.h"
using namespace Dxf;
using namespace Toolbox;
namespace
{
bool Near_Internal(f64 A, f64 B, f64 Epsilon = 1e-4)
{
	return Abs(A - B) <= Epsilon;
}
// 3D命令の件数だけを数える診断用Backend。
class FCountingBackend3D final : public IRenderBackend
{
public:
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
	TResult<void> BeginView3D(const FRenderView3D&) override
	{
		return {};
	}
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D& Packet) override
	{
		m_Packets.PushBack(Packet);
		return {};
	}
};
// 2D命令の件数だけを数える診断用Backend。
class FCountingBackend2D final : public IRenderBackend
{
public:
	size_t m_Lines = 0;
	size_t m_Circles = 0;
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
	TResult<void> DrawLine2D(const FLineCommand2D&) override
	{
		++m_Lines;
		return {};
	}
	TResult<void> DrawCircle2D(const FCircleCommand2D&) override
	{
		++m_Circles;
		return {};
	}
	TResult<void> Present() override
	{
		return {};
	}
};
} // namespace
TEST("debug capture reads an actual stepped 3d world without a watch list")
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Body;
	Body.Position = {10, 20, 30};
	Body.Velocity = {3, 0, 0};
	Body.Orientation = FQuaternion::FromAxisAngle({0, 0, 1}, 1.57079632679f);
	const auto BodyId = World.CreateBody(Body);
	FColliderDescription3D Sphere;
	Sphere.Shape = FSphere{{2, 0, 0}, 0.5f};
	FColliderDescription3D Box;
	FOBB LocalBox;
	LocalBox.Center = {0, 0, 1};
	Box.Shape = LocalBox;
	// 同じBodyへ二つのCollider。表示側へ別登録は渡さない。
	const auto SphereId = World.AttachCollider(BodyId, Sphere);
	const auto BoxId = World.AttachCollider(BodyId, Box);
	World.Step(1.0 / 60.0);
	auto Captured = CapturePhysicsDebugSnapshot3D(World, 1.0 / 60.0);
	REQUIRE(Captured);
	const auto& Frame = Captured.Value();
	REQUIRE(Frame.Step == 1 && Frame.LastSubSteps == 1 && Frame.BodyCount == 1);
	REQUIRE(Frame.Items.Size() == 2);
	REQUIRE(Frame.Items[0].Collider == SphereId && Frame.Items[1].Collider == BoxId);
	// 実Stepの位置と姿勢を、重心相対のオフセットへ一度だけ適用する。
	const auto Position = World.GetPosition(BodyId);
	const auto Center = Get<FSphere>(Frame.Items[0].Shape).Center;
	REQUIRE(Near_Internal(Center.X, Position.X) && Near_Internal(Center.Y, Position.Y + 2));
	REQUIRE(Near_Internal(Position.X, 10.05f));
	const auto& WorldBox = Get<FOBB>(Frame.Items[1].Shape);
	REQUIRE(Near_Internal(WorldBox.Center.Z, 31) && Near_Internal(WorldBox.Axes[0].Y, 1));
	REQUIRE(Frame.Items[0].Velocity == FVector3(3, 0, 0));
	// 削除後も採取済みの値は保持され、次の採取には現れない。
	REQUIRE(World.DestroyBody(BodyId));
	REQUIRE(Get<FSphere>(Frame.Items[0].Shape).Center == Center);
	const auto Removed = CapturePhysicsDebugSnapshot3D(World, 2.0 / 60.0);
	REQUIRE(Removed);
	REQUIRE(Removed.Value().Items.IsEmpty() && Removed.Value().BodyCount == 0);
}
TEST("debug capture reaches the 3d render queue from an actual world")
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	FColliderDescription3D Floor;
	Floor.Shape = FOBB{};
	World.AttachCollider(World.CreateBody(Ground), Floor);
	FBodyDescription3D Ball;
	Ball.Position = {0, 3, 0};
	Ball.Velocity = {1, 0, 0};
	FColliderDescription3D Sphere;
	Sphere.Shape = FSphere{{}, 0.5f};
	World.AttachCollider(World.CreateBody(Ball), Sphere);
	World.Step(1.0 / 60.0);
	const auto Frame = CapturePhysicsDebugSnapshot3D(World, 1.0 / 60.0).Value();
	FRenderQueue2D Queue;
	FJobSystem Jobs(2);
	FRenderContext Render(Queue, nullptr, &Jobs);
	Queue.SetAccepting_Internal(true);
	REQUIRE(SubmitPhysicsDebugSnapshot3D(Frame, {}, Render.Get3D()));
	FCountingBackend3D Backend;
	REQUIRE(Render.Execute3D_Internal(Backend));
	REQUIRE(Backend.m_Packets.Size() == 2);
}
TEST("debug capture reports refusals as results and leaves no partial output")
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	World.CreateBody({});
	// 停止済みJob Systemで実Stepを途中失敗させる。
	FJobSystem Jobs(2);
	Jobs.Shutdown();
	FPhysicsExecutionSettings Broken;
	Broken.JobSystem = &Jobs;
	World.SetExecutionSettings(Broken);
	bool bThrown = false;
	try
	{
		World.Step(1.0 / 60.0);
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	REQUIRE(bThrown);
	const auto Refused = CapturePhysicsDebugSnapshot3D(World, 0);
	REQUIRE(!Refused);
	REQUIRE(Refused.Error().Code == EErrorCode::InvalidState);
	// 表示上限を超えるWorldは、一部だけを返さず失敗する。
	World.SetExecutionSettings({});
	World.Step(1.0 / 60.0);
	FPhysicsWorld3D Crowded;
	const auto Body = Crowded.CreateBody({});
	FColliderDescription3D Sphere;
	Sphere.Shape = FSphere{{}, 0.25f};
	for (size_t Index = 0; Index <= MaxPhysicsDebugColliders3D; ++Index)
	{
		Crowded.AttachCollider(Body, Sphere);
	}
	REQUIRE(!CapturePhysicsDebugSnapshot3D(Crowded, 0));
	REQUIRE(CapturePhysicsDebugSnapshot3D(World, 0));
}
TEST("recorder does not capture while disabled and keeps paused history stable")
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Moving;
	Moving.Velocity = {1, 0, 0};
	FColliderDescription3D Sphere;
	Sphere.Shape = FSphere{{}, 0.5f};
	World.AttachCollider(World.CreateBody(Moving), Sphere);
	FPhysicsDebugRecorder3D Recorder(8, 2);
	// 外側の時計で記録した経過秒数。Step数×秒数から逆算しない。
	f64 Clock = 0;
	REQUIRE(Recorder.Record(World, Clock, true).Value());
	REQUIRE(Recorder.GetHistory().GetCount() == 1);
	Recorder.SetEnabled(false);
	REQUIRE(!Recorder.HasLive());
	for (uint32 Tick = 0; Tick < 5; ++Tick)
	{
		World.Step(0.1);
		Clock += 0.1;
		// 無効中はWorldへ触れない。成功だが採取は行わない。
		REQUIRE(!Recorder.Record(World, Clock, false).Value());
	}
	REQUIRE(Recorder.GetHistory().GetCount() == 1);
	// 途中失敗したStepの後でも、無効中は採取しないため失敗しない。
	FJobSystem Stopped(2);
	Stopped.Shutdown();
	FPhysicsExecutionSettings Broken;
	Broken.JobSystem = &Stopped;
	World.SetExecutionSettings(Broken);
	bool bThrown = false;
	try
	{
		World.Step(0.1);
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	REQUIRE(bThrown);
	REQUIRE(Recorder.Record(World, Clock, false));
	Recorder.SetEnabled(true);
	REQUIRE(!Recorder.Record(World, Clock, false));
	REQUIRE(!Recorder.HasLive() && Recorder.GetHistory().GetCount() == 1);
	World.SetExecutionSettings({});
	World.Step(0.1);
	Clock += 0.1;
	REQUIRE(Recorder.Record(World, Clock, true).Value());
	REQUIRE(Recorder.GetLive().Step == 6 && Recorder.GetHistory().GetCount() == 2);
	const auto Paused = Recorder.GetLive();
	// 停止中の再描画・再採取ではStep番号も履歴も増えない。
	for (uint32 Frame = 0; Frame < 3; ++Frame)
	{
		REQUIRE(Recorder.Record(World, Clock, true).Value());
	}
	REQUIRE(Recorder.GetLive().Step == Paused.Step);
	REQUIRE(Recorder.GetHistory().GetCount() == 2);
	// 履歴の閲覧はWorldへ書き戻さない。
	const auto Before = World.GetPosition(Recorder.GetLive().Items[0].Collider.Body);
	const auto Oldest = Recorder.GetHistory().ReadAge(1);
	REQUIRE(Oldest && Oldest.Value().Step == 0);
	REQUIRE(World.GetPosition(Recorder.GetLive().Items[0].Collider.Body) == Before);
	REQUIRE(Oldest.Value().Items[0].CenterOfMass.X == 0);
	REQUIRE(Near_Internal(Recorder.GetLive().SimulationSeconds, 0.6));
}
TEST("2d debug capture reads an actual world and reaches the 2d render queue")
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FBodyDescription2D Description;
	Description.Position = {2, 1};
	Description.Angle = 0.5f;
	Description.AngularVelocity = 1;
	const auto Body = World.CreateBody(Description);
	FColliderDescription2D Box;
	FOrientedBox2D LocalBox;
	LocalBox.Center = {1, 0};
	LocalBox.Angle = 0.25f;
	Box.Shape = LocalBox;
	FColliderDescription2D Circle;
	FCircle2D LocalCircle;
	LocalCircle.Radius = 0.5f;
	Circle.Shape = LocalCircle;
	const auto BoxId = World.AttachCollider(Body, Box);
	World.AttachCollider(Body, Circle);
	World.CreateBody({});
	World.Step(0.25, 2);
	auto Captured = CapturePhysicsDebugSnapshot2D(World, 0.25);
	REQUIRE(Captured);
	const auto& Frame = Captured.Value();
	REQUIRE(Frame.Step == 1 && Frame.LastSubSteps == 2 && Frame.BodyCount == 2 && Frame.Items.Size() == 2);
	REQUIRE(Frame.Items[0].Collider == BoxId);
	const f32 Angle = World.GetAngle(Body);
	const auto Position = World.GetPosition(Body);
	const auto& WorldBox = Get<FOrientedBox2D>(Frame.Items[0].Shape);
	REQUIRE(Near_Internal(WorldBox.Angle, Angle + 0.25f));
	REQUIRE(Near_Internal(WorldBox.Center.X, Position.X + Cos(f64(Angle))));
	REQUIRE(Near_Internal(WorldBox.Center.Y, Position.Y + Sin(f64(Angle))));
	FRenderQueue2D Queue;
	FJobSystem Jobs(2);
	FRenderContext Render(Queue, nullptr, &Jobs);
	Queue.SetAccepting_Internal(true);
	REQUIRE(SubmitPhysicsDebugSnapshot2D(Frame, {}, {}, Render.Get2D()));
	FCountingBackend2D Backend;
	REQUIRE(Queue.Execute_Internal(Backend));
	REQUIRE(Backend.m_Circles == 1);
	REQUIRE(Backend.m_Lines > 4);
	// World破棄後も採取値は利用できる。
	REQUIRE(World.DestroyBody(Body));
	REQUIRE(Get<FOrientedBox2D>(Frame.Items[0].Shape).Angle == WorldBox.Angle);
}
TEST("actual worlds preserve debug geometry in one and four execution lanes")
{
	TVector<FPhysicsDebugSnapshot3D> Results;
	const uint32 LaneCounts[] = {1, 4};
	for (uint32 Lanes : LaneCounts)
	{
		FJobSystem Jobs(Lanes);
		FPhysicsWorld3D World;
		FPhysicsExecutionSettings Execution;
		Execution.JobSystem = &Jobs;
		World.SetExecutionSettings(Execution);
		World.SetGravity({0, 0, 0});
		FBodyDescription3D Description;
		Description.Velocity = {0.5f, 0, 0};
		const auto Body = World.CreateBody(Description);
		FColliderDescription3D Collider;
		Collider.Shape = FOBB{};
		World.AttachCollider(Body, Collider);
		for (uint32 Tick = 0; Tick < 60; ++Tick)
		{
			World.Step(1.0 / 60.0);
		}
		Results.PushBack(CapturePhysicsDebugSnapshot3D(World, 1).Value());
	}
	REQUIRE(Results[0].Items[0].CenterOfMass == Results[1].Items[0].CenterOfMass);
	REQUIRE(Results[0].Items[0].Velocity == Results[1].Items[0].Velocity);
	REQUIRE(Results[0].Step == 60 && Results[1].Step == 60);
}
