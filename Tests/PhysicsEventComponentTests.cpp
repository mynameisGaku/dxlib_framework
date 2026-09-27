// SPDX-License-Identifier: NOASSERTION
// 接触・Triggerの配送（R3）を、実DPhysicsScene2D／3D・GameObject・固定更新で確かめる。
// 確認:
// 物理Stepの外での配送、固定更新ごとのバッチ（0回・複数回）、通知中の自己・相手の破棄、購読の変更、例外、終了の要求、
// イベントを有効にしていないシーンの拒否、Triggerの領域の出入り（同じBodyの複数Collider）。
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/ContactListenerComponent2D.h"
#include "Dxf/ContactListenerComponent3D.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/TriggerVolumeComponent2D.h"
#include "Dxf/TriggerVolumeComponent3D.h"
using namespace Dxf;
using namespace Dxf::Testing;
using namespace Toolbox;
namespace
{
// 2Dの型と配置。
struct FCase2D
{
	using FScene = DPhysicsScene2D;
	using FVector = Toolbox::FVector2;
	using FListener = DContactListener2DComponent;
	using FTrigger = DTriggerVolume2DComponent;
	using FTriggerDescription = FTriggerVolumeDescription2D;
	using FNotice = FContactNotice2D;
	using FBodyId = FBodyId2D;
	using FBodyDescription = FBodyDescription2D;
	using FColliderDescription = FColliderDescription2D;
	using FRigid = DRigidBody2DComponent;
	using FCollider = DCollider2DComponent;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y};
	}
	static f32 Y(FVector Value)
	{
		return Value.Y;
	}
	static FColliderDescription Ball(f32 Radius, FVector Center = {})
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FCircle2D{Center, Radius};
		return Description;
	}
	static FColliderDescription Box(f32 HalfX, f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOrientedBox2D{{0, 0}, {HalfX, HalfY}, 0};
		return Description;
	}
	static void Place(FScene& Scene, FBodyId Body, FVector Position)
	{
		Scene.GetPhysicsWorld().SetBodyTransform(Body, Position, 0);
	}
};
// 3Dの型と配置（Z=0の平面に2Dと同じ配置を作る）。
struct FCase3D
{
	using FScene = DPhysicsScene3D;
	using FVector = Toolbox::FVector3;
	using FListener = DContactListener3DComponent;
	using FTrigger = DTriggerVolume3DComponent;
	using FTriggerDescription = FTriggerVolumeDescription3D;
	using FNotice = FContactNotice3D;
	using FBodyId = FBodyId3D;
	using FBodyDescription = FBodyDescription3D;
	using FColliderDescription = FColliderDescription3D;
	using FRigid = DRigidBody3DComponent;
	using FCollider = DCollider3DComponent;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y, 0};
	}
	static f32 Y(FVector Value)
	{
		return Value.Y;
	}
	static FColliderDescription Ball(f32 Radius, FVector Center = {})
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FSphere{Center, Radius};
		return Description;
	}
	static FColliderDescription Box(f32 HalfX, f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOBB{{0, 0, 0}, {HalfX, HalfY, HalfX}};
		return Description;
	}
	static void Place(FScene& Scene, FBodyId Body, FVector Position)
	{
		Scene.GetPhysicsWorld().SetBodyTransform(Body, Position, Toolbox::FQuaternion{});
	}
};

// シーンの実行環境（CharacterMovementComponentTestsと同じ構成）。
struct FFixture
{
	FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FAudioPlayer Audio{Backend};
	FSceneNavigator Navigator{Assets, Audio};
	FInputStateTracker Tracker;
	FInitContext Init()
	{
		return {Assets};
	}
	FTickContext Tick(f64 DeltaSeconds)
	{
		FFrameTime Time;
		Time.DeltaSeconds = DeltaSeconds;
		Time.UnscaledDeltaSeconds = DeltaSeconds;
		FTickContext Context{Tracker.GetSnapshot(), Time};
		Context.Scenes = &Navigator;
		return Context;
	}
};

// 受け取った状態遷移を記録する監視のGameObject。
template <typename T> class TListenerObject : public DGameObject
{
public:
	Toolbox::TVector<typename T::FNotice> Notices;
	Toolbox::TFunction<void(const typename T::FNotice&)> OnNotice;
	typename T::FListener& Listener()
	{
		auto Found = FindComponent<typename T::FListener>();
		REQUIRE(Found.Get() != nullptr);
		return *Found.Get();
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		auto Added = AddComponent<typename T::FListener>(false);
		if (!Added)
		{
			return TResult<void>::Failure(Added.Error());
		}
		Added.Value().Get()->SetHandler(
		    [this](const typename T::FNotice& Notice)
		    {
			    Notices.PushBack(Notice);
			    if (OnNotice)
			    {
				    OnNotice(Notice);
			    }
		    });
		return {};
	}
};

