#pragma once
#include "Dxf/Sound.h"
#include "Dxf/SlotMap.h"
namespace Dxf
{
struct FPlaybackOptions
{
	bool bLoop = false;
	float Volume = 1.0f;
	std::uint64_t Scope = 0;
};
class DPlayback final : public DObject
{
public:
	DPlayback(FNativeHandle Handle, std::uint64_t Scope) : m_Handle(std::move(Handle)), m_Scope(Scope)
	{
	}
	int GetHandle_Internal() const noexcept
	{
		return m_Handle.Get();
	}
	std::uint64_t GetScope() const noexcept
	{
		return m_Scope;
	}
	bool IsHandleAccessible_Internal() const noexcept override
	{
		return m_Handle.Get() >= 0;
	}
private:
	FNativeHandle m_Handle;
	std::uint64_t m_Scope;
};
using FPlaybackHandle = TObjectHandle<DPlayback>;
class FAudioPlayer
{
public:
	explicit FAudioPlayer(ISoundBackend& Backend) : m_pBackend(&Backend)
	{
	}
	~FAudioPlayer()
	{
		Shutdown();
	}
	FAudioPlayer(const FAudioPlayer&) = delete;
	FAudioPlayer& operator=(const FAudioPlayer&) = delete;
	TResult<FPlaybackHandle> Play(const FSound& Sound, const FPlaybackOptions& Options = {});
	bool Stop(FPlaybackHandle Handle) noexcept;
	TResult<void> SetVolume(FPlaybackHandle Handle, float Volume);
	void StopScope(std::uint64_t Scope) noexcept;
	TResult<void> Tick();
	void Shutdown() noexcept;
private:
	TResult<FPlaybackHandle> Play_Internal(const FSound& Sound, const FPlaybackOptions& Options);
	ISoundBackend* m_pBackend;
	TSlotMap<DPlayback> m_Playbacks;
	bool m_bShutdown = false;
};
}
