// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_SAVE_COURSE_PROGRESS_H
#define DXF_SAMPLE_SAVE_COURSE_PROGRESS_H
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Dxf::GameplaySample
{
/**
 * チェックポイントから再開するコースの進行値。物理の実行時IDや移動中の速度を持たない。
 */
struct FSaveCourseProgress
{
	/**
	 * プレイヤーの安全な再開位置を選ぶ論理キー。
	 */
	Toolbox::FString Checkpoint = "start";
	/**
	 * 再開後の通知で二重取得しない鍵の論理キー集合。
	 */
	Toolbox::TVector<Toolbox::FString> CollectedKeys;
	/**
	 * 取得した得点。保存では厳密な十進文字列を使う。
	 */
	Toolbox::uint64 Score = 0;
	/**
	 * 明示生成した荷物の論理キーへ使う通し番号。除去後も巻き戻さない。
	 */
	Toolbox::uint64 SpawnCounter = 0;
	/**
	 * 意味のある進行変更を区別する通し番号。同じ値ではautosaveを繰り返さない。
	 */
	Toolbox::uint64 Revision = 0;
	/**
	 * doorA／doorBの解錠状態。配列位置はこのC++値の内側だけで使う。
	 */
	bool bDoorUnlocked[2] = {};
	/**
	 * 再開レシピで扉を最初から開いた配置にする論理状態。
	 */
	bool bDoorOpen[2] = {};
};
} // namespace Dxf::GameplaySample
#endif
