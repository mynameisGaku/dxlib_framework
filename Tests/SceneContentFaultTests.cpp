// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/SceneContentParser.h"
#include "Dxf/SceneContentRequest.h"
#include "Dxf/ContentScene2D.h"
#include "Dxf/ContentScene3D.h"
#include "Dxf/InputStateTracker.h"
#include "../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
#include <stdio.h>
namespace
{
// 両次元で全種類を同じ二端点へ接続する。単位とFrameは各次元の実APIを通す。
const char* Text2D =
    R"({"schema":1,"kind":"prefab","dimension":2,"parts":[{"id":"a","body":{"type":"Static","position":[0,0]}},{"id":"b","body":{"type":"Dynamic","position":[0,1]},"colliders":[{"shape":"Box","halfExtents":[0.2,0.2]}]}],"joints":[{"id":"distance","kind":"Distance","bodyA":"a","bodyB":"b","length":1},{"id":"fixed","kind":"Fixed","bodyA":"a","bodyB":"b","frame":{"space":"Prefab","anchor":[0,1],"angle":0}},{"id":"revolute","kind":"Revolute","bodyA":"a","bodyB":"b","frame":{"space":"Prefab","anchor":[0,1],"angle":0}},{"id":"prismatic","kind":"Prismatic","bodyA":"a","bodyB":"b","frame":{"space":"Prefab","anchor":[0,1],"angle":0}}],"exports":{"body":"b/body","drive":"prismatic"}})";
const char* Text3D =
    R"({"schema":1,"kind":"prefab","dimension":3,"parts":[{"id":"a","body":{"type":"Static","position":[0,0,0]}},{"id":"b","body":{"type":"Dynamic","position":[0,1,0]},"colliders":[{"shape":"Box","halfExtents":[0.2,0.2,0.2]}]}],"joints":[{"id":"distance","kind":"Distance","bodyA":"a","bodyB":"b","length":1},{"id":"fixed","kind":"Fixed","bodyA":"a","bodyB":"b","frame":{"space":"Prefab","anchor":[0,1,0],"rotation":[0,0,0,1]}},{"id":"revolute","kind":"Revolute","bodyA":"a","bodyB":"b","frame":{"space":"Prefab","anchor":[0,1,0],"rotation":[0,0,0,1]}},{"id":"prismatic","kind":"Prismatic","bodyA":"a","bodyB":"b","frame":{"space":"Prefab","anchor":[0,1,0],"rotation":[0,0,0,1]}}],"exports":{"body":"b/body","drive":"prismatic"}})";
// 一地点だけを失敗させ、終了後に未解放件数を確認する。非注入の最初の試行で探索を終える。
template <typename F>
void Sweep(const char* Name, F Run)
{
	bool WarmInjected = false;
	Run(-1, WarmInjected);
	Toolbox::int32 Injected = 0;
	bool Completed = false;
	for (Toolbox::int64 Countdown = 0; Countdown < 2048; ++Countdown)
	{
		const auto Before = Toolbox::Testing::GetOutstandingTestAllocations();
		bool Failed = false;
		bool Actual = false;
		try
		{
			Run(Countdown, Actual);
		}
		catch (const Toolbox::FException&)
		{
			Failed = true;
		}
		Actual = Actual || Toolbox::Testing::WasAllocationFailureInjected();
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
		REQUIRE(Actual ? Failed : !Failed);
		REQUIRE(Toolbox::Testing::GetOutstandingTestAllocations() == Before);
		printf("CONTENT_FAULT %s countdown=%lld injected=%d failure=%d outstanding_delta=0\n", Name, Countdown, Actual, Failed);
		if (!Actual)
		{
			Completed = true;
			break;
		}
		++Injected;
		// 注入がない独立の回復試行。成功するまでの再試行ではない。
		bool RecoveryInjected = false;
		Run(-1, RecoveryInjected);
		REQUIRE(Toolbox::Testing::GetOutstandingTestAllocations() == Before);
	}
	REQUIRE(Completed && Injected > 0);
}
// Scene自身の寿命を試験のスコープへ結び、例外時も既存終了境界を通す。
struct FInjectionEnd
{
	bool& Actual;
	void End() noexcept
	{
		Actual = Actual || Toolbox::Testing::WasAllocationFailureInjected();
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
	}
	~FInjectionEnd()
	{
		End();
	}
};
template <typename TScene>
struct TSceneOwner
{
	TScene Scene;
	~TSceneOwner()
	{
		Scene.Shutdown_Internal();
	}
};
// JSONから実Worldまでの各境界を分けて注入する。
template <typename TScene, typename TInstance>
void RuntimeFaults(const char* Name, const char* Text)
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets{Backend, Backend, Backend};
	const auto Prepared = [&]()
	{
		if constexpr (Toolbox::IsSame<TInstance, Dxf::DPrefabInstance2D>)
		{
			return Dxf::PreparePrefab(Dxf::ParsePrefab2D(Text), Assets);
		}
		else
		{
			return Dxf::PreparePrefab(Dxf::ParsePrefab3D(Text), Assets);
		}
	}();
	for (Toolbox::int32 Stage = 0; Stage < 3; ++Stage)
	{
		const auto Label = Toolbox::FString(Name) + "-stage" + Toolbox::ToString(Stage);
		Sweep(Label.CStr(),
		      [&](Toolbox::int64 Countdown, bool& Actual)
		      {
			      TSceneOwner<TScene> Owner;
			      auto& Scene = Owner.Scene;
			      FInjectionEnd End{Actual};
			      if (Stage == 0)
			      {
				      Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
			      }
			      const auto Spawned = Scene.template Spawn<TInstance>(Prepared);
			      if (!Spawned)
			      {
				      throw Toolbox::FException(Spawned.Error().Message);
			      }
			      if (Stage == 0)
			      {
				      End.End();
			      }
			      if (Stage == 1)
			      {
				      Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
			      }
			      const auto Initialized = Scene.Initialize_Internal({Assets});
			      if (!Initialized)
			      {
				      throw Toolbox::FException(Initialized.Error().Message);
			      }
			      if (Stage == 1)
			      {
				      End.End();
			      }
			      Dxf::FInputStateTracker Input;
			      Dxf::FFrameTime Time;
			      Time.DeltaSeconds = 1.0 / 60.0;
			      Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
			      if (Stage == 2)
			      {
				      Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
			      }
			      const auto Tick = Scene.Tick_Internal({Input.GetSnapshot(), Time});
			      if (!Tick)
			      {
				      throw Toolbox::FException(Tick.Error().Message);
			      }
			      REQUIRE(Spawned.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
			      // 終了は別の境界。成功途中の未使用countdownを持ち込まない。
			      End.End();
		      });
	}
	TSceneOwner<TScene> Owner;
	const auto Spawned = Owner.Scene.template Spawn<TInstance>(Prepared);
	REQUIRE(Spawned && Owner.Scene.Initialize_Internal({Assets}));
	Dxf::FInputStateTracker Input;
	Dxf::FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	REQUIRE(Owner.Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	Toolbox::Testing::SetAllocationFailureCountdown(0);
	Owner.Scene.Shutdown_Internal();
	const auto DuringShutdown = Toolbox::Testing::WasAllocationFailureInjected();
	Toolbox::Testing::SetAllocationFailureCountdown(-1);
	REQUIRE(!DuringShutdown && !Spawned.Value());
	printf("CONTENT_SHUTDOWN %s injected=0 stale_handle=0\n", Name);
}
} // namespace
TEST("Content allocation faults JSON schema parameters references recover without leaks in both dimensions")
{
	Sweep("parse2d",
	      [](Toolbox::int64 Countdown, bool& Actual)
	      {
		      FInjectionEnd End{Actual};
		      Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
		      const auto Definition = Dxf::ParsePrefab2D(Text2D);
		      REQUIRE(Definition.Joints.Size() == 4);
	      });
	Sweep("parse3d",
	      [](Toolbox::int64 Countdown, bool& Actual)
	      {
		      FInjectionEnd End{Actual};
		      Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
		      const auto Definition = Dxf::ParsePrefab3D(Text3D);
		      REQUIRE(Definition.Joints.Size() == 4);
	      });
}
TEST("Content allocation faults 2D root children first Body Joint and shutdown preserve ownership")
{
	RuntimeFaults<Dxf::DPhysicsScene2D, Dxf::DPrefabInstance2D>("2d", Text2D);
}
TEST("Content allocation faults 3D root children first Body Joint and shutdown preserve ownership")
{
	RuntimeFaults<Dxf::DPhysicsScene3D, Dxf::DPrefabInstance3D>("3d", Text3D);
}

TEST("Content allocation faults reload preserve the previous accepted definition in both dimensions")
{
	for (const bool Is3D : {false, true})
	{
		Sweep(Is3D ? "reload3d" : "reload2d", [Is3D](Toolbox::int64 Countdown, bool& Actual)
		      {
			      Dxf::Testing::FFakeBackend Backend;
			      Dxf::FAssetService Assets{Backend, Backend, Backend};
			      Dxf::FSceneContentRequest Request{Dxf::FSceneContentSource{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)}};
			      const auto Old = Is3D ? Request.LoadScene3D("fault-scene3d-a.dxfscene.json") : Request.LoadScene2D("fault-scene2d-a.dxfscene.json");
			      Request.Poll(Assets);
			      REQUIRE(Old.GetState() == Dxf::ESceneContentRequestState::Ready);
			      const auto Sequence = Request.GetAcceptedSequence();
			      FInjectionEnd End{Actual};
			      Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
			      bool Failed = false;
			      try
			      {
				      const auto New = Is3D ? Request.LoadScene3D("fault-scene3d-b.dxfscene.json") : Request.LoadScene2D("fault-scene2d-b.dxfscene.json");
				      Request.Poll(Assets);
				      Failed = New.GetState() != Dxf::ESceneContentRequestState::Ready;
			      }
			      catch (const Toolbox::FException&)
			      {
				      Failed = true;
			      }
			      End.End();
			      if (Failed)
			      {
				      REQUIRE(Request.GetAcceptedSequence() == Sequence);
				      if (Is3D)
				      {
					      REQUIRE(Request.GetPrepared3D().Definition->Path == "fault-scene3d-a.dxfscene.json");
				      }
				      else
				      {
					      REQUIRE(Request.GetPrepared2D().Definition->Path == "fault-scene2d-a.dxfscene.json");
				      }
				      throw Toolbox::FException("Injected reload failure retained previous value");
			      }
		      });
	}
}