// Triggerの領域のGameObject。
template <typename T> class TTriggerObject : public DGameObject
{
public:
	explicit TTriggerObject(typename T::FTriggerDescription Description) : m_Description(Description)
	{
	}
	Toolbox::TVector<typename T::FBodyId> Entered;
	Toolbox::TVector<typename T::FBodyId> Exited;
	Toolbox::TVector<EWorldEventEndReason> Reasons;
	Toolbox::TFunction<void(typename T::FBodyId)> OnEnter;
	typename T::FTrigger& Trigger()
	{
		auto Found = FindComponent<typename T::FTrigger>();
		REQUIRE(Found.Get() != nullptr);
		return *Found.Get();
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		auto Added = AddComponent<typename T::FTrigger>(m_Description);
		if (!Added)
		{
			return TResult<void>::Failure(Added.Error());
		}
		Added.Value().Get()->SetEnterHandler(
		    [this](typename T::FBodyId Body)
		    {
			    Entered.PushBack(Body);
			    if (OnEnter)
			    {
				    OnEnter(Body);
			    }
		    });
		Added.Value().Get()->SetExitHandler(
		    [this](typename T::FBodyId Body, EWorldEventEndReason Reason)
		    {
			    Exited.PushBack(Body);
			    Reasons.PushBack(Reason);
		    });
		return {};
	}

private:
	typename T::FTriggerDescription m_Description;
};

template <typename TScene> void Boundary_Internal(FFixture& Fixture, TScene& Scene)
{
	auto* Children = Scene.GetChildren_Internal();
	REQUIRE(Children != nullptr);
	Children->FreezeBoundary_Internal();
	REQUIRE(Children->CommitBoundary_Internal(Fixture.Init()));
}

FWorldEventSettings Events_Internal()
{
	FWorldEventSettings Settings;
	Settings.bEnabled = true;
	return Settings;
}

// 領域（中心x=0、半幅1）と、その外に置いたKinematicの球。
template <typename T> struct FZoneScene
{
	FFixture Fixture;
	typename T::FScene Scene;
	TTriggerObject<T>* Zone = nullptr;
	TListenerObject<T>* Watcher = nullptr;
	typename T::FBodyId Mover;
	FZoneScene()
	{
		Scene.GetPhysicsWorld().SetEventSettings(Events_Internal());
		typename T::FTriggerDescription Description;
		Description.Shape = T::Box(1, 1).Shape;
		auto ZoneObject = Scene.template Spawn<TTriggerObject<T>>(Description);
		REQUIRE(ZoneObject);
		Zone = ZoneObject.Value().Get();
		auto WatcherObject = Scene.template Spawn<TListenerObject<T>>();
		REQUIRE(WatcherObject);
		Watcher = WatcherObject.Value().Get();
		REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
		typename T::FBodyDescription Body;
		Body.Type = EBodyType::Kinematic;
		Body.Position = T::At(-5);
		Mover = Scene.GetPhysicsWorld().CreateBody(Body);
		(void)Scene.GetPhysicsWorld().AttachCollider(Mover, T::Ball(0.5f));
		Watcher->Listener().WatchBody(Mover);
	}
	~FZoneScene()
	{
		Scene.Shutdown_Internal();
	}
};

