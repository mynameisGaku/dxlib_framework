#include "Dxf/SceneNavigator.h"
#include "Dxf/AssetService.h"
#include "Dxf/GuardValue.h"
namespace Dxf
{
FSceneNavigator::FSceneNavigator(FAssetService& Assets, FAudioPlayer& Audio, DGameInstance* Game)
	: m_pAssets(&Assets), m_pAudio(&Audio), m_pGame(Game), m_Lifecycle(Audio)
{
}
FSceneNavigator::~FSceneNavigator()
{
	Shutdown();
}
TResult<void> FSceneNavigator::RequestChange(std::unique_ptr<DScene> Scene)
{
	if (m_bShutdown || m_bShutdownRequested)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Navigator stopped");
	}
	if (!Scene || Scene->GetState() != ELifecycleState::Pending)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Expected a new scene");
	}
	m_Storage.SetPending_Internal(std::move(Scene));
	return {};
}
TResult<bool> FSceneNavigator::Commit_Internal()
{
	auto Next = m_Storage.TakePending_Internal();
	if (!Next)
	{
		return TResult<bool>::Success(false);
	}
	auto Prepared = m_Lifecycle.Prepare_Internal(*Next, {*m_pAssets});
	if (!Prepared)
	{
		m_LastTransitionError = Prepared.Error();
		return TResult<bool>::Failure(Prepared.Error());
	}
	if (Next->IsDestroyRequested())
	{
		Next->Shutdown_Internal();
		m_LastTransitionError = FError{EErrorCode::InvalidState, "Scene destroyed during preparation"};
		return TResult<bool>::Failure(*m_LastTransitionError);
	}
	if (WantsQuit() || m_bShutdownRequested)
	{
		// A prepared scene has not entered an audio scope; do not stop global scope 0.
		Next->Shutdown_Internal();
		return TResult<bool>::Success(false);
	}
	if (auto* Current = m_Storage.GetCurrent())
	{
		m_Lifecycle.Stop_Internal(*Current);
	}
	if (WantsQuit() || m_bShutdownRequested)
	{
		Next->Shutdown_Internal();
		return TResult<bool>::Success(false);
	}
	m_Storage.SetCurrent_Internal(std::move(Next));
	m_LastTransitionError.reset();
	m_Lifecycle.Activate_Internal(*m_Storage.GetCurrent(), {*m_pAudio, *this, m_pGame, AllocateDomain_Internal()});
	return TResult<bool>::Success(true);
}
TResult<bool> FSceneNavigator::Commit()
{
	if (m_bBusy || m_bShutdown)
	{
		return TResult<bool>::Failure(EErrorCode::InvalidState, "Invalid or reentrant scene commit");
	}
	auto Result = TResult<bool>::Success(false);
	{
		TGuardValue Guard(m_bBusy, true);
		Result = Commit_Internal();
	}
	FinishDispatch_Internal();
	return Result;
}
TResult<void> FSceneNavigator::CommitObjects()
{
	if (m_bBusy || m_bShutdown)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid scene boundary");
	}
	TResult<void> Result;
	{
		TGuardValue Guard(m_bBusy, true);
		if (auto* Scene = GetCurrent())
		{
			if (auto* Children = Scene->GetChildren_Internal())
			{
				Children->FreezeBoundary_Internal();
				Result = Children->CommitBoundary_Internal({*m_pAssets});
			}
		}
	}
	FinishDispatch_Internal();
	return Result;
}
TResult<void> FSceneNavigator::Tick(FFrameTime Time, const FInputSnapshot& Input)
{
	if (m_bBusy || m_bShutdown)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or reentrant scene tick");
	}
	TResult<void> Result;
	{
		TGuardValue Guard(m_bBusy, true);
		if (auto* Scene = GetCurrent())
		{
			Time = Scene->GetClock().Advance(Time);
			Result = Scene->Tick_Internal({Input, Time, this, m_pGame, m_pAudio, Scene->GetAudioScope()});
		}
	}
	FinishDispatch_Internal();
	return Result;
}
TResult<void> FSceneNavigator::Draw(FRenderContext& Render)
{
	if (m_bBusy || m_bShutdown)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or reentrant scene draw");
	}
	TResult<void> Result;
	{
		TGuardValue Guard(m_bBusy, true);
		if (auto* Scene = GetCurrent())
		{
			Result = Scene->Draw_Internal(Render);
		}
	}
	FinishDispatch_Internal();
	return Result;
}
void FSceneNavigator::FinishDispatch_Internal() noexcept
{
	if (m_bShutdownRequested)
	{
		Shutdown();
	}
}
void FSceneNavigator::Shutdown() noexcept
{
	if (m_bBusy)
	{
		m_bShutdownRequested = true;
		m_bQuit = true;
		return;
	}
	if (m_bShutdown)
	{
		return;
	}
	m_bShutdown = true;
	m_bQuit = true;
	TGuardValue Guard(m_bBusy, true);
	if (auto* Current = GetCurrent())
	{
		m_Lifecycle.Stop_Internal(*Current);
	}
	m_Storage.Clear_Internal();
}
}
