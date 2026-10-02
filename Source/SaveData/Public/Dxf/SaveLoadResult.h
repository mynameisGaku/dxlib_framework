// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVELOADRESULT_H
#define DXF_SAVELOADRESULT_H
#include "Dxf/SaveDocument.h"
#include "Dxf/ESaveLoadState.h"
#include "Toolbox/Optional.h"
namespace Dxf
{
/**
 * 正常面の論理値と、前の面から回復した事実を分けて返す。
 */
struct FSaveLoadResult
{
	/**
	 * 通常読込か回復か。
	 */
	ESaveLoadState State = ESaveLoadState::Empty;
	/**
	 * 空slotでは値を持たない。
	 */
	Toolbox::TOptional<FSaveDocument> Document;
	/**
	 * 不正面の元の診断。成功だけに読み替えない。
	 */
	Toolbox::FString RecoveryReason;
};
} // namespace Dxf
#endif
