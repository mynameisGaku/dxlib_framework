// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/AssetService.h"
using namespace Dxf;
using namespace Toolbox;
namespace
{
// 失敗位置と呼出し順だけを保持し、Physicsを模倣しない。
struct FCompletionCounts
{
	uint32 Fixed = 0;
	uint32 Pre = 0;
	uint32 Post = 0;
	uint32 Completed = 0;
	uint32 Failure = 0;
};
template <typename TScene>
class TCompletionProbe final : public DGameObject, private IPrePhysicsStep, private IPostPhysicsStep
{
public:
	TCompletionProbe(FCompletionCounts& Counts, TScene& Scene) : m_pCounts(&Counts), m_pScene(&Scene)
	{
	}

protected:
	void OnFixedTick(const FFixedTickContext& C) override
	{
		REQUIRE(!m_pScene->IsAtPhysicsCompletionBoundary());
		++m_pCounts->Fixed;
		if (m_pCounts->Failure == 1)
		{
			throw FException("Test Fixed failure");
		}
		C.PrePhysicsStep->Enqueue(*this);
		C.PostPhysicsStep->Enqueue(*this);
	}

private:
	void OnPrePhysicsStep_Internal(const FFixedTickContext&) override
	{
		REQUIRE(!m_pScene->IsAtPhysicsCompletionBoundary());
		++m_pCounts->Pre;
		if (m_pCounts->Failure == 2)
		{
			throw FException("Test Pre failure");
		}
	}
	void OnPostPhysicsStep_Internal(const FFixedTickContext&) override
	{
		REQUIRE(!m_pScene->IsAtPhysicsCompletionBoundary());
		++m_pCounts->Post;
		if (m_pCounts->Failure == 3)
		{
			throw FException("Test Post failure");
		}
		if (m_pCounts->Failure == 5)
		{
			m_pScene->RequestDestroy_Internal();
		}
	}
	bool IsPostPhysicsStepAlive_Internal() const noexcept override
	{
		return !IsDestroyRequested();
	}
	FCompletionCounts* m_pCounts;
	TScene* m_pScene;
};
template <typename TBase>
class TCompletionScene final : public TBase
{
public:
	explicit TCompletionScene(FCompletionCounts& Counts) : m_pCounts(&Counts)
	{
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		for (uint32 I = 0; I < 2; ++I)
		{
			const auto Spawned = this->template Spawn<TCompletionProbe<TCompletionScene>>(*m_pCounts, *this);
			if (!Spawned)
			{
				return TResult<void>::Failure(Spawned.Error());
			}
		}
		return {};
	}
	void OnPhysicsStepCompleted(const FFixedTickContext&) override
	{
		REQUIRE(this->IsAtPhysicsCompletionBoundary());
		REQUIRE(m_pCounts->Fixed == m_pCounts->Pre && m_pCounts->Pre == m_pCounts->Post);
		++m_pCounts->Completed;
		REQUIRE(this->GetCompletedPhysicsStepCount() == m_pCounts->Completed);
		if (m_pCounts->Failure == 4)
		{
			throw FException("Test completion failure");
		}
	}

private:
	FCompletionCounts* m_pCounts;
};
template <typename TBase>
void Boundary()
{
	Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FInputStateTracker Input;
	FCompletionCounts Counts;
	TCompletionScene<TBase> Scene(Counts);
	struct FShutdown
	{
		TCompletionScene<TBase>& Scene;
		~FShutdown()
		{
			Scene.Shutdown_Internal();
		}
	} Guard{Scene};
	REQUIRE(Scene.Initialize_Internal({Assets}));
	FFrameTime Time;
	REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	REQUIRE(Counts.Completed == 0 && Scene.GetCompletedPhysicsStepCount() == 0);
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	REQUIRE(Counts.Completed == 1 && Counts.Post == 2);
	REQUIRE(!Scene.IsAtPhysicsCompletionBoundary());
	Time.DeltaSeconds = 3.0 / 60.0;
	REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	REQUIRE(Counts.Completed == 4 && Counts.Post == 8);
	Scene.SetTickWhenPaused(true);
	Time.bPaused = true;
	REQUIRE(Scene.Tick_Internal({Input.GetSnapshot(), Time}));
	REQUIRE(Counts.Completed == 4 && !Scene.IsAtPhysicsCompletionBoundary());
}
template <typename TBase>
void Failures()
{
	Testing::FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FInputStateTracker Input;
	FFrameTime Time;
	Time.DeltaSeconds = 1.0 / 60.0;
	for (uint32 Failure = 1; Failure <= 5; ++Failure)
	{
		FCompletionCounts Counts;
		Counts.Failure = Failure;
		TCompletionScene<TBase> Scene(Counts);
		struct FShutdown
		{
			TCompletionScene<TBase>& Scene;
			~FShutdown()
			{
				Scene.Shutdown_Internal();
			}
		} Guard{Scene};
		REQUIRE(Scene.Initialize_Internal({Assets}));
		const auto Result = Scene.Tick_Internal({Input.GetSnapshot(), Time});
		REQUIRE(!Scene.IsAtPhysicsCompletionBoundary());
		REQUIRE(Counts.Completed == (Failure == 4 ? 1U : 0U));
		REQUIRE(Scene.GetCompletedPhysicsStepCount() == (Failure >= 4 ? 1U : 0U));
		REQUIRE(static_cast<bool>(Result) == (Failure == 5));
	}
}
} // namespace
TEST("Physics 2D completion boundary follows all Post callbacks and ignores zero tick and pause")
{
	Boundary<DPhysicsScene2D>();
}
TEST("Physics 3D completion boundary follows all Post callbacks and ignores zero tick and pause")
{
	Boundary<DPhysicsScene3D>();
}
TEST("Physics 2D completion suppresses failed and destroyed pipelines and clears its marker on exceptions")
{
	Failures<DPhysicsScene2D>();
}
TEST("Physics 3D completion suppresses failed and destroyed pipelines and clears its marker on exceptions")
{
	Failures<DPhysicsScene3D>();
}
