#pragma once
#include "Dxf/LifecycleObject.h"
#include "Dxf/SlotMap.h"
#include <optional>
namespace Dxf
{
/** Freezes requested mutations recursively before executing any user callbacks. */
template <typename T>
class TManagedLifecycle
{
public:
    explicit TManagedLifecycle(TSlotMap<T>& Storage) : m_pStorage(&Storage) {}
    void FreezeBoundary_Internal()
    {
        if (m_bFrozen) { return; }
        m_Batch.clear();
        for (auto Handle : m_pStorage->Snapshot())
        {
            T* Object = m_pStorage->Find_Internal(Handle.GetId());
            const auto Operation = Object->IsDestroyRequested() ? EOperation::Destroy : (Object->GetState() == ELifecycleState::Pending ? EOperation::Initialize : EOperation::Keep);
            m_Batch.push_back({Handle, Operation});
            if (Operation == EOperation::Keep && Object->GetChildren_Internal()) { Object->GetChildren_Internal()->FreezeBoundary_Internal(); }
        }
        m_bFrozen = true;
    }
    TResult<void> CommitBoundary_Internal(const FInitContext& Context)
    {
        if (!m_bFrozen) { FreezeBoundary_Internal(); }
        auto Batch = std::move(m_Batch); m_Batch.clear(); m_bFrozen = false;
        std::optional<FError> FirstError;
        for (const auto& Entry : Batch)
        {
            if (Entry.Operation != EOperation::Destroy) { continue; }
            if (T* Object = m_pStorage->Find_Internal(Entry.Handle.GetId()))
            { Object->Shutdown_Internal(); m_pStorage->Remove(Entry.Handle); }
        }
        for (const auto& Entry : Batch)
        {
            T* Object = m_pStorage->Find_Internal(Entry.Handle.GetId());
            if (!Object || Object->IsDestroyRequested() || Entry.Operation == EOperation::Destroy) { continue; }
            TResult<void> Result;
            if (Entry.Operation == EOperation::Initialize)
            {
                Result = Object->Initialize_Internal(Context);
                if (!Result) { m_pStorage->Remove(Entry.Handle); }
            }
            else if (auto* Children = Object->GetChildren_Internal()) { Result = Children->CommitBoundary_Internal(Context); }
            if (!Result && !FirstError) { FirstError = Result.Error(); }
        }
        return FirstError ? TResult<void>::Failure(*FirstError) : TResult<void>{};
    }
    void Shutdown_Internal() noexcept
    {
        m_Batch.clear(); m_bFrozen = false;
        m_pStorage->ForEach_Internal([](T& Object) { Object.Shutdown_Internal(); });
        m_pStorage->RemoveIf_Internal([](const T&) { return true; });
    }
private:
    enum class EOperation { Initialize, Keep, Destroy };
    struct FEntry
    {
        TObjectHandle<T> Handle;
        EOperation Operation;
    };
    TSlotMap<T>* m_pStorage;
    std::vector<FEntry> m_Batch;
    bool m_bFrozen = false;
};
}
