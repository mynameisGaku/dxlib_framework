#pragma once
#include "Dxf/Scene.h"
#include "Dxf/GameObjectCollection.h"
namespace Dxf
{
class DGameScene : public DScene
{
public:
	DGameScene()
	{
		AttachChildren_Internal(m_Objects);
	}
	template <typename T, typename... TArgs> TResult<TObjectHandle<T>> Spawn(TArgs&&... Args)
	{
		return m_Objects.Spawn<T>(std::forward<TArgs>(Args)...);
	}
	template <typename T> bool Destroy(TObjectHandle<T> Object) noexcept
	{
		return m_Objects.Destroy(Object);
	}
	std::size_t GetObjectCount() const noexcept
	{
		return m_Objects.Size();
	}
private:
	FGameObjectCollection m_Objects;
};
}
