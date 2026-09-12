#pragma once
#include <utility>
namespace Dxf
{
/** Move-only owner. Deleters must be nonthrowing and accept only valid handles. */
class FNativeHandle
{
public:
	FNativeHandle() = default;
	using FReleaseFunction = void (*)(void*, int) noexcept;
	/** Release must be non-null for a valid handle; Context must outlive this owner. */
	FNativeHandle(int Handle, void* Context, FReleaseFunction Release) noexcept
		: m_Handle(Handle), m_pContext(Context), m_pRelease(Release)
	{
	}
	~FNativeHandle()
	{
		Reset();
	}
	FNativeHandle(const FNativeHandle&) = delete;
	FNativeHandle& operator=(const FNativeHandle&) = delete;
	FNativeHandle(FNativeHandle&& Other) noexcept
		: m_Handle(std::exchange(Other.m_Handle, -1)), m_pContext(Other.m_pContext), m_pRelease(Other.m_pRelease)
	{
	}
	FNativeHandle& operator=(FNativeHandle&& Other) noexcept
	{
		if (this != &Other)
		{
			Reset();
			m_Handle = std::exchange(Other.m_Handle, -1);
			m_pContext = Other.m_pContext;
			m_pRelease = Other.m_pRelease;
		}
		return *this;
	}
	int Get() const noexcept
	{
		return m_Handle;
	}
	const void* GetBackendIdentity_Internal() const noexcept
	{
		return m_pContext;
	}
	void Reset() noexcept
	{
		const int Handle = std::exchange(m_Handle, -1);
		if (Handle >= 0 && m_pRelease)
		{
			m_pRelease(m_pContext, Handle);
		}
	}
private:
	int m_Handle = -1;
	void* m_pContext = nullptr;
	FReleaseFunction m_pRelease = nullptr;
};
}
