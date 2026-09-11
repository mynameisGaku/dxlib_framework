#pragma once
#include "Dxf/SceneStorage.h"
#include "Dxf/SceneLifecycle.h"
#include <optional>
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
    { return RequestChange(std::make_unique<T>(std::forward<TArgs>(Args)...)); }
    TResult<bool> Commit();
    TResult<void> CommitObjects();
    TResult<void> Tick(FFrameTime Time, const FInputSnapshot& Input);
    TResult<void> Draw(FRenderContext& Render);
    DScene* GetCurrent() const noexcept { return m_Storage.GetCurrent(); }
    const std::optional<FError>& GetLastTransitionError() const noexcept { return m_LastTransitionError; }
    void RequestQuit() noexcept { m_bQuit = true; }
    bool WantsQuit() const noexcept { return m_bQuit || m_bShutdown; }
    void Shutdown() noexcept;
private:
    TResult<bool> Commit_Internal();
    void FinishDispatch_Internal() noexcept;
    FAssetService* m_pAssets;
    FAudioPlayer* m_pAudio;
    DGameInstance* m_pGame;
    FSceneStorage m_Storage;
    FSceneLifecycle m_Lifecycle;
    std::optional<FError> m_LastTransitionError;
    bool m_bBusy = false;
    bool m_bShutdown = false;
    bool m_bShutdownRequested = false;
    bool m_bQuit = false;
};
}
