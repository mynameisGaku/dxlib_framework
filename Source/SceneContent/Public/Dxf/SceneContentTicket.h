// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SCENE_CONTENT_TICKET_H
#define DXF_SCENE_CONTENT_TICKET_H
#include "Dxf/SceneContentRequestState.h"
#include "Dxf/SceneContentDiagnostic.h"
#include "Toolbox/SharedPtr.h"
namespace Dxf
{
namespace ContentPrivate
{
/**
 * CPU準備だけを共有する内部状態。Native資源やSceneを保持しない。
 */
struct FContentRequestState;
} // namespace ContentPrivate
/**
 * 個別要求の観察値。後続要求が来ても元の取消し・置換を観察できる。
 */
class FSceneContentTicket
{
public:
	/**
	 * 空の観察値。GetStateはCanceledを返す。
	 */
	FSceneContentTicket() = default;
	/**
	 * @param State Requestが発行する内部CPU状態。利用側では生成しない。
	 */
	explicit FSceneContentTicket(Toolbox::TSharedPtr<ContentPrivate::FContentRequestState> State);
	/**
	 * 準備結果と終端状態を、CPU側との競合なしに読む。
	 */
	ESceneContentRequestState GetState() const noexcept;
	/**
	 * 失敗位置の値コピー。失敗していないときは空。
	 */
	FSceneContentDiagnostic GetDiagnostic() const;
	/**
	 * Request内の通し番号。WorldのStep番号ではない。
	 */
	Toolbox::uint64 GetSequence() const noexcept;

private:
	/**
	 * 終了済み要求も観察できるCPU値だけの共有参照。
	 */
	Toolbox::TSharedPtr<ContentPrivate::FContentRequestState> m_State;
};
} // namespace Dxf
#endif
