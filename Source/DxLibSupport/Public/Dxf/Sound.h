#pragma once
#include "Dxf/AssetBackend.h"
#include "Dxf/ResourceRegistry.h"
namespace Dxf
{
struct FSoundMetadata
{
	std::string Path;
	FSoundLoadOptions Options;
};
using FSoundResource = TResourceRecord<FSoundMetadata>;
class FSound
{
public:
	FSound() = default;
	explicit FSound(std::shared_ptr<FSoundResource> Resource) : m_pResource(std::move(Resource))
	{
	}
	bool IsValid() const noexcept
	{
		return GetNativeHandle_Internal() >= 0;
	}
	int GetNativeHandle_Internal() const noexcept
	{
		return m_pResource ? m_pResource->GetHandle_Internal() : -1;
	}
	const std::shared_ptr<FSoundResource>& GetResource_Internal() const noexcept
	{
		return m_pResource;
	}
private:
	std::shared_ptr<FSoundResource> m_pResource;
};
}
