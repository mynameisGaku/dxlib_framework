#pragma once
#include "Dxf/Object.h"
#include "Dxf/LifecycleGroup.h"
namespace Dxf
{
enum class ELifecycleState
{
	Pending,
	Initializing,
	Active,
	Stopping,
	Stopped
};
/** Framework dispatch is fixed; user overrides only the On* hooks. */
class DLifecycleObject : public DObject
{
public:
	ELifecycleState GetState() const noexcept
	{
		return m_State;
	}
	bool IsInitialized() const noexcept
	{
		return m_State == ELifecycleState::Active;
	}
	bool IsDestroyRequested() const noexcept
	{
		return m_bDestroyRequested;
	}
	void SetUpdateOrder(int Order) noexcept
	{
		m_UpdateOrder = Order;
	}
	int GetUpdateOrder() const noexcept
	{
		return m_UpdateOrder;
	}
	void SetTickWhenPaused(bool bEnabled) noexcept
	{
		m_bTickWhenPaused = bEnabled;
	}
	void SetVisible(bool bVisible) noexcept
	{
		m_bVisible = bVisible;
	}
	bool IsVisible() const noexcept
	{
		return m_bVisible;
	}
	bool IsHandleAccessible_Internal() const noexcept final
	{
		return !m_bDestroyRequested && m_State != ELifecycleState::Stopped && m_State != ELifecycleState::Stopping;
	}
	TResult<void> Initialize_Internal(const FInitContext& Context);
	TResult<void> Tick_Internal(const FTickContext& Context);
	TResult<void> Draw_Internal(FRenderContext& Context);
	void Shutdown_Internal() noexcept;
	void RequestDestroy_Internal() noexcept
	{
		m_bDestroyRequested = true;
	}
	void SetCreationOrder_Internal(std::uint64_t Order) noexcept
	{
		m_CreationOrder = Order;
	}
	std::uint64_t GetCreationOrder_Internal() const noexcept
	{
		return m_CreationOrder;
	}
	ILifecycleGroup* GetChildren_Internal() noexcept
	{
		return m_pChildren;
	}
protected:
	DLifecycleObject() = default;
	virtual TResult<void> OnInitialize(const FInitContext&)
	{
		return {};
	}
	virtual void OnTick(const FTickContext&)
	{
	}
	virtual void OnDraw(FRenderContext&) const
	{
	}
	/** Called once after an attempted initialization, including partial failure. Must not throw. */
	virtual void OnDeinitialize() noexcept
	{
	}
	void AttachChildren_Internal(ILifecycleGroup& Children) noexcept
	{
		m_pChildren = &Children;
	}
private:
	ILifecycleGroup* m_pChildren = nullptr;
	ELifecycleState m_State = ELifecycleState::Pending;
	std::uint64_t m_CreationOrder = 0;
	int m_UpdateOrder = 0;
	bool m_bDestroyRequested = false;
	bool m_bTickWhenPaused = false;
	bool m_bVisible = true;
	bool m_bBusy = false;
	bool m_bInitializationAttempted = false;
};
}
