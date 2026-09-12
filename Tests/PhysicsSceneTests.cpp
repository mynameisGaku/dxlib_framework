// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/RigidBodyComponent2D.h"
#include "Dxf/RigidBodyComponent3D.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/GameObject.h"
#include "Dxf/GameObjectComponent.h"
#include "Dxf/InputStateTracker.h"
#include "Toolbox/Function.h"
using namespace Dxf;
using namespace Dxf::Testing;
using namespace Toolbox;
namespace
{
// 固定更新の回数とエッジを数える。
struct FSceneCounters
{
	// 固定更新フックを呼んだ回数。
	Toolbox::int32 FixedTick = 0;
	// 最初の更新で観測した押下。
	bool bPressedOnFirst = false;
	// 二回目以降の更新で観測した押下。
	bool bPressedLater = false;
	// 未配達分から観測した押下。
	bool bPressedFromPending = false;
};
// 固定更新を数える検証用コンポーネント。
class DProbeComponent final : public DGameObjectComponent
{
public:
	// 検証に必要な依存先を設定する。
	explicit DProbeComponent(FSceneCounters& Counters) : m_pCounters(&Counters)
	{
	}

protected:
	// 固定更新の呼び出しとエッジを観測する。
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		++m_pCounters->FixedTick;
		if (Context.StepIndex == 0 && Context.WasPressed(EKey::Space))
		{
			m_pCounters->bPressedOnFirst = true;
		}
		if (Context.StepIndex > 0 && Context.WasPressed(EKey::Space))
		{
			m_pCounters->bPressedLater = true;
		}
		if (!Context.PendingInputs.IsEmpty() && Context.WasPressed(EKey::Space))
		{
			m_pCounters->bPressedFromPending = true;
		}
	}

private:
	// 外部の集計への参照。
	FSceneCounters* m_pCounters;
};
// 検証用のプローブを持つオブジェクト。
class DProbeObject : public DGameObject
{
public:
	// 検証に必要な依存先を設定する。
	explicit DProbeObject(FSceneCounters& Counters) : m_pCounters(&Counters)
	{
	}

protected:
	// プローブを所有へ追加する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		auto Probe = AddComponent<DProbeComponent>(*m_pCounters);
		if (!Probe)
		{
			return TResult<void>::Failure(Probe.Error());
		}
		return {};
	}

