#pragma once
// Native translation units use narrow UTF-8 strings, independently of consumer UNICODE settings.
#ifndef DX_NON_USING_NAMESPACE_DXLIB
#define DX_NON_USING_NAMESPACE_DXLIB
#endif
#include "DxLib.h"
#include "Dxf/Result.h"
namespace Dxf::Detail
{
inline TResult<void> CheckNative_Internal(int Code, const char* Operation)
{
    return Code < 0 ? TResult<void>::Failure(EErrorCode::BackendFailure, Operation) : TResult<void>{};
}
}
