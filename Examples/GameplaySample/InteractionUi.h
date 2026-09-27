// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_UI_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_UI_H
#include "Dxf/UiSceneHost.h"
#include "Dxf/UiAssetTextService.h"
#include "Dxf/UiPopup.h"
#include "Dxf/UiLabel.h"
namespace Dxf::GameplaySample
{
/**
 * 相互作用サンプルの設定画面。既存のUI入力仲介で操作を消費し、Scene時計だけを止める。
 */
class FInteractionUi final : public IInputRouter
{
public:
	/**
	 * Sceneと文字資源へ接続する。ToggleSplitは表示設定だけを変更する。
	 */
	void Initialize(DScene& Scene, FAssetService& Assets, Toolbox::TFunction<void()> ToggleSplit);
	/**
	 * Sceneが生存している間に入力の接続を外す。
	 */
	~FInteractionUi() override;
	/**
	 * 設定の表示、入力消費、描画先寸法の更新を一回行う。
	 */
	FInputSnapshot RouteInput(const FTickContext& Context) override;
	/**
	 * 状態表示と設定画面を重ねる。Statusはこのフレームのゲーム状態。
	 */
	void Draw(FRenderContext& Render, const char* Status);
	/**
	 * 現在の描画先寸法。Window情報が来るまでは起動時の寸法。
	 */
	Toolbox::int32 GetWidth() const noexcept
	{
		return m_Width;
	}
	Toolbox::int32 GetHeight() const noexcept
	{
		return m_Height;
	}

private:
	/**
	 * 設定を開き、閉じたら元のポーズ状態へ戻す。
	 */
	void ToggleSettings_Internal();
	/**
	 * 所有Scene。Sceneのメンバーとして保持される。
	 */
	DScene* m_pScene = nullptr;
	/**
	 * 文字資源はRootより長く生存する。
	 */
	Toolbox::TUniquePtr<FUiAssetTextService> m_pText;
	/**
	 * この画面の部品を所有する。
	 */
	Toolbox::TUniquePtr<FUiRoot> m_pRoot;
	/**
	 * 入力消費と全画面描画の既存窓口。
	 */
	FUiSceneHost m_Host;
	/**
	 * Scene終了時に購読を解除する。
	 */
	FUiScope m_Scope;
	/**
	 * 設定画面と状態の表示先。
	 */
	TUiRef<DUiPopup> m_Settings;
	TUiRef<DUiLabel> m_Status;
	/**
	 * 設定を開く前の時計の状態。
	 */
	bool m_bWasPaused = false;
	/**
	 * 描画だけを省く検証用の表示切替。入力処理と物理更新は変えない。
	 */
	bool m_bVisible = true;
	/**
	 * ウィンドウから取得した描画先寸法。
	 */
	Toolbox::int32 m_Width = 1280;
	Toolbox::int32 m_Height = 720;
};
} // namespace Dxf::GameplaySample
#endif
