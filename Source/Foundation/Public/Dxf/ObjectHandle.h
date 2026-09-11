#pragma once
#include "Dxf/Object.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <limits>
namespace Dxf
{
struct FObjectId
{
	std::uint64_t Domain = 0;
	std::size_t Index = std::numeric_limits<std::size_t>::max();
	std::uint64_t Generation = 0;
	bool operator==(const FObjectId&) const = default;
};
namespace Detail
{
struct FHandleDomain
{
	std::function<DObject*(FObjectId)> Resolve;
};
}
template <typename T> class TSlotMap;
/** Non-owning, domain + slot + generation checked reference. Main-thread use only. */
template <typename T>
class TObjectHandle
{
public:
	TObjectHandle() = default;
	T* Get() const noexcept
	{
		const auto Domain = m_pDomain.lock();
		DObject* Object = Domain && Domain->Resolve ? Domain->Resolve(m_Id) : nullptr;
		return Object && Object->IsHandleAccessible_Internal() ? dynamic_cast<T*>(Object) : nullptr;
	}
	explicit operator bool() const noexcept
	{
		return Get() != nullptr;
	}
	FObjectId GetId() const noexcept
	{
		return m_Id;
	}
	template <typename U> TObjectHandle<U> Cast() const noexcept
	{
		return TObjectHandle<U>(m_pDomain, m_Id);
	}
	bool operator==(const TObjectHandle& Other) const noexcept
	{
		return m_Id == Other.m_Id;
	}
private:
	template <typename> friend class TSlotMap;
	template <typename> friend class TObjectHandle;
	TObjectHandle(std::weak_ptr<Detail::FHandleDomain> Domain, FObjectId Id) : m_pDomain(std::move(Domain)), m_Id(Id)
	{
	}
	std::weak_ptr<Detail::FHandleDomain> m_pDomain;
	FObjectId m_Id;
};
}
