#pragma once
#include <functional>
#include <utility>
namespace Dxf
{
/** Move-only owner. Deleters must be nonthrowing and accept only valid handles. */
class FNativeHandle
{
public:
    FNativeHandle() = default;
    FNativeHandle(int Handle, std::function<void(int)> Deleter) : m_Handle(Handle), m_Deleter(std::move(Deleter)) {}
    ~FNativeHandle() { Reset(); }
    FNativeHandle(const FNativeHandle&) = delete;
    FNativeHandle& operator=(const FNativeHandle&) = delete;
    FNativeHandle(FNativeHandle&& Other) noexcept
        : m_Handle(std::exchange(Other.m_Handle, -1)), m_Deleter(std::move(Other.m_Deleter)) {}
    FNativeHandle& operator=(FNativeHandle&& Other) noexcept
    {
        if (this != &Other) { Reset(); m_Handle = std::exchange(Other.m_Handle, -1); m_Deleter = std::move(Other.m_Deleter); }
        return *this;
    }
    int Get() const noexcept { return m_Handle; }
    void Reset() noexcept
    {
        const int Handle = std::exchange(m_Handle, -1);
        if (Handle >= 0 && m_Deleter) { m_Deleter(Handle); }
    }
private:
    int m_Handle = -1;
    std::function<void(int)> m_Deleter;
};
}
