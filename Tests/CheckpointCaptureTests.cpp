// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Support/CheckpointFixture.h"
#include "Dxf/CheckpointCapture2D.h"
#include "Dxf/CheckpointCapture3D.h"
#include "Dxf/CheckpointCodec.h"
#include "Dxf/InputStateTracker.h"
#ifdef DXF_ALLOCATION_FAULT_TEST_EXECUTABLE
#include "../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
#endif
using namespace Dxf;
using namespace Toolbox;
using namespace TestCheckpointFixture;

namespace
{
// ゲーム状態の反映後だけ採取する、本体ContentSceneを使った境界の対照。
template <typename TScene, typename TCapture, typename TCatalog>
class TCaptureScene : public TScene
{
public:
	TCaptureScene(const TCatalog& Catalog, FAssetService& Assets) : TScene(PrepareScene(Catalog.Scene, Assets)), Capture(Catalog), TestAssets(Assets)
	{
	}
	~TCaptureScene() override
	{
		this->Shutdown_Internal();
	}
	// Application境界を再現するための所有側資源。
	FAssetService& TestAssets;
	// 所有はSceneに置き、外部の終了済み参加者を捕捉しない。
	TCapture Capture;
	// 同じ境界のゲーム進行値。
	FString Payload = "{\"score\":0}";
	// テストだけの成功Post後要求。
	TFunction<void(TCaptureScene&)> AfterPost;
	// 注入は採取処理の範囲だけに限定し、後続の可変更新と分ける。
	TFunction<void()> AfterCapture;
	// 初期化完了とBody登録後に一度だけ参照を解決する。
	bool Bound = false;

protected:
	void OnPhysicsStepCompleted(const FFixedTickContext&) override
	{
		if (!Bound)
		{
			Capture.Bind(*this);
			Bound = true;
		}
		if (AfterPost)
		{
			AfterPost(*this);
		}
		struct FEndCapture
		{
			TCaptureScene& Scene;
			~FEndCapture()
			{
				if (Scene.AfterCapture)
				{
					Scene.AfterCapture();
				}
			}
		} End{*this};
		Capture.CaptureIfRequested(*this, Payload);
	}
	void OnDeinitialize() noexcept override
	{
		Capture.Retire();
		TScene::OnDeinitialize();
	}
};
template <typename TScene>
void CaptureTick(TScene& Scene, f64 Delta = 1.0 / 60.0, bool Commit = true)
{
	if (Commit)
	{
		REQUIRE(Scene.GetChildren_Internal()->CommitBoundary_Internal({Scene.TestAssets}));
	}
	FInputStateTracker Input;
	FFrameTime Time;
	Time.DeltaSeconds = Delta;
	Time.UnscaledDeltaSeconds = Delta;
	Time.bPaused = Scene.GetClock().IsPaused();
	REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
}
template <typename TScene, typename TCatalog>
void CaptureBoundary(const TCatalog& C)
{
	Dxf::Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	TScene Scene(C, Assets);
	REQUIRE(Scene.Initialize_Internal({Assets}));
	REQUIRE(Scene.Capture.RequestCapture());
	CaptureTick(Scene, 0);
	REQUIRE(!Scene.Capture.GetLastCapture());
	REQUIRE(!Scene.Capture.CaptureIfRequested(Scene, "{}"));
	CaptureTick(Scene);
	REQUIRE(Scene.Capture.GetCapturedStep() == 1);
	REQUIRE(Scene.Capture.GetLastCapture()->GamePayload == "{\"score\":0}");
	// 全4種類の内部と個体間状態が、同じ成功Stepに属する。
	REQUIRE(Scene.Capture.GetLastCapture()->Joints.Size() == 16);
	const auto Joint = Scene.GetPrefab("doorA").Get()->GetPrismaticJoint("prismatic");
	const auto OldId = Joint.Get()->GetJointId();
	Scene.AfterPost = [Joint](TScene& S)
	{
		if (S.GetCompletedPhysicsStepCount() == 2)
		{
			S.Payload = "{\"score\":7}";
			Joint.Get()->RequestDrive({true, 0.4, 5});
		}
	};
	REQUIRE(Scene.Capture.RequestCapture("north"));
	REQUIRE(Scene.Capture.HasCaptureRequest());
	CaptureTick(Scene);
	REQUIRE(Joint.Get()->HasPendingRequest());
	REQUIRE(Scene.Capture.GetCapturedStep() == 1);
	REQUIRE(Scene.Capture.GetLastCapture()->GamePayload == "{\"score\":0}");
	CaptureTick(Scene);
	REQUIRE(!Joint.Get()->HasPendingRequest());
	REQUIRE(Scene.Capture.GetCapturedStep() == 3);
	REQUIRE(Scene.Capture.GetLastCapture()->Identity.CheckpointKey == "north");
	REQUIRE(!Scene.Capture.HasCaptureRequest());
	REQUIRE(Scene.Capture.GetLastCapture()->GamePayload == "{\"score\":7}");
	REQUIRE(Joint.Get()->GetJointId() && OldId && *Joint.Get()->GetJointId() == *OldId);
	bool Found = false;
	for (const auto& J : Scene.Capture.GetLastCapture()->Joints)
	{
		if (J.Instance == "doorA" && J.Kind == EJointKind::Prismatic)
		{
			REQUIRE(J.LinearDrive.TargetSpeed == 0.4 && J.LinearDrive.MaxForce == 5);
			Found = true;
		}
	}
	REQUIRE(Found);
	// 保留解除も同じ境界へ混ぜず、反映後に解除として保存する。
	const auto Distance = Scene.GetPrefab("doorB").Get()->GetDistanceJoint("distance");
	Distance.Get()->RequestDisconnect();
	REQUIRE(Distance.Get()->HasPendingRequest());
	REQUIRE(Scene.Capture.RequestCapture());
	CaptureTick(Scene);
	REQUIRE(!Distance.Get()->HasPendingRequest());
	Found = false;
	for (const auto& J : Scene.Capture.GetLastCapture()->Joints)
	{
		if (J.Instance == "doorB" && J.Kind == EJointKind::Distance)
		{
			REQUIRE(!J.bConnected);
			Found = true;
		}
	}
	REQUIRE(Found);
	const auto Step = Scene.GetCompletedPhysicsStepCount();
	const auto Bytes = FCheckpointCodec::Encode(*Scene.Capture.GetLastCapture());
	Scene.GetClock().SetPaused(true);
	CaptureTick(Scene);
	REQUIRE(Scene.GetCompletedPhysicsStepCount() == Step);
	REQUIRE(FCheckpointCodec::Encode(*Scene.Capture.GetLastCapture()).Payload == Bytes.Payload);
	// 要求なしのPollと終了は追加確保しない。World全体の無確保を測ったものではない。
#ifdef DXF_ALLOCATION_FAULT_TEST_EXECUTABLE
	Toolbox::Testing::SetAllocationFailureCountdown(0);
#endif
	const bool Idle = Scene.Capture.CaptureIfRequested(Scene, "{bad}");
#ifdef DXF_ALLOCATION_FAULT_TEST_EXECUTABLE
	const bool Injected = Toolbox::Testing::WasAllocationFailureInjected();
	Toolbox::Testing::SetAllocationFailureCountdown(-1);
	REQUIRE(!Injected);
#endif
	REQUIRE(!Idle);
	Scene.Shutdown_Internal();
	REQUIRE(!Scene.Capture.RequestCapture());
	REQUIRE(Scene.Capture.GetLastCapture());
}
template <typename TScene, typename TCatalog, typename TInstance>
void CaptureLifetime(const TCatalog& C)
{
	Dxf::Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	TScene Scene(C, Assets);
	REQUIRE(Scene.Initialize_Internal({Assets}));
	REQUIRE(Scene.Capture.RequestCapture());
	CaptureTick(Scene);
	const auto Old = Scene.GetPrefab("removedKey");
	REQUIRE(Scene.Capture.Destroy(Scene, "removedKey"));
	REQUIRE(!Old);
	TInstance New;
	New.Key = "cargo";
	New.Prefab = "mechanism";
	New.DefinitionFingerprint = Stamp;
	New.bDynamic = true;
	New.Placement.Position.X = 30;
	const auto Spawned = Scene.Capture.Spawn(Scene, New, PreparePrefab(C.Prefabs[0].Definition, Assets));
	REQUIRE(Spawned);
	REQUIRE(Scene.Capture.RequestCapture());
	CaptureTick(Scene, 1.0 / 60.0, false);
	REQUIRE(Scene.Capture.GetCapturedStep() == 1);
	// Spawn受付は初期化・Body登録・成功観察とは別。次の固定更新まで待つ。
	CaptureTick(Scene);
	const auto& Saved = *Scene.Capture.GetLastCapture();
	REQUIRE(Saved.Instances.Size() == 4);
	REQUIRE(!Saved.Instances[2].bPresent && Saved.Instances[3].bDynamic);
	REQUIRE(Saved.Instances[3].Placement.Position.X == 30);
	REQUIRE(FCheckpointRestore::Build(Saved, C).Scene.Instances.Size() == 3);
	// 同じslotへ新個体を置いても、以前のhandleと履歴を自動接続しない。
	REQUIRE(Scene.Destroy(Spawned.Value()));
	REQUIRE(!Spawned.Value());
	REQUIRE(Scene.SpawnPrefab("cargo", PreparePrefab(C.Prefabs[0].Definition, Assets), New.Placement));
	REQUIRE(Scene.Capture.RequestCapture());
	const auto Previous = Scene.Capture.GetCapturedStep();
	CaptureTick(Scene);
	REQUIRE(Scene.Capture.GetCapturedStep() == Previous);
	REQUIRE(!Scene.Capture.Destroy(Scene, "cargo"));
	Scene.Shutdown_Internal();
}
using FCaptureScene2D = TCaptureScene<DContentScene2D, FCheckpointCapture2D, FCheckpointCatalog2D>;
using FCaptureScene3D = TCaptureScene<DContentScene3D, FCheckpointCapture3D, FCheckpointCatalog3D>;
} // namespace
#ifndef DXF_CHECKPOINT_FAULT_ONLY
TEST("Checkpoint capture 2D waits for successful Post and pending settings")
{
	CaptureBoundary<FCaptureScene2D>(Catalog2D());
}
TEST("Checkpoint capture 3D waits for successful Post and pending settings")
{
	CaptureBoundary<FCaptureScene3D>(Catalog3D());
}
TEST("Checkpoint capture 2D tracks tombstone spawn and refuses same slot new generation")
{
	CaptureLifetime<FCaptureScene2D, FCheckpointCatalog2D, FCheckpointInstance2D>(Catalog2D());
}
TEST("Checkpoint capture 3D tracks tombstone spawn and refuses same slot new generation")
{
	CaptureLifetime<FCaptureScene3D, FCheckpointCatalog3D, FCheckpointInstance3D>(Catalog3D());
}
#endif
#ifdef DXF_ALLOCATION_FAULT_TEST_EXECUTABLE
TEST("Checkpoint capture allocation failure keeps last successful document and retries next Step")
{
	const auto C = Catalog2D();
	Dxf::Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FCaptureScene2D Scene(C, Assets);
	REQUIRE(Scene.Initialize_Internal({Assets}));
	REQUIRE(Scene.Capture.RequestCapture());
	CaptureTick(Scene);
	const auto Old = FCheckpointCodec::Encode(*Scene.Capture.GetLastCapture());
	Scene.AfterPost = [](FCaptureScene2D&)
	{
		Toolbox::Testing::SetAllocationFailureCountdown(0);
	};
	REQUIRE(Scene.Capture.RequestCapture());
	bool Failed = false;
	try
	{
		CaptureTick(Scene);
	}
	catch (...)
	{
		Failed = true;
	}
	const bool Actual = Toolbox::Testing::WasAllocationFailureInjected();
	Toolbox::Testing::SetAllocationFailureCountdown(-1);
	REQUIRE(Failed && Actual);
	REQUIRE(Scene.Capture.GetCapturedStep() == 1);
	REQUIRE(FCheckpointCodec::Encode(*Scene.Capture.GetLastCapture()).Payload == Old.Payload);
	Scene.AfterPost = {};
	Scene.Payload = "{\"score\":99}";
	CaptureTick(Scene);
	REQUIRE(Scene.Capture.GetCapturedStep() == 3);
	REQUIRE(Scene.Capture.GetLastCapture()->GamePayload == Scene.Payload);
	Scene.Shutdown_Internal();
}

