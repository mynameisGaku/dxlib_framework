// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_JOINT_COURSE_SUPPORT_H
#define DXF_SAMPLE_JOINT_COURSE_SUPPORT_H
#include "SampleHud.h"
namespace Dxf::GameplaySample
{
/**
 * 配置の公開操作が失敗した場合、その最初の理由をSceneへ伝える。
 * @param Result SpawnまたはAddComponentの結果。
 */
template <typename T>
T JointCourseValue_Internal(TResult<T> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
	return Toolbox::Move(Result.Value());
}
} // namespace Dxf::GameplaySample
#endif
