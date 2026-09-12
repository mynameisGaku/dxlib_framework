#pragma once
#include "Dxf/ComponentCollection.h"
namespace Dxf
{
class DGameObject : public DLifecycleObject
{
public:
	DGameObject() : m_Components(*this)
	{
		AttachChildren_Internal(m_Components);
	}
	void Destroy() noexcept
	{
		RequestDestroy_Internal();
	}
	template <typename T, typename... TArgs> TResult<TObjectHandle<T>> AddComponent(TArgs&&... Args)
	{
		return m_Components.Spawn<T>(std::forward<TArgs>(Args)...);
	}
	template <typename T> bool RemoveComponent(TObjectHandle<T> Component) noexcept
	{
		return m_Components.Destroy(Component);
	}
	template <typename T> TObjectHandle<T> FindComponent() const noexcept
	{
		return m_Components.FindFirst<T>();
	}
	template <typename T> std::vector<TObjectHandle<T>> GetComponents() const
	{
		return m_Components.FindAll<T>();
	}
	std::size_t GetComponentCount() const noexcept
	{
		return m_Components.Size();
	}
private:
	FComponentCollection m_Components;
};
}
