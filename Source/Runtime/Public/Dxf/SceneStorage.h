#pragma once
#include "Toolbox/UniquePtr.h"
#include "Dxf/Scene.h"
#include "Toolbox/SharedPtr.h"
namespace Dxf
{
/**
 * シーンの所有だけを担当する。準備と遷移の規則はNavigatorとLifecycleが管理する。
 */
class FSceneStorage
{
public:
	/**
	 * 現在の状態を取得する。
	 */
	FORCEINLINE DScene* GetCurrent() const noexcept
	{
		return m_pCurrent.Get();
	}
	/**
	 * 次の境界で反映する要求を設定する。
	 * @param Scene 対象のシーン。
	 */
	void SetPending_Internal(Toolbox::TUniquePtr<DScene> Scene)
	{
		m_pPending = Toolbox::Move(Scene);
	}
	/**
	 * 待機中の対象の所有権を取り出す。
	 */
	FORCEINLINE Toolbox::TUniquePtr<DScene> TakePending_Internal() noexcept
	{
		return Toolbox::Move(m_pPending);
	}
	/**
	 * 現在の状態を設定する。
	 * @param Scene 対象のシーン。
	 */
	void SetCurrent_Internal(Toolbox::TUniquePtr<DScene> Scene)
	{
		m_pCurrent = Toolbox::Move(Scene);
	}
	/**
	 * 蓄積した内容を消去する。
	 */
	void Clear_Internal() noexcept
	{
		m_pPending.Reset();
		m_pCurrent.Reset();
	}

private:
	/**
	 * 現在の状態。
	 */
	Toolbox::TUniquePtr<DScene> m_pCurrent;
	/**
	 * 次の境界で反映する要求。
	 */
	Toolbox::TUniquePtr<DScene> m_pPending;
};
} // namespace Dxf
