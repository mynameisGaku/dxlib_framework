// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_CONTENT_COURSE_PORTAL_H
#define DXF_SAMPLE_CONTENT_COURSE_PORTAL_H
#include "Dxf/GameObject.h"
#include "Dxf/SceneContentRequest.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiScope.h"
#include "Dxf/UiPanel.h"
namespace Dxf::GameplaySample
{
/**
 * 現在のSceneを残して準備し、Ready値を通常更新からNavigatorへ渡す利用例。
 * WorkerへSceneやNative資源を捕捉しない。
 */
class DContentCoursePortal final : public DGameObject
{
public:
	/**
	 * @param b3D 読み込む次元。ファイル自身の次元も厳密に検査する。
	 */
	explicit DContentCoursePortal(bool b3D);
	/**
	 * @param Path ProjectRoot相対のSceneファイル。受付は次の通常更新。
	 */
	void RequestLoad(Toolbox::FString Path);
	/**
	 * 準備中の要求を取り消す。現在のSceneは変更しない。
	 */
	void RequestCancel() noexcept;
	/**
	 * @param Panel 既存F1設定画面。
	 * @param Scope SceneのUI購読寿命。
	 * @param Self Collectionの世代付き参照。破棄後のUI通知を抑止する。
	 */
	void BuildPanel(DUiPanel& Panel, FUiScope& Scope, TObjectHandle<DContentCoursePortal> Self);
	/**
	 * 最後の要求状態。採用の成功とファイルの準備成功を区別する。
	 */
	ESceneContentRequestState GetRequestState() const noexcept;

protected:
	/**
	 * @param Context 借りる所有側AssetService。準備中に再生しない。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * @param Context 所有側の通常更新。Task Commitへ再入しない。
	 */
	void OnTick(const FTickContext& Context) override;
	/**
	 * 対象Scopeだけを退役し、要求のCPU入力寿命を保つ。
	 */
	void OnDeinitialize() noexcept override;

private:
	/**
	 * 所有側の資源窓口。Applicationが長く生存する。
	 */
	FAssetService* m_pAssets = nullptr;
	/**
	 * この要求だけのScopeとCPU状態。
	 */
	Toolbox::TUniquePtr<FSceneContentRequest> m_pRequest;
	/**
	 * 最後の要求の診断を読む非所有チケット。
	 */
	FSceneContentTicket m_Ticket;
	/**
	 * 次回に読むファイル。空なら追加要求なし。
	 */
	Toolbox::FString m_Path;
	/**
	 * 状態が変わったときだけ更新する既存UIラベル。
	 */
	TUiRef<DUiLabel> m_Status;
	/**
	 * 前回表示した状態。定常で診断文字列を再確保しない。
	 */
	ESceneContentRequestState m_LastState = ESceneContentRequestState::Pending;
	/**
	 * 読み込む次元。
	 */
	bool m_b3D;
	/**
	 * 受付時に固定した次元。UIの次回選択で進行中の型を変更しない。
	 */
	bool m_bRequested3D = false;
	/**
	 * 次の更新で取消しを採用するか。
	 */
	bool m_bCancel = false;
	/**
	 * 準備値をNavigatorへ一度渡したか。
	 */
	bool m_bSubmitted = false;
};
} // namespace Dxf::GameplaySample
#endif