namespace
{
// 小さい文書で実際の採取確保を一地点ずつ失敗させ、最初の非注入終了で止める。
template <typename TScene, typename TCatalog>
void SweepCapture(TCatalog C, const char* Dimension)
{
	C.Scene.Instances.Resize(1);
	C.Scene.Prefabs.Resize(1);
	C.Scene.Placements.Resize(1);
	C.InitialPrefabKeys.Resize(1);
	C.Scene.Connections.Clear();
	// 物理の4種類は保持し、論理公開先だけを代表の一種類へ絞る。
	C.Scene.Prefabs[0].Exports.Resize(3);
	C.Prefabs[0].Definition.Exports = C.Scene.Prefabs[0].Exports;
	Dxf::Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	TScene Scene(C, Assets);
	REQUIRE(Scene.Initialize_Internal({Assets}));
	REQUIRE(Scene.Capture.RequestCapture());
	CaptureTick(Scene);
	Scene.Payload = "{\"score\":99}";
	uint64 BeforeCapture = 0;
	uint64 AllocationCount = 0;
	Scene.AfterPost = [&BeforeCapture](TScene&)
	{
		BeforeCapture = Toolbox::Testing::GetTotalTestAllocations();
	};
	Scene.AfterCapture = [&BeforeCapture, &AllocationCount]()
	{
		AllocationCount = Toolbox::Testing::GetTotalTestAllocations() - BeforeCapture;
	};
	REQUIRE(Scene.Capture.RequestCapture());
	CaptureTick(Scene);
	Out << FString("CHECKPOINT_CAPTURE_BASELINE ") + Dimension + " allocations=" + ToString(AllocationCount) << '\n';
	REQUIRE(AllocationCount > 0 && AllocationCount < 8192);
	const auto Old = FCheckpointCodec::Encode(*Scene.Capture.GetLastCapture());
	Scene.Payload = "{\"score\":77}";
	bool Finished = false;
	for (int64 Countdown = 0; Countdown <= static_cast<int64>(AllocationCount); ++Countdown)
	{
		Scene.AfterPost = [Countdown](TScene&)
		{
			Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
		};
		bool Actual = false;
		Scene.AfterCapture = [&Actual]()
		{
			Actual = Toolbox::Testing::WasAllocationFailureInjected();
			Toolbox::Testing::SetAllocationFailureCountdown(-1);
		};
		const auto PreviousStep = Scene.Capture.GetCapturedStep();
		const auto Before = Toolbox::Testing::GetOutstandingTestAllocations();
		bool Failed = false;
		REQUIRE(Scene.Capture.RequestCapture());
		try
		{
			CaptureTick(Scene);
		}
		catch (...)
		{
			Failed = true;
		}
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
		REQUIRE(Actual ? Failed : !Failed);
		if (Actual)
		{
			REQUIRE(Scene.Capture.GetCapturedStep() == PreviousStep);
			REQUIRE(FCheckpointCodec::Encode(*Scene.Capture.GetLastCapture()).Payload == Old.Payload);
			REQUIRE(Toolbox::Testing::GetOutstandingTestAllocations() == Before);
		}
		Out << FString("CHECKPOINT_CAPTURE_FAULT ") + Dimension + " countdown=" + ToString(Countdown) + " injected=" + ToString(Actual ? 1 : 0) + " failed=" + ToString(Failed ? 1 : 0) << '\n';
		if (!Actual)
		{
			REQUIRE(Scene.Capture.GetLastCapture()->GamePayload == Scene.Payload);
			Finished = true;
			break;
		}
	}
	REQUIRE(Finished);
	Scene.AfterPost = {};
	Scene.AfterCapture = {};
	// 終了は新規確保しない。旧参加者へ配送するcallbackを保持しない。
	Toolbox::Testing::SetAllocationFailureCountdown(0);
	Scene.Capture.Retire();
	const bool Actual = Toolbox::Testing::WasAllocationFailureInjected();
	Toolbox::Testing::SetAllocationFailureCountdown(-1);
	REQUIRE(!Actual && !Scene.Capture.RequestCapture());
	Scene.Shutdown_Internal();
}
} // namespace
TEST("Checkpoint capture faults 2D keep last document through first non injected attempt")
{
	SweepCapture<FCaptureScene2D>(Catalog2D(), "2D");
}
TEST("Checkpoint capture faults 3D keep last document through first non injected attempt")
{
	SweepCapture<FCaptureScene3D>(Catalog3D(), "3D");
}

#endif
