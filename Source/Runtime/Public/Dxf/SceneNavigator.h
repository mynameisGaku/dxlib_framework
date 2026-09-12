#pragma once
#include "Dxf/SceneStorage.h"
#include "Dxf/SceneLifecycle.h"
#include <optional>
#include <exception>
#include <type_traits>
#include <utility>
namespace Dxf
{
class FAssetService;
class FSceneNavigator
{
public:
	FSceneNavigator(FAssetService& Assets, FAudioPlayer& Audio, DGameInstance* Game = nullptr);
	~FSceneNavigator();
	FSceneNavigator(const FSceneNavigator&) = delete;
	FSceneNavigator& operator=(const FSceneNavigator&) = delete;
	TResult<void> RequestChange(std::unique_ptr<DScene> Scene);
	template <typename T, typename... TArgs> TResult<void> RequestChange(TArgs&&... Args)
	{
		static_assert(std::is_base_of_v<DScene, T>);
		if (!CanAcceptRequest_Internal())
		{
			return TResult<void>::Failure(EErrorCode::InvalidState, "Navigator cannot accept a scene request");
		}
		try
		{
			return RequestChange(std::make_unique<T>(std::forward<TArgs>(Args)...));
		}
		catch (const std::exception& Error)
		{
			return TResult<void>::Failure(EErrorCode::UserException, Error.what());
		}
		catch (...)
		{
			return TResult<void>::Failure(EErrorCode::UserException, "Unknown scene constructor exception");
		}
	}
	TResult<bool> Commit();
	TResult<void> CommitObjects();
	TResult<void> Tick(FFrameTime Time, const FInputSnapshot& Input);
	TResult<void> Draw(FRenderContext& Render);
	DScene* GetCurrent() const noexcept
	{
		return m_Storage.GetCurrent();
	}
	const std::optional<FError>& GetLastTransitionError() const noexcept
	{
		return m_LastTransitionError;
	}
	void RequestQuit() noexcept;
	bool WantsQuit() const noexcept
	{
		return m_bQuit || m_bShutdown;
	}
	void Shutdown() noexcept;
private:
	bool CanAcceptRequest_Internal() const noexcept
	{
		return !WantsQuit() && !m_bShutdownRequested && !m_bReplacingPending;
	}
	TResult<bool> Commit_Internal();
	void FinishDispatch_Internal() noexcept;
	FAssetService* m_pAssets;
	FAudioPlayer* m_pAudio;
	DGameInstance* m_pGame;
	DScene* m_pPreparing = nullptr;
	FSceneStorage m_Storage;
	FSceneLifecycle m_Lifecycle;
	std::optional<FError> m_LastTransitionError;
	bool m_bBusy = false;
	bool m_bReplacingPending = false;
	bool m_bShutdown = false;
	bool m_bShutdownRequested = false;
	bool m_bQuit = false;
};
}
