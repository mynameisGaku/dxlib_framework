#pragma once
#include "Dxf/PhysicsEventTraits3D.h"
#include "Dxf/TriggerVolumeComponent.h"
namespace Dxf
{
/**
 * 3DのTriggerの領域の登録内容。
 */
using FTriggerVolumeDescription3D = TTriggerVolumeDescription<FPhysicsEventTraits3D>;
/**
 * 3Dの自分のSensorへ入っているBodyの集合を更新するComponent（TTriggerVolumeComponentの3D版）。
 */
using DTriggerVolume3DComponent = TTriggerVolumeComponent<FPhysicsEventTraits3D>;
} // namespace Dxf
