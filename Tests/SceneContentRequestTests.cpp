// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Support/ContentRenderBackend.h"
#include "Dxf/SceneContentRequest.h"
#include "Dxf/Application.h"
#include "Dxf/ContentScene2D.h"
#include "Dxf/ContentScene3D.h"
#include "Toolbox/Thread.h"
namespace
{
// 同期と非同期で、準備失敗・取消し・再試行の採用規則を揃える。
void CheckRequests(Toolbox::uint32 Dimension, bool bAsync)
{
	Toolbox::FJobSystem Jobs(2);
	Dxf::FTaskDispatcher Tasks(Jobs);
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets{Backend, Backend, Backend};
	Dxf::FSceneContentRequest Request(Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT)), bAsync ? &Tasks : nullptr);
	const auto Load = [&Request, Dimension](const char* Suffix)
	{
		const auto Path = Toolbox::FString(Dimension == 2 ? "scene2d-" : "scene3d-") + Suffix + ".dxfscene.json";
		return Dimension == 2 ? Request.LoadScene2D(Path) : Request.LoadScene3D(Path);
	};
	const auto FinishCpu = [&Tasks, bAsync]()
	{
		if (bAsync)
		{
			Tasks.WaitForPrepares();
			Tasks.PumpCommits();
		}
	};
	const auto A = Load("a");
	FinishCpu();
	// CPUのCommitだけでは必須資源の準備を完了扱いにしない。
	REQUIRE(A.GetState() == Dxf::ESceneContentRequestState::Preparing);
	REQUIRE(Request.GetAcceptedSequence() == 0);
	Request.Poll(Assets);
	REQUIRE(A.GetState() == Dxf::ESceneContentRequestState::Ready);
	REQUIRE(Request.GetAcceptedSequence() == A.GetSequence());
	const auto Original =
	    Dimension == 2 ? Request.GetPrepared2D().Definition->Path : Request.GetPrepared3D().Definition->Path;
	const auto C = Load("missing");
	FinishCpu();
	Request.Poll(Assets);
	REQUIRE(C.GetState() == Dxf::ESceneContentRequestState::Failed);
	REQUIRE(!C.GetDiagnostic().Path.IsEmpty() && !C.GetDiagnostic().Reason.IsEmpty());
	REQUIRE(Request.GetAcceptedSequence() == A.GetSequence());
	REQUIRE((Dimension == 2 ? Request.GetPrepared2D().Definition->Path : Request.GetPrepared3D().Definition->Path) == Original);
	const auto Canceled = Load("b");
	FinishCpu();
	Request.Cancel();
	Request.Poll(Assets);
	REQUIRE(Canceled.GetState() == Dxf::ESceneContentRequestState::Canceled);
	REQUIRE(Request.GetAcceptedSequence() == A.GetSequence());
	const auto Old = Load("a");
	FinishCpu();
	const auto B = Load("b");
	FinishCpu();
	REQUIRE(Old.GetState() == Dxf::ESceneContentRequestState::Superseded);
	Request.Poll(Assets);
	REQUIRE(B.GetState() == Dxf::ESceneContentRequestState::Ready);
	REQUIRE(Request.GetAcceptedSequence() == B.GetSequence());
	REQUIRE((Dimension == 2 ? Request.GetPrepared2D().Definition->Placements[1].Position.X : Request.GetPrepared3D().Definition->Placements[1].Position.X) == 20);
	REQUIRE(Old.GetState() == Dxf::ESceneContentRequestState::Superseded);
	REQUIRE(Request.Retire());
}
} // namespace
TEST("Content 2D synchronous requests retain accepted data on failure and cancellation")
{
	CheckRequests(2, false);
}
TEST("Content 3D synchronous requests retain accepted data on failure and cancellation")
{
	CheckRequests(3, false);
}
TEST("Content 2D asynchronous requests only adopt the newest CPU result from owner Poll")
{
	CheckRequests(2, true);
}
TEST("Content 3D asynchronous requests only adopt the newest CPU result from owner Poll")
{
	CheckRequests(3, true);
}
TEST("Content request cancels a queued prepare and retires only its own Scope")
{
	// 1レーンは同期実行なので、呼出し側とWorkerの2レーンを使う。
	Toolbox::FJobSystem Jobs(2);
	Dxf::FTaskDispatcher Tasks(Jobs);
	Toolbox::FAtomicCounter Release;
	Toolbox::FAtomicCounter Entered;
	Toolbox::FAtomicCounter Commits;
	struct FRelease
	{
		Toolbox::FAtomicCounter& Flag;
		~FRelease()
		{
			Flag.Store(1);
		}
	} Guard{Release};
	REQUIRE(Tasks.Submit({[&Release, &Entered]()
	                      {
		                      Entered.Store(1);
		                      while (Release.Load() == 0)
		                      {
			                      Toolbox::FThread::Yield();
		                      }
		                      return Dxf::ETaskPrepare::Success;
	                      },
	                      [&Commits]()
	                      {
		                      Commits.FetchAdd(1);
		                      return true;
	                      }}));
	while (Entered.Load() == 0)
	{
		Toolbox::FThread::Yield();
	}
	Dxf::FSceneContentRequest Request(Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT)), &Tasks);
	const auto A = Request.LoadScene2D("scene2d-a.dxfscene.json");
	REQUIRE(A.GetState() == Dxf::ESceneContentRequestState::Pending);
	const auto B = Request.LoadScene3D("scene3d-b.dxfscene.json");
	REQUIRE(A.GetState() == Dxf::ESceneContentRequestState::Superseded);
	REQUIRE(Request.Retire());
	REQUIRE(B.GetState() == Dxf::ESceneContentRequestState::Canceled);
	REQUIRE(Release.Load() == 0 && Commits.Load() == 0);
	Release.Store(1);
	Tasks.WaitForPrepares();
	Tasks.PumpCommits();
	REQUIRE(Commits.Load() == 1);
}
TEST("Content request observes parent cancellation and Task job submission rejection")
{
	Toolbox::FJobSystem Jobs(1);
	Dxf::FTaskDispatcher Tasks(Jobs);
	const auto Parent = Tasks.CreateScope();
	Dxf::Testing::FFakeBackend Backend;
	Dxf::FAssetService Assets{Backend, Backend, Backend};
	Dxf::FSceneContentRequest Request(Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT)), &Tasks, Parent);
	const auto A = Request.LoadScene2D("scene2d-a.dxfscene.json");
	Tasks.WaitForPrepares();
	Tasks.Cancel(Parent);
	Tasks.PumpCommits();
	Request.Poll(Assets);
	REQUIRE(A.GetState() == Dxf::ESceneContentRequestState::Canceled);
	REQUIRE(Request.GetAcceptedSequence() == 0);
	REQUIRE(Request.Retire());
	Jobs.Shutdown();
	Dxf::FSceneContentRequest Rejected(Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT)), &Tasks);
	const auto B = Rejected.LoadScene3D("scene3d-b.dxfscene.json");
	REQUIRE(B.GetState() == Dxf::ESceneContentRequestState::Failed);
	REQUIRE(!B.GetDiagnostic().Reason.IsEmpty());
	Rejected.Poll(Assets);
	REQUIRE(Rejected.GetAcceptedSequence() == 0);
}
namespace
{
// 現在のSceneが準備失敗後も更新・描画を続けたことを実Applicationで観察する。
class DContentWaitingScene final : public Dxf::DGameScene
{
public:
	explicit DContentWaitingScene(Toolbox::uint32& Ticks) : m_pTicks(&Ticks)
	{
	}

protected:
	void OnTick(const Dxf::FTickContext&) override
	{
		++*m_pTicks;
	}

private:
	Toolbox::uint32* m_pTicks;
};
template <typename TScene>
class TContentFailingScene final : public TScene
{
public:
	template <typename TPrepared>
	explicit TContentFailingScene(TPrepared Prepared) : TScene(Toolbox::Move(Prepared))
	{
	}

protected:
	Dxf::TResult<void> OnInitialize(const Dxf::FInitContext& Context) override
	{
		const auto Initialized = TScene::OnInitialize(Context);
		if (!Initialized)
		{
			return Initialized;
		}
		return Dxf::TResult<void>::Failure(Dxf::EErrorCode::InitializationFailed, "Injected content Scene initialization failure");
	}
};
template <typename TScene>
void CheckApplicationContent()
{
	Dxf::Testing::FFakeBackend Backend;
	Dxf::Testing::FContentRenderBackend Renderer(Backend);
	Dxf::FApplicationSettings Settings;
	Settings.ExecutionThreadCount = 2;
	Settings.ProjectRoot = DXF_CONTENT_TEST_ROOT;
	Dxf::FApplication App({Backend, Backend, Backend, Backend, Backend, Renderer}, Settings);
	Toolbox::uint32 OldTicks = 0;
	REQUIRE(App.Start(Toolbox::MakeUnique<DContentWaitingScene>(OldTicks)));
	auto* OldScene = App.GetScenes().GetCurrent();
	Dxf::FSceneContentRequest Request(Dxf::FSceneContentSource(Toolbox::FPath(DXF_CONTENT_TEST_ROOT)), &App.GetTaskDispatcher());
	const auto Load = [&Request](const char* Path)
	{
		if constexpr (Toolbox::IsSame<TScene, Dxf::DContentScene2D>)
		{
			return Request.LoadScene2D(Path);
		}
		else
		{
			return Request.LoadScene3D(Path);
		}
	};
	const auto Step = [&App](Toolbox::f64 Time)
	{
		const auto R = App.Step(Time);
		REQUIRE(R && R.Value());
	};
	const auto Missing = Load("missing.dxfscene.json");
	App.GetTaskDispatcher().WaitForPrepares();
	Step(0);
	Request.Poll(App.GetAssets());
	REQUIRE(Missing.GetState() == Dxf::ESceneContentRequestState::Failed);
	REQUIRE(App.GetScenes().GetCurrent() == OldScene);
	REQUIRE(OldTicks == 1);
	const auto AssetsFailed =
	    Load(Toolbox::IsSame<TScene, Dxf::DContentScene2D> ? "resources2d.dxfscene.json" : "resources3d.dxfscene.json");
	App.GetTaskDispatcher().WaitForPrepares();
	Step(1.0 / 60.0);
	REQUIRE(Backend.GetTrace().TextureLoads == 0 && Backend.GetTrace().SoundLoads == 0);
	Backend.GetTrace().bFailSound = true;
	Request.Poll(App.GetAssets());
	REQUIRE(AssetsFailed.GetState() == Dxf::ESceneContentRequestState::Failed);
	REQUIRE(AssetsFailed.GetDiagnostic().Key == "cue");
	REQUIRE(App.GetScenes().GetCurrent() == OldScene);
	REQUIRE(Backend.GetTrace().Textures.Size() == 0);
	Backend.GetTrace().bFailSound = false;
	const auto Ready =
	    Load(Toolbox::IsSame<TScene, Dxf::DContentScene2D> ? "resources2d.dxfscene.json" : "resources3d.dxfscene.json");
	App.GetTaskDispatcher().WaitForPrepares();
	Step(2.0 / 60.0);
	Request.Poll(App.GetAssets());
	REQUIRE(Ready.GetState() == Dxf::ESceneContentRequestState::Ready);
	REQUIRE(App.GetScenes().GetCurrent() == OldScene);
	const auto Prepared = [&Request]()
	{
		if constexpr (Toolbox::IsSame<TScene, Dxf::DContentScene2D>)
		{
			return Request.GetPrepared2D();
		}
		else
		{
			return Request.GetPrepared3D();
		}
	}();
	REQUIRE(Prepared.Prefabs[0].Resources->GetTexture(0).GetNativeHandle_Internal() == Prepared.Prefabs[1].Resources->GetTexture(0).GetNativeHandle_Internal());
	REQUIRE(App.GetScenes().template RequestChange<TContentFailingScene<TScene>>(Prepared));
	Step(3.0 / 60.0);
	REQUIRE(App.GetScenes().GetCurrent() == OldScene);
	REQUIRE(App.GetScenes().GetLastTransitionError());
	REQUIRE(OldTicks == 4);
	REQUIRE(App.GetScenes().template RequestChange<TScene>(Prepared));
	Step(4.0 / 60.0);
	auto* Scene = static_cast<TScene*>(App.GetScenes().GetCurrent());
	REQUIRE(Scene != OldScene && Scene->GetObjectCount() == 2);
	Step(5.0 / 60.0);
	REQUIRE(Scene->GetPrefab("doorA").Get()->GetState() == Dxf::EPrefabInstanceState::Ready);
	REQUIRE(Scene->GetPrefab("doorA").Get()->GetTexture("picture").IsValid());
	REQUIRE(Scene->GetPrefab("doorA").Get()->GetFont("label").IsValid());
	REQUIRE(Scene->GetPrefab("doorA").Get()->GetSound("openCue").IsValid());
	bool WrongKind = false;
	try
	{
		Scene->GetPrefab("doorA").Get()->GetModel("picture");
	}
	catch (const Toolbox::FException&)
	{
		WrongKind = true;
	}
	REQUIRE(WrongKind);
	const auto Body = Scene->GetPrefab("doorA").Get()->GetRigidBody( Toolbox::IsSame<TScene, Dxf::DContentScene2D> ? "doorBody" : "body");
	REQUIRE(Body);
	App.GetScenes().RequestQuit();
	const auto Stopped = App.Step(6.0 / 60.0);
	REQUIRE(Stopped && !Stopped.Value());
	REQUIRE(!Body);
	REQUIRE(Request.Retire());
	// App終了ではAssetServiceの資源も失効する。外部の準備値が寿命を延ばすとは扱わない。
}
} // namespace
TEST("Content 2D real Application preserves old Scene during parse assets and pending initialization failures")
{
	CheckApplicationContent<Dxf::DContentScene2D>();
}
TEST("Content 3D real Application preserves old Scene during parse assets and pending initialization failures")
{
	CheckApplicationContent<Dxf::DContentScene3D>();
}
