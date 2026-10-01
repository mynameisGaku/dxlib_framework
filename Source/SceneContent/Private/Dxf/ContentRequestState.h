// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_REQUEST_STATE_H
#define DXF_CONTENT_REQUEST_STATE_H
#include "Dxf/SceneContentTicket.h"
#include "Dxf/SceneDefinition2D.h"
#include "Dxf/SceneDefinition3D.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/Mutex.h"
namespace Dxf::ContentPrivate
{
/**
 * Workerが保持できるCPU値だけの受渡し領域。資源やSceneへの参照を持たない。
 */
struct FContentRequestState
{
	/**
	 * 開始・取消し・置換を同じ原子的な値で確定する。
	 */
	Toolbox::TAtomic<Toolbox::uint32> Status{static_cast<Toolbox::uint32>(ESceneContentRequestState::Pending)};
	/**
	 * Prepareが例外でも戻ったことを示す。
	 */
	Toolbox::TAtomic<Toolbox::uint32> PrepareDone;
	/**
	 * 定義と診断の公開境界。
	 */
	mutable Toolbox::FMutex Mutex;
	/**
	 * 発行時に決める要求番号。
	 */
	Toolbox::uint64 Sequence = 0;
	/**
	 * この要求で読む次元。
	 */
	Toolbox::uint32 Dimension = 0;
	/**
	 * 所有側のCommitまで通ったCPU値だけをPollが採用する。
	 */
	bool bCommitted = false;
	/**
	 * 解析失敗を成功した空のSceneと区別する。
	 */
	bool bFailed = false;
	/**
	 * 完了前はMutexで保護する失敗位置。
	 */
	FSceneContentDiagnostic Diagnostic;
	/**
	 * 例外処理で診断の確保も失敗した場合の、確保しない最初の理由。
	 */
	char FailureReason[384] = {};
	/**
	 * 検証済み2D CPU定義。
	 */
	FSceneDefinition2D Definition2D;
	/**
	 * 検証済み3D CPU定義。
	 */
	FSceneDefinition3D Definition3D;
};
} // namespace Dxf::ContentPrivate
#endif
