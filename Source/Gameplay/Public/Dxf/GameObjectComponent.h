#pragma once
#include "Dxf/LifecycleObject.h"
namespace Dxf
{
class DGameObject;
class DGameObjectComponent : public DLifecycleObject
{
public:
	/** Valid from OnInitialize until OnDeinitialize completes; not during construction. */
	DGameObject* GetOwner() const noexcept
	{
		return m_pOwner;
	}
	void Destroy() noexcept
	{
		RequestDestroy_Internal();
	}
	void SetOwner_Internal(DGameObject& Owner) noexcept
	{
		m_pOwner = &Owner;
	}
private:
	DGameObject* m_pOwner = nullptr;
};
}
