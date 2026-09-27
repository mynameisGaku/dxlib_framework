// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_TEST_WORLD_EVENT_ALLOCATION_FAULT_TESTS_H
#define DXF_PHYSICS_TEST_WORLD_EVENT_ALLOCATION_FAULT_TESTS_H
#include "Toolbox/Utility.h"
namespace PhysicsTest
{
/**
 * 隔離した確保故障注入exeで、2D／3Dイベント生成がStep中に確保しないことを検証する。
 * 初回・慣らし後・再登録・密集・容量超過と復帰を確認し、失敗数を返す。
 */
Toolbox::int32 RunWorldEventAllocationChecks();
} // namespace PhysicsTest
#endif
