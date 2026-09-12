#pragma once
#include "Dxf/ManagedCollection.h"
#include "Dxf/GameObjectComponent.h"
namespace Dxf
{
/**
 * コンポーネントの世代付き所有領域。
 */
using FComponentStorage = TSlotMap<DGameObjectComponent>;
/**
 * コンポーネントの初期化と終了を制御する型。
 */
using FComponentLifecycle = TManagedLifecycle<DGameObjectComponent>;
/**
 * コンポーネントの更新を実行する型。
 */
using FComponentUpdater = TManagedUpdater<DGameObjectComponent>;
/**
 * コンポーネントへ描画を通知する型。
 */
using FComponentDrawDispatcher = TManagedDrawDispatcher<DGameObjectComponent>;
/**
 * コンポーネントの所有と通知を管理する型。
 */
class FComponentCollection final : public TManagedCollection<DGameObjectComponent>
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Owner 所有元のオブジェクト。
	 */
	explicit FComponentCollection(DGameObject& Owner) : m_pOwner(&Owner)
	{
	}

private:
	/**
	 * オブジェクトを使用可能な状態まで初期化する。
	 * @param Component 対象のコンポーネント。
	 */
	void PrepareObject_Internal(DGameObjectComponent& Component) override
	{
		Component.SetOwner_Internal(*m_pOwner);
	}
	/**
	 * 所有元のオブジェクト。
	 */
	DGameObject* m_pOwner;
};
} // namespace Dxf
