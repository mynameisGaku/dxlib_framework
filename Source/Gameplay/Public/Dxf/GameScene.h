#pragma once
#include "Dxf/Scene.h"
#include "Dxf/GameObjectCollection.h"
namespace Dxf
{
/**
 * ゲームオブジェクトを所有して更新するシーン。
 */
class DGameScene : public DScene
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	DGameScene()
	{
		AttachChildren_Internal(m_Objects);
	}
	/**
	 * ゲームオブジェクトを生成して登録する。
	 * @param Args 生成先へ転送する引数。
	 */
	template <typename T, typename... TArgs> TResult<TObjectHandle<T>> Spawn(TArgs&&... Args)
	{
		return m_Objects.Spawn<T>(Toolbox::Forward<TArgs>(Args)...);
	}
	/**
	 * 対象の破棄を要求する。
	 * @param Object オブジェクト。
	 */
	template <typename T> bool Destroy(TObjectHandle<T> Object) noexcept
	{
		return m_Objects.Destroy(Object);
	}
	/**
	 * 指定した型のゲームオブジェクトを探す。
	 */
	template <typename T> TObjectHandle<T> FindObject() const noexcept
	{
		return m_Objects.FindFirst<T>();
	}
	/**
	 * 所有するオブジェクト群を取得する。
	 */
	template <typename T> Toolbox::TVector<TObjectHandle<T>> GetObjects() const
	{
		return m_Objects.FindAll<T>();
	}
	/**
	 * オブジェクトの数を取得する。
	 */
	FORCEINLINE Toolbox::size_t GetObjectCount() const noexcept
	{
		return m_Objects.Size();
	}

private:
	/**
	 * 所有するオブジェクト群。
	 */
	FGameObjectCollection m_Objects;
};
} // namespace Dxf
