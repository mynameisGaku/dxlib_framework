// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_TEST_QUERY_INDEX_ALLOCATION_FAULT_TESTS_H
#define DXF_PHYSICS_TEST_QUERY_INDEX_ALLOCATION_FAULT_TESTS_H
#include "Toolbox/Utility.h"
namespace PhysicsTest
{
/**
 * 問い合わせ索引の確保の試験（確保置換を隔離した実行ファイルで実行する）。結果を1行ずつ出力し、失敗の数を返す。
 * Body・Colliderの登録の途中の確保失敗で索引に幽霊の葉を残さないこと、変更直後の問い合わせと姿勢の変更で確保しないこと。
 */
Toolbox::int32 RunQueryIndexAllocationChecks();
} // namespace PhysicsTest
#endif
