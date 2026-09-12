#pragma once
#include "Dxf/ComponentCollection.h"
namespace Dxf
{
/**
 * シーン内のオブジェクトとコンポーネントを管理する。
 */
class DGameObject : public DLifecycleObject
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	DGameObject() : m_Components(*this)
	{
		AttachChildren_Internal(m_Components);
	}
	/**
	 * 対象の破棄を要求する。
	 */
	void Destroy() noexcept
	{
		RequestDestroy_Internal();
	}
	/**
	 * コンポーネントを生成して所有者へ追加する。
	 * @param Args 生成先へ転送する引数。
	 */
	template <typename T, typename... TArgs> TResult<TObjectHandle<T>> AddComponent(TArgs&&... Args)
	{
		return m_Components.Spawn<T>(Toolbox::Forward<TArgs>(Args)...);
	}
	/**
	 * コンポーネントの取り外しを要求する。
	 * @param Component 対象のコンポーネント。
	 */
	template <typename T> bool RemoveComponent(TObjectHandle<T> Component) noexcept
	{
		return m_Components.Destroy(Component);
	}
	/**
	 * 指定した型のコンポーネントを探す。
	 */
	template <typename T> TObjectHandle<T> FindComponent() const noexcept
	{
		return m_Components.FindFirst<T>();
	}
	/**
	 * 所有するコンポーネント群を取得する。
	 */
	template <typename T> Toolbox::TVector<TObjectHandle<T>> GetComponents() const
	{
		return m_Components.FindAll<T>();
	}
	/**
	 * コンポーネントの数を取得する。
	 */
	FORCEINLINE Toolbox::size_t GetComponentCount() const noexcept
	{
		return m_Components.Size();
	}

private:
	/**
	 * 所有するコンポーネント群。
	 */
	FComponentCollection m_Components;
};
} // namespace Dxf
