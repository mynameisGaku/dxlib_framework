// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_SAVE_COURSE_INPUT_H
#define DXF_SAMPLE_SAVE_COURSE_INPUT_H
#include "Dxf/UserSettings.h"
#include "Dxf/InputProfilePolicy.h"
namespace Dxf::GameplaySample
{
/**
 * SampleのゲームActionと予約入力。OS入力を別に取得しない。
 */
class FSaveCourseInput
{
public:
	/**
	 * 許可する完全なAction集合と、メニューから脱出するための予約入力。
	 */
	static FInputProfilePolicy Policy();
	/**
	 * WASD移動、SpaceでJump、Eで扉、Gで荷物を生成する初期設定。
	 */
	static FUserSettings Defaults();
};
} // namespace Dxf::GameplaySample
#endif
