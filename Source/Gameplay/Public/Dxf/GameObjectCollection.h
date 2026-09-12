#pragma once
#include "Dxf/GameObject.h"
namespace Dxf
{
/**
 * ゲームオブジェクトの世代付き所有領域。
 */
using FGameObjectStorage = TSlotMap<DGameObject>;
/**
 * ゲームオブジェクトの初期化と終了を制御する型。
 */
using FGameObjectLifecycle = TManagedLifecycle<DGameObject>;
/**
 * ゲームオブジェクトの更新を実行する型。
 */
using FGameObjectUpdater = TManagedUpdater<DGameObject>;
/**
 * ゲームオブジェクトへ描画を通知する型。
 */
using FGameObjectDrawDispatcher = TManagedDrawDispatcher<DGameObject>;
/**
 * ゲームオブジェクトの所有と通知を管理する型。
 */
class FGameObjectCollection final : public TManagedCollection<DGameObject>
{
};
} // namespace Dxf