// 入る・出るは各1回。1描画フレームに固定更新3回なら3つのバッチを順に配送し、0回のフレームは配送しない。
template <typename T> void DeliversEachFixedStep_Internal()
{
	FZoneScene<T> Zone;
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Watcher->Notices.IsEmpty() && Zone.Zone->Trigger().GetOccupantCount() == 0);
	const uint64 Before = Zone.Watcher->Listener().GetLastBatchId();
	// 固定更新0回のフレーム：配送なし。
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 240.0)));
	REQUIRE(Zone.Watcher->Listener().GetLastBatchId() == Before);
	T::Place(Zone.Scene, Zone.Mover, T::At(0));
	// 残りの3/240と合わせて固定更新3回。1回目でBegin、2・3回目でStay。
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 20.0 - 1.0 / 240.0)));
	REQUIRE(Zone.Watcher->Listener().GetLastBatchId() == Before + 3);
	REQUIRE(Zone.Watcher->Notices.Size() == 3);
	REQUIRE(Zone.Watcher->Notices[0].Phase == EWorldEventPhase::Begin);
	REQUIRE(Zone.Watcher->Notices[1].Phase == EWorldEventPhase::Stay);
	REQUIRE(Zone.Watcher->Notices[2].Phase == EWorldEventPhase::Stay);
	REQUIRE(Zone.Watcher->Notices[0].StepIndex + 1 == Zone.Watcher->Notices[1].StepIndex);
	REQUIRE(Zone.Watcher->Notices[0].Self.Body == Zone.Mover &&
	        Zone.Watcher->Notices[0].Kind == EWorldEventKind::Trigger);
	REQUIRE(Zone.Zone->Entered.Size() == 1 && Zone.Zone->Trigger().Contains(Zone.Mover));
	T::Place(Zone.Scene, Zone.Mover, T::At(5));
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Watcher->Notices.Back().Phase == EWorldEventPhase::End);
	REQUIRE(Zone.Zone->Exited.Size() == 1 && Zone.Zone->Reasons[0] == EWorldEventEndReason::Separated);
	REQUIRE(Zone.Zone->Trigger().GetOccupantCount() == 0);
	// 同じバッチを描画フレーム側から重ねて配送しない（固定更新なしのフレームでも数は増えない）。
	const auto Count = Zone.Watcher->Notices.Size();
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 1000.0)));
	REQUIRE(Zone.Watcher->Notices.Size() == Count);
}

// 剛体とColliderと監視を持つ球（監視は所有者の剛体を自動で対象にする）。
template <typename T> class TFallingBall : public DGameObject
{
public:
	Toolbox::TVector<typename T::FNotice> Notices;

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		typename T::FBodyDescription Body;
		Body.Position = T::At(0, 2);
		auto Rigid = AddComponent<typename T::FRigid>(Body);
		if (!Rigid)
		{
			return TResult<void>::Failure(Rigid.Error());
		}
		auto Collider = AddComponent<typename T::FCollider>(T::Ball(0.5f));
		if (!Collider)
		{
			return TResult<void>::Failure(Collider.Error());
		}
		auto Listener = AddComponent<typename T::FListener>();
		if (!Listener)
		{
			return TResult<void>::Failure(Listener.Error());
		}
		Listener.Value().Get()->SetHandler(
		    [this](const typename T::FNotice& Notice)
		    {
			    Notices.PushBack(Notice);
		    });
		return {};
	}
};

// 所有者の剛体を自動で監視する。同じ固定更新で作られたBodyも対象で、法線は相手（床）から受け取る側へ向く（上向き）。
template <typename T> void WatchesOwnerBody_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	Scene.GetPhysicsWorld().SetEventSettings(Events_Internal());
	auto Ball = Scene.template Spawn<TFallingBall<T>>();
	REQUIRE(Ball);
	REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
	typename T::FBodyDescription FloorBody;
	FloorBody.Type = EBodyType::Static;
	const auto Floor = Scene.GetPhysicsWorld().CreateBody(FloorBody);
	(void)Scene.GetPhysicsWorld().AttachCollider(Floor, T::Box(10, 0.5f));
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		REQUIRE(Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0)));
	}
	const auto& Notices = Ball.Value().Get()->Notices;
	int32 Begins = 0;
	for (const auto& Notice : Notices)
	{
		Begins += Notice.Phase == EWorldEventPhase::Begin ? 1 : 0;
		REQUIRE(Notice.Kind == EWorldEventKind::Contact && Notice.Other.Body == Floor);
		REQUIRE(Notice.Normal && T::Y(*Notice.Normal) > 0.9f);
	}
	REQUIRE(Begins == 1 && Notices.Size() > 30);
	Scene.Shutdown_Internal();
}

