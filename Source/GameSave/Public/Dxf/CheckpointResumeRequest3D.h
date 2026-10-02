// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_RESUME_REQUEST_3D_H
#define DXF_CHECKPOINT_RESUME_REQUEST_3D_H
#include "Dxf/CheckpointScene3D.h"
#include "Dxf/CheckpointCatalog3D.h"
#include "Dxf/GameSaveRequest.h"
#include "Dxf/SceneNavigator.h"
#include "Toolbox/Function.h"
namespace Dxf
{
namespace SavePrivate
{
class FCheckpointResumeRuntime3D;
}
/**
 * 長期所有者の読込と再開窓口。CPU検証、所有側資源準備、通常Scene要求、必須Readyを分ける。
 */
class FCheckpointResumeRequest3D
{
public:
	/**
	 * @param Saves 借用する保存要求。@param Scenes 通常の切替要求先。@param Assets 既存資源所有者。
	 * @param Catalog 信頼する不変Content。@param RestoreProgress 必須の純粋なゲーム進行検証・再開変換。
	 * @param CreateScene 任意のC++構築フック。Preparedと検証済み進行を渡し、未初期化の対応Sceneを返す。
	 * @param Limits 保存値の上限。依存先はこの窓口より長く生存させ、所有側Task/Commit外で使う。
	 */
	FCheckpointResumeRequest3D(FGameSaveRequest& Saves, FSceneNavigator& Scenes, FAssetService& Assets, FCheckpointCatalog3D Catalog, Toolbox::TFunction<FCheckpointDocument3D(const FCheckpointDocument3D&)> RestoreProgress, Toolbox::TFunction<Toolbox::TUniquePtr<ACheckpointScene3D>(FPreparedScene3D, FCheckpointDocument3D)> CreateScene = {}, FCheckpointLimits Limits = {});
	/**
	 * Sceneを破棄せず、自分の未完了結果だけを終端へ確定する。
	 */
	~FCheckpointResumeRequest3D();
	/**
	 * 要求番号と借用先を複製しない。
	 */
	FCheckpointResumeRequest3D(const FCheckpointResumeRequest3D&) = delete;
	/**
	 * 要求番号と借用先を複製しない。
	 */
	FCheckpointResumeRequest3D& operator=(const FCheckpointResumeRequest3D&) = delete;
	/**
	 * @param Slot 保存系列。互換性・種類・進行・全参照を準備前に確認する。
	 */
	FGameSaveTicket RequestLoad(Toolbox::FString Slot);
	/**
	 * 公開前だけ取消す。切替要求済みSceneの巻き戻しとは扱わない。
	 */
	bool Cancel();
	/**
	 * 通常更新で呼ぶ。Model等の資源準備はブロックし得る。Navigator.Commitを再入しない。
	 */
	void Poll();
	/**
	 * 自分の観察を終端へ確定する。借用する保存要求やScene全体を退役しない。
	 * Task/Commit・Scene通知中はfalse。切替要求後のScene巻き戻しは行わない。
	 */
	bool Retire() noexcept;

private:
	/**
	 * 所有側採用先。WorkerへScene・Native資源を捕捉しない。
	 */
	Toolbox::TUniquePtr<SavePrivate::FCheckpointResumeRuntime3D> m_pRuntime;
};
} // namespace Dxf
#endif
