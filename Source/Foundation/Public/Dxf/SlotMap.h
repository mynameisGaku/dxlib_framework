#pragma once
#include "Dxf/ObjectHandle.h"
#include <atomic>
#include <stdexcept>
#include <vector>
namespace Dxf
{
inline std::uint64_t AllocateDomain_Internal()
{
	static std::atomic<std::uint64_t> Next{1};
	const auto Value = Next.fetch_add(1, std::memory_order_relaxed);
	if (Value == 0)
	{
		throw std::overflow_error("Handle domain exhausted");
	}
	return Value;
}
/** Sole owner of instances; never invokes gameplay callbacks. */
template <typename T>
class TSlotMap
{
public:
	TSlotMap() : m_Domain(AllocateDomain_Internal()), m_pDomain(std::make_shared<Detail::FHandleDomain>())
	{
		m_pDomain->Resolve = [this](FObjectId Id) -> DObject*
		{
			return Find_Internal(Id);
		};
	}
	~TSlotMap()
	{
		m_pDomain->Resolve = {};
	}
	TSlotMap(const TSlotMap&) = delete;
	TSlotMap& operator=(const TSlotMap&) = delete;
	TSlotMap(TSlotMap&&) = delete;
	TSlotMap& operator=(TSlotMap&&) = delete;
	TObjectHandle<T> Insert(std::unique_ptr<T> Object)
	{
		if (!Object)
		{
			return {};
		}
		std::size_t Index = 0;
		for (; Index < m_Slots.size(); ++Index)
		{
			if (!m_Slots[Index].Object && m_Slots[Index].Generation != std::numeric_limits<std::uint64_t>::max())
			{
				break;
			}
		}
		if (Index == m_Slots.size())
		{
			m_Slots.emplace_back();
		}
		auto& Slot = m_Slots[Index];
		Slot.Object = std::move(Object);
		++m_Size;
		return TObjectHandle<T>(m_pDomain, {m_Domain, Index, Slot.Generation});
	}
	template <typename U> bool Remove(const TObjectHandle<U>& Handle) noexcept
	{
		const auto Id = Handle.GetId();
		if (!Find_Internal(Id))
		{
			return false;
		}
		// Publish the removal before invoking a user destructor. It may insert a new
		// object into this slot or grow m_Slots, so no slot reference may survive it.
		auto Removed = std::move(m_Slots[Id.Index].Object);
		++m_Slots[Id.Index].Generation;
		--m_Size;
		return true;
	}
	T* Find_Internal(FObjectId Id) const noexcept
	{
		if (Id.Domain != m_Domain || Id.Index >= m_Slots.size())
		{
			return nullptr;
		}
		const auto& Slot = m_Slots[Id.Index];
		return Id.Generation == Slot.Generation ? Slot.Object.get() : nullptr;
	}
	std::vector<TObjectHandle<T>> Snapshot() const
	{
		std::vector<TObjectHandle<T>> Result;
		Result.reserve(m_Size);
		for (std::size_t Index = 0; Index < m_Slots.size(); ++Index)
		{
			const auto& Slot = m_Slots[Index];
			if (Slot.Object)
			{
				Result.push_back(TObjectHandle<T>(m_pDomain, {m_Domain, Index, Slot.Generation}));
			}
		}
		return Result;
	}
	template <typename TPredicate> void RemoveIf_Internal(TPredicate Predicate) noexcept
	{
		const auto Count = m_Slots.size();
		for (std::size_t Index = 0; Index < Count; ++Index)
		{
			T* Object = m_Slots[Index].Object.get();
			const auto Generation = m_Slots[Index].Generation;
			if (Object && Predicate(*Object))
			{
				Remove(TObjectHandle<T>(m_pDomain, {m_Domain, Index, Generation}));
			}
		}
	}
	template <typename TFunction> void ForEach_Internal(TFunction Function)
	{
		for (auto& Slot : m_Slots)
		{
			if (Slot.Object)
			{
				Function(*Slot.Object);
			}
		}
	}
	template <typename U> TObjectHandle<U> FindFirst() const noexcept
	{
		static_assert(std::is_base_of_v<T, U>);
		for (std::size_t Index = 0; Index < m_Slots.size(); ++Index)
		{
			const auto& Slot = m_Slots[Index];
			if (Slot.Object && Slot.Object->IsHandleAccessible_Internal() && dynamic_cast<U*>(Slot.Object.get()))
			{
				return TObjectHandle<U>(m_pDomain, {m_Domain, Index, Slot.Generation});
			}
		}
		return {};
	}
	template <typename U> std::vector<TObjectHandle<U>> FindAll() const
	{
		static_assert(std::is_base_of_v<T, U>);
		std::vector<TObjectHandle<U>> Result;
		for (auto Handle : Snapshot())
		{
			auto Typed = Handle.template Cast<U>();
			if (Typed)
			{
				Result.push_back(Typed);
			}
		}
		return Result;
	}
	std::size_t Size() const noexcept
	{
		return m_Size;
	}
private:
	struct FSlot
	{
		std::unique_ptr<T> Object;
		std::uint64_t Generation = 1;
	};
	std::vector<FSlot> m_Slots;
	std::size_t m_Size = 0;
	std::uint64_t m_Domain;
	std::shared_ptr<Detail::FHandleDomain> m_pDomain;
};
}