// 通知の中で自身・相手を破棄する。後続の自身の配送は止まり、破棄を要求された相手の監視は呼ばれない。
template <typename T> void DestroyDuringNotice_Internal()
{
	FZoneScene<T> Zone;
	auto Other = Zone.Scene.template Spawn<TListenerObject<T>>();
	REQUIRE(Other);
	TListenerObject<T>* Second = Other.Value().Get();
	Boundary_Internal(Zone.Fixture, Zone.Scene);
	Second->Listener().WatchBody(Zone.Mover);
	// 最初の監視は、最初の通知で自身と二つ目の監視の両方の破棄を要求する。
	Zone.Watcher->OnNotice = [&](const typename T::FNotice&)
	{
		Zone.Watcher->Destroy();
		Second->Destroy();
	};
	// 取得物の想定：入った時に領域のGameObjectを破棄する。
	Zone.Zone->OnEnter = [&](typename T::FBodyId)
	{
		Zone.Zone->Destroy();
	};
	T::Place(Zone.Scene, Zone.Mover, T::At(0));
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Watcher->Notices.Size() == 1 && Second->Notices.IsEmpty());
	REQUIRE(Zone.Zone->Entered.Size() == 1);
	// 境界で破棄を確定すると領域のSensorは削除され、以後は組がない。
	Boundary_Internal(Zone.Fixture, Zone.Scene);
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	const auto& Batch = Zone.Scene.GetPhysicsWorld().GetEventBatch();
	REQUIRE(Batch.bPublished && Batch.Events.Size() == 1);
	REQUIRE(Batch.Events[0].Phase == EWorldEventPhase::End &&
	        Batch.Events[0].EndReason == EWorldEventEndReason::Removed);
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Scene.GetPhysicsWorld().GetEventBatch().Events.IsEmpty());
}

// 通知の中の購読の変更：外した対象の残りは配送せず、加えた対象は同じバッチの以降のイベントから配送する。
template <typename T> void SubscriptionChanges_Internal()
{
	FZoneScene<T> Zone;
	typename T::FBodyDescription Body;
	Body.Type = EBodyType::Kinematic;
	Body.Position = T::At(0.5f);
	const auto Late = Zone.Scene.GetPhysicsWorld().CreateBody(Body);
	(void)Zone.Scene.GetPhysicsWorld().AttachCollider(Late, T::Ball(0.2f));
	bool bFirst = true;
	Zone.Watcher->OnNotice = [&](const typename T::FNotice&)
	{
		if (bFirst)
		{
			bFirst = false;
			Zone.Watcher->Listener().UnwatchBody(Zone.Mover);
			Zone.Watcher->Listener().WatchBody(Late);
		}
	};
	T::Place(Zone.Scene, Zone.Mover, T::At(-0.5f));
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	// Mover（スロットが先）で1件、その後に加えたLateで1件。
	REQUIRE(Zone.Watcher->Notices.Size() == 2);
	REQUIRE(Zone.Watcher->Notices[0].Self.Body == Zone.Mover && Zone.Watcher->Notices[1].Self.Body == Late);
	REQUIRE(!Zone.Watcher->Listener().IsWatching(Zone.Mover) && Zone.Watcher->Listener().IsWatching(Late));
	REQUIRE(Zone.Zone->Trigger().GetOccupantCount() == 2);
}

// 通知の例外は固定更新の失敗として伝わり、同じバッチを再配送しない。次のフレームは通常どおり。
template <typename T> void HandlerException_Internal()
{
	FZoneScene<T> Zone;
	bool bThrow = true;
	Zone.Watcher->OnNotice = [&](const typename T::FNotice&)
	{
		if (bThrow)
		{
			bThrow = false;
			throw FException("game handler failed");
		}
	};
	T::Place(Zone.Scene, Zone.Mover, T::At(0));
	const auto Failed = Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0));
	REQUIRE(!Failed);
	REQUIRE(Zone.Watcher->Notices.Size() == 1);
	const uint64 Batch = Zone.Watcher->Listener().GetLastBatchId();
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Watcher->Listener().GetLastBatchId() == Batch + 1);
	REQUIRE(Zone.Watcher->Notices.Size() == 2 && Zone.Watcher->Notices[1].Phase == EWorldEventPhase::Stay);
}

// 通知の中の終了の要求は、同じフレームの残りの固定更新を行わない。
template <typename T> void QuitDuringNotice_Internal()
{
	FZoneScene<T> Zone;
	Zone.Watcher->OnNotice = [&](const typename T::FNotice&)
	{
		Zone.Fixture.Navigator.RequestQuit();
	};
	T::Place(Zone.Scene, Zone.Mover, T::At(0));
	const uint64 Before = Zone.Scene.GetPhysicsWorld().CaptureSnapshot().StepIndex;
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 20.0)));
	REQUIRE(Zone.Scene.GetPhysicsWorld().CaptureSnapshot().StepIndex == Before + 1);
	REQUIRE(Zone.Watcher->Notices.Size() == 1);
}

