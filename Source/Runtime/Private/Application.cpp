#include "Dxf/Application.h"
#include "Dxf/GuardValue.h"
#include <exception>
namespace Dxf
{
FApplication::FApplication(FBackendServices Services, FApplicationSettings Settings, std::unique_ptr<DGameInstance> Game)
	: m_pPlatform(&Services.Platform), m_Settings(std::move(Settings)), m_Session(Services.Platform),
m_Input(Services.Input), m_Assets(Services.Textures, Services.Sounds, Services.Fonts),
m_Renderer(Services.Renderer), m_Audio(Services.Sounds), m_pGame(std::move(Game)),
m_Scenes(m_Assets, m_Audio, m_pGame.get()), m_Clock(m_Settings.MaxDeltaSeconds)
{
}
FApplication::~FApplication()
{
	Shutdown();
}
TResult<void> FApplication::Start_Internal(std::unique_ptr<DScene> InitialScene)
{
	if (!InitialScene)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "An initial scene is required");
	}
	auto SessionResult = m_Session.Initialize(m_Settings.Window);
	if (!SessionResult)
	{
		return SessionResult;
	}
	if (m_pGame)
	{
		auto GameResult = m_pGame->Initialize_Internal({m_Assets});
		if (!GameResult)
		{
			return GameResult;
		}
	}
	if (m_bShutdownRequested)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Shutdown requested during startup");
	}
	auto Request = m_Scenes.RequestChange(std::move(InitialScene));
	if (!Request)
	{
		return Request;
	}
	auto Commit = m_Scenes.Commit();
	if (!Commit)
	{
		return TResult<void>::Failure(Commit.Error());
	}
	m_bStarted = true;
	return {};
}
TResult<void> FApplication::Start(std::unique_ptr<DScene> InitialScene)
{
	if (m_bAttemptedStart || m_bShutdown || m_bBusy)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Application is single-use or busy");
	}
	m_bAttemptedStart = true;
	TResult<void> Result;
	try
	{
		TGuardValue Guard(m_bBusy, true);
		Result = Start_Internal(std::move(InitialScene));
	}
	catch (const std::exception& Exception)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, Exception.what());
	}
	catch (...)
	{
		Result = TResult<void>::Failure(EErrorCode::UserException, "Unknown startup exception");
	}
	if (!Result || WantsQuit_Internal())
	{
		Shutdown();
	}
	return Result;
}
bool FApplication::WantsQuit_Internal() const noexcept
{
	return m_bShutdownRequested || m_Scenes.WantsQuit();
}
TResult<bool> FApplication::Step_Internal(double NowSeconds)
{
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	auto Events = m_pPlatform->PumpEvents();
	if (!Events)
	{
		return Events;
	}
	if (!Events.Value())
	{
		return TResult<bool>::Success(false);
	}
	auto Time = m_Clock.Sample(NowSeconds);
	if (!Time)
	{
		return TResult<bool>::Failure(Time.Error());
	}
	auto Input = m_Input.Update();
	if (!Input)
	{
		return TResult<bool>::Failure(Input.Error());
	}
	auto Transition = m_Scenes.Commit();
	// A failed replacement is recoverable while the previous scene remains active.
	if (!Transition && !m_Scenes.GetCurrent())
	{
		return TResult<bool>::Failure(Transition.Error());
	}
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	auto Objects = m_Scenes.CommitObjects();
	if (!Objects)
	{
		return TResult<bool>::Failure(Objects.Error());
	}
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	if (m_pGame)
	{
		auto GameTick = m_pGame->Tick_Internal({m_Input.GetSnapshot(), Time.Value(), &m_Scenes, m_pGame.get(), &m_Audio, 0});
		if (!GameTick)
		{
			return TResult<bool>::Failure(GameTick.Error());
		}
	}
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	auto Tick = m_Scenes.Tick(Time.Value(), m_Input.GetSnapshot());
	if (!Tick)
	{
		return TResult<bool>::Failure(Tick.Error());
	}
	if (WantsQuit_Internal())
	{
		return TResult<bool>::Success(false);
	}
	auto Audio = m_Audio.Tick();
	if (!Audio)
	{
		return TResult<bool>::Failure(Audio.Error());
	}
	auto Begin = m_Renderer.BeginFrame(m_Settings.Window.Width, m_Settings.Window.Height, m_Settings.ClearColor);
	if (!Begin)
	{
		return TResult<bool>::Failure(Begin.Error());
	}
	auto Draw = m_Scenes.Draw(m_Renderer.GetContext());
	if (!Draw)
	{
		return TResult<bool>::Failure(Draw.Error());
	}
	if (WantsQuit_Internal())
	{
		m_Renderer.CancelFrame();
		return TResult<bool>::Success(false);
	}
	auto Present = m_Renderer.EndFrame();
	if (!Present)
	{
		return TResult<bool>::Failure(Present.Error());
	}
	m_Assets.CollectUnused();
	return TResult<bool>::Success(!WantsQuit_Internal());
}
TResult<bool> FApplication::Step(double NowSeconds)
{
	if (m_bBusy || !IsRunning())
	{
		return TResult<bool>::Failure(EErrorCode::InvalidState, "Application is stopped or Step is reentrant");
	}
	auto Result = TResult<bool>::Success(false);
	try
	{
		TGuardValue Guard(m_bBusy, true);
		Result = Step_Internal(NowSeconds);
	}
	catch (const std::exception& Exception)
	{
		Result = TResult<bool>::Failure(EErrorCode::UserException, Exception.what());
	}
	catch (...)
	{
		Result = TResult<bool>::Failure(EErrorCode::UserException, "Unknown frame exception");
	}
	if (!Result || !Result.Value() || WantsQuit_Internal())
	{
		Shutdown();
		if (Result)
		{
			Result = TResult<bool>::Success(false);
		}
	}
	return Result;
}
void FApplication::Shutdown() noexcept
{
	if (m_bBusy)
	{
		m_bShutdownRequested = true;
		m_Scenes.RequestQuit();
		if (m_pGame)
		{
			m_pGame->RequestDestroy_Internal();
		}
		return;
	}
	if (m_bShutdown)
	{
		return;
	}
	m_bShutdown = true;
	TGuardValue Guard(m_bBusy, true);
	m_Scenes.Shutdown();
	if (m_pGame)
	{
		m_pGame->Shutdown_Internal();
		m_pGame.reset();
	}
	m_Audio.Shutdown();
	m_Renderer.CancelFrame();
	m_Assets.Shutdown();
	m_Session.Shutdown();
}
}
