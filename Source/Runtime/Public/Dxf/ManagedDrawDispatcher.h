#pragma once
#include "Dxf/LifecycleObject.h"
#include "Dxf/SlotMap.h"
#include <algorithm>
namespace Dxf
{
/** Dispatch order is creation order; visual sorting belongs to RenderQueue2D. */
template <typename T>
class TManagedDrawDispatcher
{
public:
    explicit TManagedDrawDispatcher(TSlotMap<T>& Storage) : m_pStorage(&Storage) {}
    TResult<void> Draw_Internal(FRenderContext& Context)
    {
        auto Objects = m_pStorage->Snapshot();
        std::sort(Objects.begin(), Objects.end(), [&](const auto& A, const auto& B)
        { return m_pStorage->Find_Internal(A.GetId())->GetCreationOrder_Internal() < m_pStorage->Find_Internal(B.GetId())->GetCreationOrder_Internal(); });
        for (auto Handle : Objects)
        {
            T* Object = m_pStorage->Find_Internal(Handle.GetId());
            if (!Object) { continue; }
            auto Result = Object->Draw_Internal(Context); if (!Result) { return Result; }
        }
        return {};
    }
private:
    TSlotMap<T>* m_pStorage;
};
}
