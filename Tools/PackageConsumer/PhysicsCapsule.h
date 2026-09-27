// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_PHYSICS_CAPSULE_H
#define DXF_PACKAGE_PHYSICS_CAPSULE_H
#include "Toolbox/Utility.h"

/**
 * dxf::physicsだけで、2D／3Dのカプセルの剛体・問い合わせ・Sensorのイベント・SolverのBroadPhaseの経路を検証する。
 * 成功は0、失敗は検査ごとの番号（341〜）。
 */
Toolbox::int32 RunPhysicsCapsule();
#endif
