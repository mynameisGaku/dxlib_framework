// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_TEST_CAPSULE_ALLOCATION_FAULT_TESTS_H
#define DXF_PHYSICS_TEST_CAPSULE_ALLOCATION_FAULT_TESTS_H
#include "Toolbox/Utility.h"
namespace PhysicsTest
{
/**
 * 隔離した確保故障注入exeで、2D／3Dのカプセルの登録・Step中の確保（組の候補の配列を含む）・カプセルのSensorのイベント・
 * 押す要求・高さの変更を検証し、失敗数を返す。
 */
Toolbox::int32 RunCapsuleAllocationChecks();
} // namespace PhysicsTest
#endif
