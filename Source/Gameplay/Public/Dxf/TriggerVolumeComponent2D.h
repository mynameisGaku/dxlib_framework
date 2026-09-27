#pragma once
#include "Dxf/PhysicsEventTraits2D.h"
#include "Dxf/TriggerVolumeComponent.h"
namespace Dxf
{
/**
 * 2DのTriggerの領域の登録内容。
 */
using FTriggerVolumeDescription2D = TTriggerVolumeDescription<FPhysicsEventTraits2D>;
/**
 * 2Dの自分のSensorへ入っているBodyの集合を更新するComponent（TTriggerVolumeComponentの2D版）。
 */
using DTriggerVolume2DComponent = TTriggerVolumeComponent<FPhysicsEventTraits2D>;
} // namespace Dxf
