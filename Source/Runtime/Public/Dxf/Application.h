#pragma once
#include "Dxf/BackendServices.h"
#include "Dxf/DxLibSession.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/RenderSystem2D.h"
#include "Dxf/GameInstance.h"
#include "Dxf/SceneNavigator.h"
namespace Dxf
{
struct FApplicationSettings
{
	FWindowSettings Window;
	FColor ClearColor{0, 0, 0, 255};
	double MaxDeltaSeconds = 0.25;
};
/** Single-use, main-thread composition root; Shutdown is deferred during callbacks. */
class FApplication
{
public:
	explicit FApplication(FBackendServices Services, FApplicationSettings Settings = {},
		std::unique_ptr<DGameInstance> Game = std::make_unique<DGameInstance>());
	~FApplication();
	FApplication(const FApplication&) = delete;
	FApplication& operator=(const FApplication&) = delete;
	TResult<void> Start(std::unique_ptr<DScene> InitialScene);
	/** False is a graceful exit; an error is fatal and triggers ordered shutdown. */
	TResult<bool> Step(double NowSeconds);
	void Shutdown() noexcept;
	bool IsRunning() const noexcept
	{
		return m_bStarted && !m_bShutdown && !m_bShutdownRequested;
	}
	FSceneNavigator& GetScenes() noexcept
	{
		return m_Scenes;
	}
	FAssetService& GetAssets() noexcept
	{
		return m_Assets;
	}
	FRenderSystem2D& GetRenderer() noexcept
	{
		return m_Renderer;
	}
	DGameInstance* GetGameInstance() noexcept
	{
		return m_pGame.get();
	}
private:
	TResult<void> Start_Internal(std::unique_ptr<DScene> InitialScene);
	TResult<bool> Step_Internal(double NowSeconds);
	bool WantsQuit_Internal() const noexcept;
	IPlatform* m_pPlatform;
	FApplicationSettings m_Settings;
	FDxLibSession m_Session;
	FInputSystem m_Input;
	FAssetService m_Assets;
	FRenderSystem2D m_Renderer;
	FAudioPlayer m_Audio;
	std::unique_ptr<DGameInstance> m_pGame;
	FSceneNavigator m_Scenes;
	FFrameClock m_Clock;
	bool m_bAttemptedStart = false;
	bool m_bStarted = false;
	bool m_bBusy = false;
	bool m_bShutdown = false;
	bool m_bShutdownRequested = false;
};
}
