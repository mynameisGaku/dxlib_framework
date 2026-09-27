#pragma once
#include "Dxf/ContactListenerComponent.h"
#include "Dxf/PhysicsEventTraits2D.h"
namespace Dxf
{
/**
 * 2Dの既存のBodyが関わる接触・Triggerを、物理Stepの成功後に配送するComponent（TContactListenerComponentの2D版）。
 */
using DContactListener2DComponent = TContactListenerComponent<FPhysicsEventTraits2D>;
} // namespace Dxf