private:
	// 外部の集計への参照。
	FSceneCounters* m_pCounters;
};
// 円と床を持つ落下検証用オブジェクト。
class DBallObject : public DGameObject
{
protected:
	// 剛体とコライダーを所有へ追加する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		FBodyDescription2D Fall;
		Fall.Position = {0, 5};
		auto Body = AddComponent<DRigidBody2DComponent>(Fall);
		if (!Body)
		{
			return TResult<void>::Failure(Body.Error());
		}
		FColliderDescription2D Ball;
		FCircle2D Shape;
		Shape.Center = {0, 0};
		Shape.Radius = 0.5f;
		Ball.Shape = Shape;
		Ball.Friction = 0.4f;
		auto Collider = AddComponent<DCollider2DComponent>(Ball);
		if (!Collider)
		{
			return TResult<void>::Failure(Collider.Error());
		}
		return {};
	}
};
// 静止床の検証用オブジェクト。
class DFloorObject : public DGameObject
{
protected:
	// 剛体とコライダーを所有へ追加する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		FBodyDescription2D Ground;
		Ground.Type = EBodyType::Static;
		auto Body = AddComponent<DRigidBody2DComponent>(Ground);
		if (!Body)
		{
			return TResult<void>::Failure(Body.Error());
		}
		FColliderDescription2D Floor;
		FOrientedBox2D Shape;
		Shape.Center = {0, -1};
		Shape.HalfExtents = {5, 1};
		Floor.Shape = Shape;
		Floor.Friction = 0.4f;
		auto Collider = AddComponent<DCollider2DComponent>(Floor);
		if (!Collider)
		{
			return TResult<void>::Failure(Collider.Error());
		}
		return {};
	}
};
// 上昇する移動床の検証用オブジェクト。
class DPlatformObject : public DGameObject
{
protected:
	// 剛体とコライダーを所有へ追加する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		FBodyDescription2D Rise;
		Rise.Type = EBodyType::Kinematic;
		Rise.Velocity = {0, 1};
		auto Body = AddComponent<DRigidBody2DComponent>(Rise);
		if (!Body)
		{
			return TResult<void>::Failure(Body.Error());
		}
		FColliderDescription2D Deck;
		FOrientedBox2D Shape;
		Shape.Center = {0, 0};
		Shape.HalfExtents = {2, 0.5f};
		Deck.Shape = Shape;
		Deck.Friction = 0.8f;
		auto Collider = AddComponent<DCollider2DComponent>(Deck);
		if (!Collider)
		{
			return TResult<void>::Failure(Collider.Error());
		}
		return {};
	}
};
// 物理シーンの実行環境を作る。
struct FSceneFixture
{
	// 検証対象が参照するバックエンド。
	FFakeBackend Backend;
	// 検証用の資源管理。
	FAssetService Assets{Backend, Backend, Backend};
	// 検証用の音声サービス。
	FAudioPlayer Audio{Backend};
	// 検証用のシーン遷移管理。
	FSceneNavigator Navigator{Assets, Audio};
	// 検証用の入力履歴。
	FInputStateTracker Tracker;
	// 初期化の実行環境を返す。
	FInitContext GetInit()
	{
		return {Assets};
	}
	// 更新の実行環境を返す。
	FTickContext GetTick(Toolbox::f64 DeltaSeconds)
	{
		// サンプリングしたフレーム時刻。
		FFrameTime Time;
		Time.DeltaSeconds = DeltaSeconds;
		Time.UnscaledDeltaSeconds = DeltaSeconds;
		return {Tracker.GetSnapshot(), Time};
	}
	// 遷移管理つきの更新実行環境を返す。
	FTickContext GetTickWithScenes(Toolbox::f64 DeltaSeconds)
	{
		// サンプリングしたフレーム時刻。
		FFrameTime Time;
		Time.DeltaSeconds = DeltaSeconds;
		Time.UnscaledDeltaSeconds = DeltaSeconds;
		FTickContext Context{Tracker.GetSnapshot(), Time};
		Context.Scenes = &Navigator;
		return Context;
	}
	// 指定キーを押した入力を一段進める。
	void PressKey(EKey Key)
	{
		FRawInput Raw;
		Raw.Keys[static_cast<Toolbox::size_t>(Key)] = true;
		Tracker.Advance(Raw);
	}
	// 入力を離した状態へ一段進める。
	void ReleaseAll()
	{
		FRawInput Raw;
		Tracker.Advance(Raw);
	}
};
// 落下する円と床を持つシーンを作る。
void PrepareFallScene_Internal(FSceneFixture& Fixture, DPhysicsScene2D& Scene)
{
	REQUIRE(Scene.Spawn<DFloorObject>());
	REQUIRE(Scene.Spawn<DBallObject>());
	REQUIRE(Scene.Initialize_Internal(Fixture.GetInit()));
}
// 円の描画位置を探す。
Toolbox::FVector2 FindBallRenderPosition_Internal(DPhysicsScene2D& Scene)
{
	auto Ball = Scene.FindObject<DBallObject>();
	REQUIRE(Ball.Get() != nullptr);
	auto Body = Ball.Get()->FindComponent<DRigidBody2DComponent>();
	REQUIRE(Body.Get() != nullptr);
	return Body.Get()->GetRenderPosition();
}
// 落下する球と床を持つ3D検証用オブジェクト。
class DBallObject3D : public DGameObject
{
protected:
	// 剛体とコライダーを所有へ追加する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		FBodyDescription3D Fall;
		Fall.Position = {0, 5, 0};
		auto Body = AddComponent<DRigidBody3DComponent>(Fall);
		if (!Body)
		{
			return TResult<void>::Failure(Body.Error());
		}
		FColliderDescription3D Ball;
		FSphere Shape;
		Shape.Center = {0, 0, 0};
		Shape.Radius = 0.5f;
		Ball.Shape = Shape;
		Ball.Friction = 0.4f;
		auto Collider = AddComponent<DCollider3DComponent>(Ball);
		if (!Collider)
		{
			return TResult<void>::Failure(Collider.Error());
		}
		return {};
	}
};
// 静止床の3D検証用オブジェクト。
class DFloorObject3D : public DGameObject
{
protected:
	// 剛体とコライダーを所有へ追加する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		FBodyDescription3D Ground;
		Ground.Type = EBodyType::Static;
		auto Body = AddComponent<DRigidBody3DComponent>(Ground);
		if (!Body)
		{
			return TResult<void>::Failure(Body.Error());
		}
		FColliderDescription3D Floor;
		FOBB Shape;
		Shape.Center = {0, -1, 0};
		Shape.HalfExtents = {5, 1, 5};
		Floor.Shape = Shape;
		Floor.Friction = 0.4f;
		auto Collider = AddComponent<DCollider3DComponent>(Floor);
		if (!Collider)
		{
			return TResult<void>::Failure(Collider.Error());
		}
		return {};
	}
};
} // namespace
TEST("Physics scene runs scheduled fixed steps per frame")
{
	FSceneFixture Fixture;
	FSceneCounters Counts;
	DPhysicsScene2D Scene;
	REQUIRE(Scene.Spawn<DProbeObject>(Counts));
	REQUIRE(Scene.Initialize_Internal(Fixture.GetInit()));
	Fixture.ReleaseAll();
	REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 30.0)));
	REQUIRE(Counts.FixedTick == 2);
	REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 240.0)));
	REQUIRE(Counts.FixedTick == 2);
}
TEST("Physics scene exposes press edges on first step only")
{
	FSceneFixture Fixture;
	FSceneCounters Counts;
	DPhysicsScene2D Scene;
	REQUIRE(Scene.Spawn<DProbeObject>(Counts));
	REQUIRE(Scene.Initialize_Internal(Fixture.GetInit()));
	Fixture.PressKey(EKey::Space);
	REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 30.0)));
	REQUIRE(Counts.bPressedOnFirst);
	REQUIRE(!Counts.bPressedLater);
}
TEST("Physics scene keeps presses across zero step frames")
{
	FSceneFixture Fixture;
	FSceneCounters Counts;
	DPhysicsScene2D Scene;
	REQUIRE(Scene.Spawn<DProbeObject>(Counts));
	REQUIRE(Scene.Initialize_Internal(Fixture.GetInit()));
	// 蓄積が固定幅に満たない短いフレームを進める。
	Fixture.ReleaseAll();
	REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 240.0)));
	REQUIRE(Counts.FixedTick == 0);
	// 押下を含むフレームも固定更新が0回で未配達になる。
	Fixture.PressKey(EKey::Space);
	REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 240.0)));
	REQUIRE(Counts.FixedTick == 0);
	// 蓄積が届くまで進めると未配達分が一度だけ配達される。
	for (Toolbox::int32 Frame = 0; Frame < 6 && Counts.FixedTick == 0; ++Frame)
	{
		Fixture.ReleaseAll();
		REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 240.0)));
	}
	REQUIRE(Counts.FixedTick == 1);
	REQUIRE(Counts.bPressedFromPending);
	Fixture.ReleaseAll();
	REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 30.0)));
	REQUIRE(!Counts.bPressedLater);
}
TEST("Physics scene drops a ball onto the floor")
{
	FSceneFixture Fixture;
	DPhysicsScene2D Scene;
	PrepareFallScene_Internal(Fixture, Scene);
	Fixture.ReleaseAll();
	for (Toolbox::int32 Frame = 0; Frame < 180; ++Frame)
	{
		REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 60.0)));
	}
	// 半径0.5の円が床面に静止する。
	const Toolbox::FVector2 Rest = FindBallRenderPosition_Internal(Scene);
	REQUIRE(Rest.X > -0.1 && Rest.X < 0.1);
	REQUIRE(Rest.Y > 0.4 && Rest.Y < 0.62);
	Scene.Shutdown_Internal();
}
TEST("Physics scene uploads kinematic velocity to the world")
{
	FSceneFixture Fixture;
	DPhysicsScene2D Scene;
	REQUIRE(Scene.Spawn<DPlatformObject>());
	REQUIRE(Scene.Initialize_Internal(Fixture.GetInit()));
	Fixture.ReleaseAll();
	for (Toolbox::int32 Frame = 0; Frame < 60; ++Frame)
	{
		REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 60.0)));
	}
	// 毎秒1mの指定速度で1秒上昇する。
	auto Platform = Scene.FindObject<DPlatformObject>();
	REQUIRE(Platform.Get() != nullptr);
	auto Body = Platform.Get()->FindComponent<DRigidBody2DComponent>();
	REQUIRE(Body.Get() != nullptr);
	const Toolbox::FVector2 Rest = Body.Get()->GetRenderPosition();
	REQUIRE(Rest.Y > 0.9 && Rest.Y < 1.1);
	Scene.Shutdown_Internal();
}
TEST("Physics scene teleport resets interpolation immediately")
{
	FSceneFixture Fixture;
	DPhysicsScene2D Scene;
	PrepareFallScene_Internal(Fixture, Scene);
	Fixture.ReleaseAll();
	for (Toolbox::int32 Frame = 0; Frame < 30; ++Frame)
	{
		REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 60.0)));
	}
	auto Ball = Scene.FindObject<DBallObject>();
	REQUIRE(Ball.Get() != nullptr);
	auto Body = Ball.Get()->FindComponent<DRigidBody2DComponent>();
	REQUIRE(Body.Get() != nullptr);
	Body.Get()->Teleport({10, 10}, 0);
	// 補間残りがあっても描画位置は目標に一致する。
	const Toolbox::FVector2 Moved = Body.Get()->GetRenderPosition();
	REQUIRE(Moved.X > 9.9 && Moved.X < 10.1);
	REQUIRE(Moved.Y > 9.9 && Moved.Y < 10.1);
	Scene.Shutdown_Internal();
}
TEST("Physics scene survives destroy requested in fixed tick")
{
	FSceneFixture Fixture;
	FSceneCounters Counts;
	DPhysicsScene2D Scene;
	REQUIRE(Scene.Spawn<DProbeObject>(Counts));
	auto BallHandle = Scene.Spawn<DBallObject>().Value();
	REQUIRE(Scene.Spawn<DFloorObject>());
	REQUIRE(Scene.Initialize_Internal(Fixture.GetInit()));
	// 固定更新の中で円の破棄を要求する。
	Counts.FixedTick = 0;
	Fixture.ReleaseAll();
	REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 60.0)));
	REQUIRE(BallHandle.Get() != nullptr);
	BallHandle.Get()->Destroy();
	REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 60.0)));
	Scene.Shutdown_Internal();
	REQUIRE(Counts.FixedTick > 0);
}
TEST("Physics scene drops a sphere onto the floor in 3D")
{
	FSceneFixture Fixture;
	DPhysicsScene3D Scene;
	REQUIRE(Scene.Spawn<DFloorObject3D>());
	REQUIRE(Scene.Spawn<DBallObject3D>());
	REQUIRE(Scene.Initialize_Internal(Fixture.GetInit()));
	Fixture.ReleaseAll();
	for (Toolbox::int32 Frame = 0; Frame < 180; ++Frame)
	{
		REQUIRE(Scene.Tick_Internal(Fixture.GetTick(1.0 / 60.0)));
	}
	auto Ball = Scene.FindObject<DBallObject3D>();
	REQUIRE(Ball.Get() != nullptr);
	auto Body = Ball.Get()->FindComponent<DRigidBody3DComponent>();
	REQUIRE(Body.Get() != nullptr);
	// 半径0.5の球が床面に静止する。
	const Toolbox::FVector3 Rest = Body.Get()->GetRenderPosition();
	REQUIRE(Rest.X > -0.1 && Rest.X < 0.1);
	REQUIRE(Rest.Y > 0.4 && Rest.Y < 0.62);
	REQUIRE(Rest.Z > -0.1 && Rest.Z < 0.1);
	Scene.Shutdown_Internal();
}
TEST("Physics scene stops stepping after quit is requested")
{
	FSceneFixture Fixture;
	FSceneCounters Counts;
	DPhysicsScene2D Scene;
	REQUIRE(Scene.Spawn<DProbeObject>(Counts));
	REQUIRE(Scene.Spawn<DBallObject>());
	REQUIRE(Scene.Spawn<DFloorObject>());
	REQUIRE(Scene.Initialize_Internal(Fixture.GetInit()));
	Fixture.ReleaseAll();
	// 終了要求を出すと最初の配布で固定更新を打ち切る。
	Fixture.Navigator.RequestQuit();
	REQUIRE(Scene.Tick_Internal(Fixture.GetTickWithScenes(1.0 / 30.0)));
	REQUIRE(Counts.FixedTick == 1);
	// ワールド更新が行われないため円は初期位置に残る。
	const Toolbox::FVector2 Rest = FindBallRenderPosition_Internal(Scene);
	REQUIRE(Rest.Y > 4.9 && Rest.Y < 5.1);
	Scene.Shutdown_Internal();
}
