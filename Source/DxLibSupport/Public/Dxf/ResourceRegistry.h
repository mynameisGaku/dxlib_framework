#pragma once
#include "Dxf/NativeHandle.h"
#include <memory>
#include <vector>
namespace Dxf
{
class IResourceRecord
{
public:
	virtual ~IResourceRecord() = default;
	virtual void Release_Internal() noexcept = 0;
};
template <typename TMetadata>
class TResourceRecord final : public IResourceRecord
{
public:
	TResourceRecord(FNativeHandle Handle, TMetadata Metadata) : m_Handle(std::move(Handle)), m_Metadata(std::move(Metadata))
	{
	}
	void Release_Internal() noexcept override
	{
		m_Handle.Reset();
	}
	int GetHandle_Internal() const noexcept
	{
		return m_Handle.Get();
	}
	const void* GetBackendIdentity_Internal() const noexcept
	{
		return m_Handle.GetBackendIdentity_Internal();
	}
	const TMetadata& GetMetadata() const noexcept
	{
		return m_Metadata;
	}
private:
	FNativeHandle m_Handle;
	TMetadata m_Metadata;
};
class FResourceRegistry
{
public:
	~FResourceRegistry()
	{
		Shutdown();
	}
	FResourceRegistry() = default;
	FResourceRegistry(const FResourceRegistry&) = delete;
	FResourceRegistry& operator=(const FResourceRegistry&) = delete;
	bool Register(const std::shared_ptr<IResourceRecord>& Resource)
	{
		if (!Resource)
		{
			return false;
		}
		if (m_bShutdown)
		{
			Resource->Release_Internal();
			return false;
		}
		m_Records.emplace_back(Resource);
		return true;
	}
	void CollectUnused()
	{
		std::erase_if(m_Records, [](const auto& Record)
		{
			return Record.expired();
		});
	}
	void Shutdown() noexcept
	{
		if (m_bShutdown)
		{
			return;
		}
		m_bShutdown = true;
		// Release callbacks may call Shutdown or CollectUnused. Detach the list
		// before invoking external code so it cannot invalidate our traversal.
		auto Records = std::move(m_Records);
		for (auto& Record : Records)
		{
			if (auto Resource = Record.lock())
			{
				Resource->Release_Internal();
			}
		}
	}
	bool IsShutdown() const noexcept
	{
		return m_bShutdown;
	}
private:
	std::vector<std::weak_ptr<IResourceRecord>> m_Records;
	bool m_bShutdown = false;
};
}