TEST("Content owner partial asset allocation faults retain independent shared resources")
{
	Sweep("owner-assets", [](Toolbox::int64 Countdown, bool& Actual)
	      {
		      Dxf::Testing::FFakeBackend Backend;
		      Dxf::FAssetService Assets{Backend, Backend, Backend};
		      Dxf::FContentAssetDefinition Image;
		      Image.Id = "existing";
		      Image.Path = "Assets/existing.png";
		      Dxf::FContentAssetDefinition Cue;
		      Cue.Id = "cue";
		      Cue.Kind = Dxf::EContentAssetKind::Sound;
		      Cue.Path = "Assets/cue.wav";
		      const auto Existing = Dxf::FContentResources::Prepare({Image}, Assets, "old.json");
		      const auto Definitions = Toolbox::TVector<Dxf::FContentAssetDefinition>{Image, Cue};
		      FInjectionEnd End{Actual};
		      Toolbox::Testing::SetAllocationFailureCountdown(Countdown);
		      try
		      {
			      const auto New = Dxf::FContentResources::Prepare(Definitions, Assets, "new.json");
			      // 準備成功後の解放は、この準備の故障探索へ含めない。
			      End.End();
		      }
		      catch (...)
		      {
			      End.End();
			      REQUIRE(Existing.GetTexture(0).IsValid());
			      throw;
		      }
		      End.End();
		      REQUIRE(Existing.GetTexture(0).IsValid());
	      });
}
