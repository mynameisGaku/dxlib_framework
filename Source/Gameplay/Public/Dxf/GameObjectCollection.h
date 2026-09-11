#pragma once
#include "Dxf/GameObject.h"
namespace Dxf
{
using FGameObjectStorage = TSlotMap<DGameObject>;
using FGameObjectLifecycle = TManagedLifecycle<DGameObject>;
using FGameObjectUpdater = TManagedUpdater<DGameObject>;
using FGameObjectDrawDispatcher = TManagedDrawDispatcher<DGameObject>;
class FGameObjectCollection final : public TManagedCollection<DGameObject>
{
};
}
