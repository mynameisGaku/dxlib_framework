// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_SAVE_COURSE_PROGRESS_CODEC_H
#define DXF_SAMPLE_SAVE_COURSE_PROGRESS_CODEC_H
#include "SaveCourseProgress.h"
namespace Dxf::GameplaySample
{
/**
 * コース固有の鍵・得点・解錠を検証する純粋codec。World・音声・保存先を変更しない。
 */
class FSaveCourseProgressCodec
{
public:
	/**
	 * @param Progress ゲームが所有する進行値。不明キー・重複鍵・未解錠の開扉を拒否する。
	 */
	static void Validate(const FSaveCourseProgress& Progress);
	/**
	 * @param Progress 検証して安定した順序へ書く値。共有値は変更しない。
	 */
	static Toolbox::FString Encode(const FSaveCourseProgress& Progress);
	/**
	 * @param Bytes 長さ付きの保存Object。不明項目・将来バージョン・整数破損は例外。
	 */
	static FSaveCourseProgress Decode(Toolbox::FStringView Bytes);
};
} // namespace Dxf::GameplaySample
#endif
