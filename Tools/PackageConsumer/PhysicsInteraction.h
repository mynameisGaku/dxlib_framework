// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_PHYSICS_INTERACTION_H
#define DXF_PACKAGE_PHYSICS_INTERACTION_H
#include "Toolbox/Utility.h"

/**
 * Physics単独の2D／3Dイベント・Sensor・支持計算を検証し、成功なら0、失敗なら識別番号を返す。
 */
Toolbox::int32 RunPhysicsInteraction();
#endif
