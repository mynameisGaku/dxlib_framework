#pragma once
#include "Dxf/ManagedCollection.h"
#include "Dxf/GameObjectComponent.h"
namespace Dxf
{
using FComponentStorage = TSlotMap<DGameObjectComponent>;
using FComponentLifecycle = TManagedLifecycle<DGameObjectComponent>;
using FComponentUpdater = TManagedUpdater<DGameObjectComponent>;
using FComponentDrawDispatcher = TManagedDrawDispatcher<DGameObjectComponent>;
class FComponentCollection final : public TManagedCollection<DGameObjectComponent>
{
public:
    explicit FComponentCollection(DGameObject& Owner) : m_pOwner(&Owner) {}
private:
    void PrepareObject_Internal(DGameObjectComponent& Component) override { Component.SetOwner_Internal(*m_pOwner); }
    DGameObject* m_pOwner;
};
}
