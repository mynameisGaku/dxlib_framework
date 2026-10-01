// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/ContentRenderBackend.h"
#include "Dxf/Application.h"
#include "Dxf/ContentScene2D.h"
#include "Dxf/ContentScene3D.h"
#include "Dxf/SceneContentSource.h"
namespace
{
// 同じ登録順・入力・時刻で表示数だけを変える。CPU描画境界であり画素検査ではない。
template <typename TScene>
auto CheckViews(bool Split)
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::Testing::FContentRenderBackend Renderer(Backend);
	Dxf::FApplicationSettings Settings;
	Settings.ProjectRoot = DXF_CONTENT_TEST_ROOT;
	Settings.Window.Width = 1001;
	Settings.Window.Height = 501;
	Settings.ExecutionThreadCount = 1;
	Dxf::FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer}, Settings);
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	const auto Prepared = [&]()
	{
		if constexpr (Toolbox::IsSame<TScene, Dxf::DContentScene2D>)
		{
			return Dxf::PrepareScene( Source.LoadScene2D(Split ? "scene2d-views.dxfscene.json" : "scene2d-a.dxfscene.json"), App.GetAssets());
		}
		else
		{
			return Dxf::PrepareScene( Source.LoadScene3D(Split ? "scene3d-views.dxfscene.json" : "scene3d-a.dxfscene.json"), App.GetAssets());
		}
	}();
	REQUIRE(Prepared.Definition->ViewCount == (Split ? 2u : 1u));
	auto Scene = Toolbox::MakeUnique<TScene>(Prepared);
	auto* Live = Scene.Get();
	REQUIRE(App.Start(Toolbox::Move(Scene)));
	const auto First = App.Step(0);
	REQUIRE(First && First.Value());
	REQUIRE(Live->GetPrefab("doorB").Get()->GetState() == Dxf::EPrefabInstanceState::PendingPhysics);
	if constexpr (Toolbox::IsSame<TScene, Dxf::DContentScene2D>)
	{
		REQUIRE(Renderer.Triangles.Size() == (Split ? 8u : 4u));
		const auto& Triangle = Renderer.Triangles[0];
		REQUIRE(Triangle.Options.Color.R == 80 && Triangle.Options.Color.G == 180);
		// 定義の半寸法0.4×0.8m、初期中心(0,1)m。
		REQUIRE(Toolbox::Abs(Triangle.A.X - (Split ? 230 : 380)) < 0.001);
		REQUIRE(Toolbox::Abs(Triangle.A.Y - (Split ? 290 : 290)) < 0.001);
		if (Split)
		{
			REQUIRE(Triangle.Options.bClip);
			REQUIRE(Triangle.Options.ClipRect.Right == 500);
			REQUIRE(Renderer.Triangles[4].Options.ClipRect.Left == 500);
			REQUIRE(Toolbox::Abs(Renderer.Triangles[4].A.X - 731) < 0.001);
		}
	}
	else
	{
		REQUIRE(Renderer.Views.Size() == (Split ? 2u : 1u));
		REQUIRE(!Renderer.Geometry.IsEmpty());
		if (Split)
		{
			REQUIRE(Renderer.Views[0].Viewport.Right == 500);
			REQUIRE(Renderer.Views[1].Viewport.Left == 500 && Renderer.Views[1].Viewport.Right == 1001);
			REQUIRE(Renderer.Views[0].Eye != Renderer.Views[1].Eye);
		}
	}
	for (Toolbox::uint32 Frame = 1; Frame <= 12; ++Frame)
	{
		const auto Continued = App.Step((Frame + 0.25) / 60.0);
		REQUIRE(Continued && Continued.Value());
	}
	const auto Prefab = Live->GetPrefab("doorB");
	REQUIRE(Prefab.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	const auto Body = Prefab.Get()->GetRigidBody(Toolbox::IsSame<TScene, Dxf::DContentScene2D> ? "doorBody" : "body");
	const auto Position = Live->GetPhysicsWorld().GetPosition(Body.Get()->GetBodyId());
	const auto Observation = [&]()
	{
		if constexpr (Toolbox::IsSame<TScene, Dxf::DContentScene2D>)
		{
			return Prefab.Get()->GetPrismaticJoint("doorDrive").Get()->GetObservation();
		}
		else
		{
			return Prefab.Get()->GetPrismaticJoint("drive").Get()->GetObservation();
		}
	}();
	REQUIRE(Observation);
	App.GetScenes().RequestQuit();
	const auto Stopped = App.Step(1);
	REQUIRE(Stopped && !Stopped.Value());
	REQUIRE(!Body && !Prefab);
	return Position;
}
} // namespace
TEST("Content 2D file views place colored geometry and keep identical physics in odd-sized split display")
{
	REQUIRE(CheckViews<Dxf::DContentScene2D>(false) == CheckViews<Dxf::DContentScene2D>(true));
}
TEST("Content 3D file views select distinct cameras and keep identical physics in odd-sized split display")
{
	REQUIRE(CheckViews<Dxf::DContentScene3D>(false) == CheckViews<Dxf::DContentScene3D>(true));
}
TEST("Content draw failure reaches rendering and suppresses Present before normal shutdown")
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::Testing::FContentRenderBackend Renderer(Backend);
	Dxf::FApplicationSettings Settings;
	Settings.ProjectRoot = DXF_CONTENT_TEST_ROOT;
	Dxf::FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer}, Settings);
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	const auto Prepared = Dxf::PrepareScene(Source.LoadScene2D("scene2d-a.dxfscene.json"), App.GetAssets());
	REQUIRE(App.Start(Toolbox::MakeUnique<Dxf::DContentScene2D>(Prepared)));
	Backend.GetTrace().bFailDraw = true;
	const auto Failed = App.Step(0);
	REQUIRE(!Failed && Failed.Error().Code == Dxf::EErrorCode::BackendFailure);
	REQUIRE(!Renderer.Triangles.IsEmpty());
	REQUIRE(Backend.GetTrace().Presentations == 0);
	REQUIRE(App.GetScenes().GetCurrent() == nullptr);
}
namespace
{
// 動的生成でも同じCollectionと固定更新を使い、旧handleから再生成へ追従しない。
template <typename TScene>
void CheckDynamicContent()
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::Testing::FContentRenderBackend Renderer(Backend);
	Dxf::FApplicationSettings Settings;
	Settings.ProjectRoot = DXF_CONTENT_TEST_ROOT;
	Settings.ExecutionThreadCount = 1;
	Dxf::FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer}, Settings);
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	const auto Prepared = [&]()
	{
		if constexpr (Toolbox::IsSame<TScene, Dxf::DContentScene2D>)
		{
			return Dxf::PrepareScene(Source.LoadScene2D("scene2d-a.dxfscene.json"), App.GetAssets());
		}
		else
		{
			return Dxf::PrepareScene(Source.LoadScene3D("scene3d-a.dxfscene.json"), App.GetAssets());
		}
	}();
	auto Scene = Toolbox::MakeUnique<TScene>(Prepared);
	auto* Live = Scene.Get();
	REQUIRE(App.Start(Toolbox::Move(Scene)));
	auto First = App.Step(0);
	REQUIRE(First && First.Value());
	const auto Added = Live->SpawnPrefab("extra", Prepared.Prefabs[0]);
	REQUIRE(Added);
	REQUIRE(Added.Value().Get()->GetState() == Dxf::EPrefabInstanceState::PendingInitialization);
	REQUIRE(!Live->SpawnPrefab("extra", Prepared.Prefabs[0]));
	REQUIRE(!Live->SpawnPrefab("bad/id", Prepared.Prefabs[0]));
	First = App.Step(1.25 / 60.0);
	REQUIRE(First && First.Value());
	First = App.Step(2.25 / 60.0);
	REQUIRE(First && First.Value());
	const auto Old = Added.Value();
	REQUIRE(Old.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	const auto Body = Old.Get()->GetRigidBody(Toolbox::IsSame<TScene, Dxf::DContentScene2D> ? "doorBody" : "body");
	const auto BodyId = Body.Get()->GetBodyId();
	const auto Neighbor = Live->GetPrefab("doorB");
	Old.Get()->Destroy();
	REQUIRE(!Old && !Body);
	First = App.Step(3.25 / 60.0);
	REQUIRE(First && First.Value());
	REQUIRE(!Live->GetPhysicsWorld().IsAlive(BodyId));
	REQUIRE(Neighbor && Neighbor.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	const auto New = Live->SpawnPrefab("extra", Prepared.Prefabs[0]);
	REQUIRE(New && New.Value() != Old);
	First = App.Step(4.25 / 60.0);
	REQUIRE(First && First.Value());
	First = App.Step(5.25 / 60.0);
	REQUIRE(First && First.Value());
	REQUIRE(New.Value().Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(Live->GetPrefab("extra") == New.Value());
	REQUIRE(!Old && !Body);
}
template <typename TScene>
void CheckSensorContent()
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::Testing::FContentRenderBackend Renderer(Backend);
	Dxf::FApplicationSettings Settings;
	Settings.ProjectRoot = DXF_CONTENT_TEST_ROOT;
	Settings.ExecutionThreadCount = 1;
	Dxf::FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer}, Settings);
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	const auto Prepared = [&]()
	{
		if constexpr (Toolbox::IsSame<TScene, Dxf::DContentScene2D>)
		{
			return Dxf::PrepareScene(Source.LoadScene2D("sensor2d.dxfscene.json"), App.GetAssets());
		}
		else
		{
			return Dxf::PrepareScene(Source.LoadScene3D("sensor3d.dxfscene.json"), App.GetAssets());
		}
	}();
	auto Scene = Toolbox::MakeUnique<TScene>(Prepared);
	auto* Live = Scene.Get();
	REQUIRE(App.Start(Toolbox::Move(Scene)));
	auto Step = App.Step(0);
	REQUIRE(Step && Step.Value());
	const auto Prefab = Live->GetPrefab("doorA");
	const auto Listener = Prefab.Get()->GetContactListener("sensor");
	Toolbox::uint32 Triggers = 0;
	// ゲーム判断は型付き接続先のC++関数。JSON読解とPhysicsへ規則を入れない。
	Listener.Get()->SetHandler(
	    [&Triggers](const auto& Notice)
	    {
		    if (Notice.Kind == Dxf::EWorldEventKind::Trigger && Notice.Phase == Dxf::EWorldEventPhase::Begin)
		    {
			    ++Triggers;
		    }
	    });
	REQUIRE(Triggers == 0);
	Step = App.Step(1.25 / 60.0);
	REQUIRE(Step && Step.Value());
	REQUIRE(Triggers == 1);
	REQUIRE(Listener.Get()->GetDeliveredCount() == 1);
	REQUIRE(Prefab.Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	Listener.Get()->SetHandler({});
}
} // namespace
TEST("Content 2D dynamic placement destruction and same-name regeneration preserve neighboring instances")
{
	CheckDynamicContent<Dxf::DContentScene2D>();
}
TEST("Content 3D dynamic placement destruction and same-name regeneration preserve neighboring instances")
{
	CheckDynamicContent<Dxf::DContentScene3D>();
}
TEST("Content 2D sensor export dispatches the real World successful Step to a C++ handler")
{
	CheckSensorContent<Dxf::DContentScene2D>();
}
TEST("Content 3D sensor export dispatches the real World successful Step to a C++ handler")
{
	CheckSensorContent<Dxf::DContentScene3D>();
}

