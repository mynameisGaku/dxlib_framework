#pragma once
#include "Dxf/ContactListenerComponent.h"
#include "Dxf/PhysicsEventTraits3D.h"
namespace Dxf
{
/**
 * 3Dの既存のBodyが関わる接触・Triggerを、物理Stepの成功後に配送するComponent（TContactListenerComponentの3D版）。
 */
using DContactListener3DComponent = TContactListenerComponent<FPhysicsEventTraits3D>;
} // namespace Dxf
