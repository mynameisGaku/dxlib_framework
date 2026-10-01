// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_SCENE_CONNECTION_2D_H
#define DXF_CONTENT_SCENE_CONNECTION_2D_H
#include "Dxf/ContentSceneEndpoint.h"
#include "Dxf/ContentJointDefinition2D.h"
namespace Dxf
{
/**
 * Scene内の二つの公開Bodyを結ぶ初期設定。World番号と世代IDを保存しない。
 */
struct FContentSceneConnection2D
{
	/**
	 * A側の解決済み公開先。
	 */
	FContentSceneEndpoint BodyA;
	/**
	 * B側の解決済み公開先。
	 */
	FContentSceneEndpoint BodyB;
	/**
	 * 初期配置を反映済みのBodyローカルFrameと種類別設定。
	 */
	FContentJointDefinition2D Joint;
};
} // namespace Dxf
#endif
