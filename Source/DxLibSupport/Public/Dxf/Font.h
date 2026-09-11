#pragma once
#include "Dxf/AssetBackend.h"
#include "Dxf/ResourceRegistry.h"
namespace Dxf
{
using FFontResource = TResourceRecord<FFontOptions>;
class FFont
{
public:
    FFont() = default;
    explicit FFont(std::shared_ptr<FFontResource> Resource) : m_pResource(std::move(Resource)) {}
    bool IsValid() const noexcept { return GetNativeHandle_Internal() >= 0; }
    int GetNativeHandle_Internal() const noexcept { return m_pResource ? m_pResource->GetHandle_Internal() : -1; }
    const std::shared_ptr<FFontResource>& GetResource_Internal() const noexcept { return m_pResource; }
private:
    std::shared_ptr<FFontResource> m_pResource;
};
}
