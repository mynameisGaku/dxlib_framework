#pragma once
#include <memory>
#include <string>
#include <unordered_map>
namespace Dxf
{
template <typename TResource>
class TResourceCache
{
public:
    std::shared_ptr<TResource> Find(const std::string& Key) const
    {
        const auto It = m_Entries.find(Key);
        auto Resource = It != m_Entries.end() ? It->second.lock() : nullptr;
        return Resource && Resource->GetHandle_Internal() >= 0 ? Resource : nullptr;
    }
    void Insert(std::string Key, const std::shared_ptr<TResource>& Resource)
    {
        if (m_Entries.size() >= 128) { CollectUnused(); }
        m_Entries[std::move(Key)] = Resource;
    }
    void CollectUnused() { std::erase_if(m_Entries, [](const auto& Entry) { return Entry.second.expired(); }); }
    void Clear() noexcept { m_Entries.clear(); }
private:
    std::unordered_map<std::string, std::weak_ptr<TResource>> m_Entries;
};
}
