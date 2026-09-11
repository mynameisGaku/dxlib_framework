#pragma once
#include "Dxf/Scene.h"
#include <memory>
namespace Dxf
{
/** Owns only; preparation and transition policy belong to SceneNavigator/Lifecycle. */
class FSceneStorage
{
public:
	DScene* GetCurrent() const noexcept
	{
		return m_pCurrent.get();
	}
	void SetPending_Internal(std::unique_ptr<DScene> Scene)
	{
		m_pPending = std::move(Scene);
	}
	std::unique_ptr<DScene> TakePending_Internal() noexcept
	{
		return std::move(m_pPending);
	}
	void SetCurrent_Internal(std::unique_ptr<DScene> Scene)
	{
		m_pCurrent = std::move(Scene);
	}
	void Clear_Internal() noexcept
	{
		m_pPending.reset();
		m_pCurrent.reset();
	}
private:
	std::unique_ptr<DScene> m_pCurrent;
	std::unique_ptr<DScene> m_pPending;
};
}