namespace
{
// 共通資源と別Instanceの公開先を実Collection・実Worldへ接続する。
template <typename TScene>
void CheckSceneConnection()
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::Testing::FContentRenderBackend Renderer(Backend);
	Dxf::FApplicationSettings Settings;
	Settings.ProjectRoot = DXF_CONTENT_TEST_ROOT;
	Settings.ExecutionThreadCount = 1;
	Dxf::FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer}, Settings);
	REQUIRE(App.Start(Toolbox::MakeUnique<Dxf::DScene>()));
	const auto Empty = App.Step(0);
	REQUIRE(Empty && Empty.Value());
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	const auto Prepared = [&]()
	{
		if constexpr (Toolbox::IsSame<TScene, Dxf::DContentScene2D>)
		{
			return Dxf::PrepareScene(Source.LoadScene2D("scene2d-connected.dxfscene.json"), App.GetAssets());
		}
		else
		{
			return Dxf::PrepareScene(Source.LoadScene3D("scene3d-connected.dxfscene.json"), App.GetAssets());
		}
	}();
	REQUIRE(Prepared.Definition->Connections.Size() == 1);
	REQUIRE(Prepared.Definition->Assets.Size() == 1);
	REQUIRE(Backend.GetTrace().Fonts.Size() == 1);
	auto Scene = Toolbox::MakeUnique<TScene>(Prepared);
	auto* Live = Scene.Get();
	REQUIRE(App.GetScenes().RequestChange(Toolbox::Move(Scene)));
	const auto Accepted = App.Step(0);
	if (!Accepted)
	{
		throw Toolbox::FException(Accepted.Error().Message);
	}
	REQUIRE(Accepted.Value());
	const auto Joint = Live->GetFixedConnection("coupler");
	REQUIRE(!Joint.Get()->GetJointId());
	for (Toolbox::uint32 I = 1; I <= 5; ++I)
	{
		const auto Frame = App.Step((I + .25) / 60.0);
		if (!Frame)
		{
			throw Toolbox::FException(Frame.Error().Message);
		}
		REQUIRE(Frame.Value());
	}
	REQUIRE(Joint.Get()->GetJointId());
	REQUIRE(Joint.Get()->GetObservation());
	REQUIRE(Joint.Get()->GetObservation()->State.AnchorError < .001);
	bool WrongKind = false;
	try
	{
		(void)Live->GetRevoluteConnection("coupler");
	}
	catch (const Toolbox::FException&)
	{
		WrongKind = true;
	}
	REQUIRE(WrongKind);
	const auto A = Live->GetPrefab("doorA");
	const auto B = Live->GetPrefab("doorB");
	A.Get()->Destroy();
	const auto Removed = App.Step(.2);
	REQUIRE(Removed && Removed.Value());
	REQUIRE(!A && B);
	REQUIRE(!Joint.Get()->GetJointId());
	App.GetScenes().RequestQuit();
	const auto Quit = App.Step(.3);
	REQUIRE(Quit && !Quit.Value());
	REQUIRE(!B && !Joint);
}
} // namespace
TEST("Content 2D Scene common Font and cross-instance typed Fixed connection own only their Joint")
{
	CheckSceneConnection<Dxf::DContentScene2D>();
}
TEST("Content 3D Scene common Font and cross-instance typed Fixed connection own only their Joint")
{
	CheckSceneConnection<Dxf::DContentScene3D>();
}
TEST("Content Scene rejects missing typed endpoints self alias and non-Body exports before registration")
{
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	for (Toolbox::uint32 Dimension = 2; Dimension <= 3; ++Dimension)
	{
		for (const char* Suffix : {"wrong-connection", "self-connection"})
		{
			bool Failed = false;
			try
			{
				const auto Path =
				    Toolbox::FString("scene") + Toolbox::ToString(Dimension) + "d-" + Suffix + ".dxfscene.json";
				if (Dimension == 2)
				{
					(void)Source.LoadScene2D(Path);
				}
				else
				{
					(void)Source.LoadScene3D(Path);
				}
			}
			catch (const Dxf::FSceneContentError& Error)
			{
				Failed = true;
				REQUIRE(Error.GetDiagnostic().Line > 0);
			}
			REQUIRE(Failed);
		}
	}
}

