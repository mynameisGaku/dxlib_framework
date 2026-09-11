#pragma once
#include "Dxf/LifecycleObject.h"
namespace Dxf
{
/** A scene need not contain GameObjects and can be used by non-game tools. */
class DScene : public DLifecycleObject
{
public:
    DScene() { SetTickWhenPaused(true); }
    FSceneClock& GetClock() noexcept { return m_Clock; }
    const FSceneClock& GetClock() const noexcept { return m_Clock; }
    std::uint64_t GetAudioScope() const noexcept { return m_AudioScope; }
    void Enter_Internal(const FSceneActivationContext& Context) noexcept
    {
        if (m_bEntered) { return; }
        m_AudioScope = Context.AudioScope; m_bEntered = true; OnEnter(Context);
    }
    void Exit_Internal() noexcept { if (m_bEntered) { m_bEntered = false; OnExit(); } }
protected:
    virtual void OnEnter(const FSceneActivationContext&) noexcept {}
    virtual void OnExit() noexcept {}
private:
    FSceneClock m_Clock;
    std::uint64_t m_AudioScope = 0;
    bool m_bEntered = false;
};
}
