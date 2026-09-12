#pragma once
#include "Dxf/LifecycleObject.h"
namespace Dxf
{
class DGameObject;
/**
 * ゲームオブジェクトに追加する振る舞いの基底型。
 */
class DGameObjectComponent : public DLifecycleObject
{
public:
	/**
	 * OnInitializeからOnDeinitialize完了まで有効。構築中は使用できない。
	 */
	DGameObject* GetOwner() const noexcept
	{
		return m_pOwner;
	}
	/**
	 * 対象の破棄を要求する。
	 */
	void Destroy() noexcept
	{
		RequestDestroy_Internal();
	}
	/**
	 * 所有元のオブジェクトを設定する。
	 * @param Owner 所有元のオブジェクト。
	 */
	void SetOwner_Internal(DGameObject& Owner) noexcept
	{
		m_pOwner = &Owner;
	}

private:
	/**
	 * 所有元のオブジェクト。
	 */
	DGameObject* m_pOwner = nullptr;
};
} // namespace Dxf
