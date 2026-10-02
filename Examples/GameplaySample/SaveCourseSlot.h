// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_SAVE_COURSE_SLOT_H
#define DXF_SAMPLE_SAVE_COURSE_SLOT_H
#include "Toolbox/String.h"
namespace Dxf::GameplaySample
{
/**
 * 所有側が要求時だけ更新するスロット表示値。UIがファイルを読む入口にはしない。
 */
struct FSaveCourseSlot
{
	/**
	 * 手動系列またはautosaveの検証済み識別子。
	 */
	Toolbox::FString Name;
	/**
	 * 空／正常／前面回復／破損／非互換／読取失敗の表示。
	 */
	Toolbox::FString Status = "未確認";
	/**
	 * 旧面回復・互換性などの元の原因。
	 */
	Toolbox::FString Reason;
	/**
	 * 確定面があるか。不正な面も明示的な上書き確認の対象にする。
	 */
	bool bOccupied = false;
};
} // namespace Dxf::GameplaySample
#endif
