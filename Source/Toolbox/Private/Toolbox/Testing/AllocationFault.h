// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_TEST_ALLOCATION_FAULT_H
#define TOOLBOX_TEST_ALLOCATION_FAULT_H
#ifndef DXF_ALLOCATION_FAULT_TEST_EXECUTABLE
#error Allocation fault helpers must only be linked into an isolated test executable
#endif
#include "Toolbox/Utility.h"
namespace Toolbox::Testing
{
/**
 * 呼出しスレッドの指定回目の通常・アラインメント付き確保だけを失敗させる。
 * @param Countdown 0は次の確保、負値は注入停止。
 */
void SetAllocationFailureCountdown(int64 Countdown) noexcept;
/**
 * 注入が実際に行われたか。新しいCountdown設定時に解除する。
 */
bool WasAllocationFailureInjected() noexcept;
/**
 * この実行ファイルで追跡する未解放の確保件数。
 */
uint64 GetOutstandingTestAllocations() noexcept;
}
#endif