namespace
{
// 予約が届いたかだけを外部の値へ記録する。内部の実体や資源を公開しない。
template <typename TPrefab, typename TPrepared>
class TObservedContentPrefab final : public TPrefab
{
public:
	TObservedContentPrefab(TPrepared Prepared, Toolbox::uint32& Calls)
	    : TPrefab(Toolbox::Move(Prepared)), m_Calls(Calls)
	{
	}

private:
	void OnPostPhysicsStep_Internal(const Dxf::FFixedTickContext&) override
	{
		++m_Calls;
	}
	Toolbox::uint32& m_Calls;
};
// RootがPostを予約した後、同じPre境界で破棄要求する。
class DDestroyReservedPrefab final : public Dxf::DGameObject, private Dxf::IPrePhysicsStep
{
public:
	explicit DDestroyReservedPrefab(Toolbox::TFunction<void()> Destroy) : m_Destroy(Toolbox::Move(Destroy))
	{
	}

protected:
	void OnFixedTick(const Dxf::FFixedTickContext& Context) override
	{
		Context.PrePhysicsStep->Enqueue(*this);
	}

private:
	void OnPrePhysicsStep_Internal(const Dxf::FFixedTickContext&) override
	{
		m_Destroy();
	}
	Toolbox::TFunction<void()> m_Destroy;
};
template <typename TScene, typename TPrefab>
void CheckReservedDestroy()
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets(Backend, Backend, Backend);
	Dxf::FSceneContentSource Source{Toolbox::FPath(DXF_CONTENT_TEST_ROOT)};
	const auto Prepared = [&]()
	{
		if constexpr (Toolbox::IsSame<TPrefab, Dxf::DPrefabInstance2D>)
		{
			return Dxf::PreparePrefab(Source.LoadPrefab2D("door2d.dxfprefab.json"), Assets);
		}
		else
		{
			return Dxf::PreparePrefab(Source.LoadPrefab3D("door3d.dxfprefab.json"), Assets);
		}
	}();
	Toolbox::uint32 PostCalls = 0;
	TScene Scene;
	const auto Root = Scene.template Spawn<TObservedContentPrefab<TPrefab, decltype(Prepared)>>(Prepared, PostCalls);
	REQUIRE(Root);
	REQUIRE(Scene.template Spawn<DDestroyReservedPrefab>(
	    [Handle = Root.Value()]()
	    {
		    Handle.Get()->Destroy();
	    }));
	REQUIRE(Scene.Initialize_Internal({Assets}));
	Dxf::FInputStateTracker Input;
	Dxf::FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	REQUIRE(!Root.Value());
	REQUIRE(PostCalls == 0);
	Scene.Shutdown_Internal();
}
} // namespace
TEST("Content destroyed Prefab suppresses its already reserved Post in 2D and 3D")
{
	CheckReservedDestroy<Dxf::DPhysicsScene2D, Dxf::DPrefabInstance2D>();
	CheckReservedDestroy<Dxf::DPhysicsScene3D, Dxf::DPrefabInstance3D>();
}
