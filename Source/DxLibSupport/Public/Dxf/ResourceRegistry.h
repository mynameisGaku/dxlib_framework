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
    TResourceRecord(FNativeHandle Handle, TMetadata Metadata) : m_Handle(std::move(Handle)), m_Metadata(std::move(Metadata)) {}
    void Release_Internal() noexcept override { m_Handle.Reset(); }
    int GetHandle_Internal() const noexcept { return m_Handle.Get(); }
    const TMetadata& GetMetadata() const noexcept { return m_Metadata; }
private:
    FNativeHandle m_Handle;
    TMetadata m_Metadata;
};
class FResourceRegistry
{
public:
    ~FResourceRegistry() { Shutdown(); }
    FResourceRegistry() = default;
    FResourceRegistry(const FResourceRegistry&) = delete;
    FResourceRegistry& operator=(const FResourceRegistry&) = delete;
    bool Register(const std::shared_ptr<IResourceRecord>& Resource)
    {
        if (m_bShutdown) { Resource->Release_Internal(); return false; }
        m_Records.emplace_back(Resource);
        return true;
    }
    void CollectUnused()
    {
        std::erase_if(m_Records, [](const auto& Record) { return Record.expired(); });
    }
    void Shutdown() noexcept
    {
        m_bShutdown = true;
        for (auto& Record : m_Records) { if (auto Resource = Record.lock()) { Resource->Release_Internal(); } }
        m_Records.clear();
    }
    bool IsShutdown() const noexcept { return m_bShutdown; }
private:
    std::vector<std::weak_ptr<IResourceRecord>> m_Records;
    bool m_bShutdown = false;
};
}