// イベントを有効にしていない物理シーンでは、監視・領域は固定更新で明示的に失敗する（黙って有効化しない）。
template <typename T> void RequiresEnabledEvents_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	REQUIRE(Scene.template Spawn<TListenerObject<T>>());
	REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
	const auto Result = Scene.Tick_Internal(Fixture.Tick(1.0 / 60.0));
	REQUIRE(!Result);
	REQUIRE(!Scene.GetPhysicsWorld().GetEventSettings().bEnabled);
	Scene.Shutdown_Internal();
}

// 同じBodyの二つのColliderが入っている間は一つの占有で、両方が出た時に一度だけ出る（早く空にならない）。
template <typename T> void OccupancyPerBody_Internal()
{
	FZoneScene<T> Zone;
	typename T::FBodyDescription Body;
	Body.Type = EBodyType::Kinematic;
	Body.Position = T::At(-5);
	const auto Pair = Zone.Scene.GetPhysicsWorld().CreateBody(Body);
	(void)Zone.Scene.GetPhysicsWorld().AttachCollider(Pair, T::Ball(0.2f, T::At(-0.8f)));
	(void)Zone.Scene.GetPhysicsWorld().AttachCollider(Pair, T::Ball(0.2f, T::At(0.8f)));
	// 右のColliderだけが入る。
	T::Place(Zone.Scene, Pair, T::At(-1.6f));
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Zone->Entered.Size() == 1 && Zone.Zone->Trigger().GetOccupantCount() == 1);
	// 両方が入る。
	T::Place(Zone.Scene, Pair, T::At(0));
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Zone->Entered.Size() == 1);
	// 左のColliderだけが残る：まだ出ていない。
	T::Place(Zone.Scene, Pair, T::At(1.6f));
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Zone->Exited.IsEmpty() && Zone.Zone->Trigger().Contains(Pair));
	T::Place(Zone.Scene, Pair, T::At(5));
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Zone->Exited.Size() == 1 && Zone.Zone->Trigger().GetOccupantCount() == 0);
	// 相手のBodyの削除は、次の成功した固定更新でRemovedとして出る。
	T::Place(Zone.Scene, Pair, T::At(0));
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Scene.GetPhysicsWorld().DestroyBody(Pair));
	REQUIRE(Zone.Scene.Tick_Internal(Zone.Fixture.Tick(1.0 / 60.0)));
	REQUIRE(Zone.Zone->Exited.Size() == 2 && Zone.Zone->Reasons[1] == EWorldEventEndReason::Removed);
}
} // namespace

TEST("2D contact listener delivers one batch per fixed step")
{
	DeliversEachFixedStep_Internal<FCase2D>();
}
TEST("3D contact listener delivers one batch per fixed step")
{
	DeliversEachFixedStep_Internal<FCase3D>();
}
TEST("2D contact listener watches the owner rigid body")
{
	WatchesOwnerBody_Internal<FCase2D>();
}
TEST("3D contact listener watches the owner rigid body")
{
	WatchesOwnerBody_Internal<FCase3D>();
}
TEST("2D contact delivery survives destruction during a notice")
{
	DestroyDuringNotice_Internal<FCase2D>();
}
TEST("3D contact delivery survives destruction during a notice")
{
	DestroyDuringNotice_Internal<FCase3D>();
}
TEST("2D contact listener subscription changes during a notice")
{
	SubscriptionChanges_Internal<FCase2D>();
}
TEST("3D contact listener subscription changes during a notice")
{
	SubscriptionChanges_Internal<FCase3D>();
}
TEST("2D contact handler exception fails the fixed step without redelivery")
{
	HandlerException_Internal<FCase2D>();
}
TEST("3D contact handler exception fails the fixed step without redelivery")
{
	HandlerException_Internal<FCase3D>();
}
TEST("2D quit requested in a notice stops the remaining fixed steps")
{
	QuitDuringNotice_Internal<FCase2D>();
}
TEST("3D quit requested in a notice stops the remaining fixed steps")
{
	QuitDuringNotice_Internal<FCase3D>();
}
TEST("2D contact components require explicitly enabled world events")
{
	RequiresEnabledEvents_Internal<FCase2D>();
}
TEST("3D contact components require explicitly enabled world events")
{
	RequiresEnabledEvents_Internal<FCase3D>();
}
TEST("2D trigger volume counts a body once for many colliders")
{
	OccupancyPerBody_Internal<FCase2D>();
}
TEST("3D trigger volume counts a body once for many colliders")
{
	OccupancyPerBody_Internal<FCase3D>();
}
