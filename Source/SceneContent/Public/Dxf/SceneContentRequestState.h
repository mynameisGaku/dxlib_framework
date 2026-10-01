// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SCENE_CONTENT_REQUEST_STATE_H
#define DXF_SCENE_CONTENT_REQUEST_STATE_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 一つの読込要求の状態。取消し・置換・失敗は終端で、後から成功へ戻らない。
 */
enum class ESceneContentRequestState : Toolbox::uint8
{
	/**
	 * CPU準備の開始待ち。
	 */
	Pending,
	/**
	 * CPU準備または所有側の必須資源取込を待っている。
	 */
	Preparing,
	/**
	 * 必須資源まで準備した。SceneやBodyの生成完了とは別。
	 */
	Ready,
	/**
	 * 読込または必須資源の準備に失敗した。
	 */
	Failed,
	/**
	 * 明示取消しまたは所属Scopeの失効で採用しなかった。
	 */
	Canceled,
	/**
	 * 後続要求に置き換えられた。
	 */
	Superseded
};
} // namespace Dxf
#endif
