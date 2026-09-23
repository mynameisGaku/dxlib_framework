// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_MODEL_SMOKE_STEP_H
#define DXF_TEST_MODEL_SMOKE_STEP_H
#include "Dxf/Result.h"
namespace Dxf::Testing
{
/**
 * 実モデル試験の継続または注入失敗が、実際の結果と一致するか調べる。
 * @param Result 一度だけ実行したStepの結果。
 * @param bExpectFailure 受付後のモデル失効エラーを期待するか。
 * @param bInjected 資源失効を実際に実行したか。
 */
FORCEINLINE bool ModelSmokeStepMatches(const TResult<bool>& Result, bool bExpectFailure, bool bInjected)
{
	if (!bExpectFailure)
	{
		return Result && Result.Value();
	}
	return bInjected && !Result && Result.Error().Code == EErrorCode::UserException && Result.Error().Message == "Model instance was released before drawing";
}
} // namespace Dxf::Testing
#endif
