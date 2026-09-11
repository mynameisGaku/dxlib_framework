#pragma once
#include "Dxf/LifecycleObject.h"
#include "Dxf/SlotMap.h"
#include <algorithm>
#include <tuple>
namespace Dxf
{
template <typename T>
class TManagedUpdater
{
public:
	explicit TManagedUpdater(TSlotMap<T>& Storage) : m_pStorage(&Storage)
	{
	}
	TResult<void> Tick_Internal(const FTickContext& Context)
	{
		auto Objects = m_pStorage->Snapshot();
		std::stable_sort(Objects.begin(), Objects.end(), [&](const auto& A, const auto& B)
		{
			const auto* First = m_pStorage->Find_Internal(A.GetId());
			const auto* Second = m_pStorage->Find_Internal(B.GetId());
			return std::tuple(First->GetUpdateOrder(), First->GetCreationOrder_Internal()) < std::tuple(Second->GetUpdateOrder(), Second->GetCreationOrder_Internal());
		});
		for (auto Handle : Objects)
		{
			T* Object = m_pStorage->Find_Internal(Handle.GetId());
			if (!Object)
			{
				continue;
			}
			auto Result = Object->Tick_Internal(Context);
			if (!Result)
			{
				return Result;
			}
		}
		return {};
	}
private:
	TSlotMap<T>* m_pStorage;
};
}
