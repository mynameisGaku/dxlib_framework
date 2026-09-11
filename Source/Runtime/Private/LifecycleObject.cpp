#include "Dxf/LifecycleObject.h"
#include "Dxf/GuardValue.h"
#include <exception>
namespace Dxf
{
TResult<void> DLifecycleObject::Initialize_Internal(const FInitContext& Context)
{
    if (m_State != ELifecycleState::Pending || m_bBusy || m_bDestroyRequested)
    { return TResult<void>::Failure(EErrorCode::InvalidState, "Object cannot initialize in this state"); }
    TResult<void> Result;
    {
        TGuardValue Guard(m_bBusy, true);
        m_State = ELifecycleState::Initializing;
        m_bInitializationAttempted = true;
        try
        {
            Result = OnInitialize(Context);
            if (Result && m_pChildren && !m_bDestroyRequested)
            {
                m_pChildren->FreezeBoundary_Internal();
                Result = m_pChildren->CommitBoundary_Internal(Context);
            }
        }
        catch (const std::exception& Error) { Result = TResult<void>::Failure(EErrorCode::UserException, Error.what()); }
        catch (...) { Result = TResult<void>::Failure(EErrorCode::UserException, "Unknown initialization exception"); }
    }
    if (!Result) { Shutdown_Internal(); return Result; }
    m_State = ELifecycleState::Active;
    return {};
}
TResult<void> DLifecycleObject::Tick_Internal(const FTickContext& Context)
{
    if (m_bBusy) { return TResult<void>::Failure(EErrorCode::InvalidState, "Reentrant object update"); }
    if (!IsInitialized() || m_bDestroyRequested) { return {}; }
    TGuardValue Guard(m_bBusy, true);
    if (!Context.Time.bPaused || m_bTickWhenPaused) { OnTick(Context); }
    if (m_pChildren && !m_bDestroyRequested) { return m_pChildren->Tick_Internal(Context); }
    return {};
}
TResult<void> DLifecycleObject::Draw_Internal(FRenderContext& Context)
{
    if (m_bBusy) { return TResult<void>::Failure(EErrorCode::InvalidState, "Reentrant object draw"); }
    if (!IsInitialized() || m_bDestroyRequested || !m_bVisible) { return {}; }
    TGuardValue Guard(m_bBusy, true);
    OnDraw(Context);
    if (m_pChildren && !m_bDestroyRequested) { return m_pChildren->Draw_Internal(Context); }
    return {};
}
void DLifecycleObject::Shutdown_Internal() noexcept
{
    if (m_bBusy) { m_bDestroyRequested = true; return; }
    if (m_State == ELifecycleState::Stopped) { return; }
    TGuardValue Guard(m_bBusy, true);
    m_State = ELifecycleState::Stopping;
    if (m_pChildren) { m_pChildren->Shutdown_Internal(); }
    if (m_bInitializationAttempted) { OnDeinitialize(); }
    m_State = ELifecycleState::Stopped;
}
}
