// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_UI_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_UI_H
#include "Dxf/UiSceneHost.h"
#include "Dxf/UiAssetTextService.h"
#include "Dxf/UiPopup.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiScrollView.h"
namespace Dxf::GameplaySample
{
/**
 * 相互作用サンプルの設定画面。既存のUI入力仲介で操作を消費し、Scene時計だけを止める。
 */
class FInteractionUi final : public IInputRouter
{
public:
	/**
	 * Sceneと文字資源へ接続する。ToggleSplitは表示設定だけを、ToggleShape／TogglePushは遊び方だけを変更する
	 * （物理の状態は変えず、一時停止中の切り替えは再開後の固定更新から反映される）。
	 */
	void Initialize(DScene& Scene, FAssetService& Assets, Toolbox::TFunction<void()> ToggleSplit,
	                Toolbox::TFunction<void()> ToggleShape, Toolbox::TFunction<void()> TogglePush, Toolbox::TFunction<void(DUiPanel&, FUiScope&)> Populate = {}, bool bContent = false);
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
	 * @param Scale 有限の利用者倍率。描画とクリック判定へ同じ表示設定を渡す。
	 */
	void SetUserScale(Toolbox::f32 Scale);
	/**
	 * 現在の設定Modalが開いているか。
	 */
	bool IsSettingsOpen() const noexcept;
	/**
	 * @param bEnabled 既存コースのH表示切替。再割当ゲームではHを通常Actionに使える。
	 */
	void SetHudToggleEnabled(bool bEnabled) noexcept;
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
	 * 倍率を反映する全画面の表示先。
	 */
	FUiDisplayId m_Display = 0;
	/**
	 * 同じ表示・入力変換に使う現在の設定。
	 */
	FUiDisplayOptions m_DisplayOptions;
	/**
	 * Scene終了時に購読を解除する。
	 */
	FUiScope m_Scope;
	/**
	 * 設定画面と状態の表示先。
	 */
	TUiRef<DUiPopup> m_Settings;
	/**
	 * Content操作だけを小さい描画先でスクロールする。
	 */
	TUiRef<DUiScrollView> m_ContentScroll;
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
	 * 旧コース専用のH操作を受け付けるか。
	 */
	bool m_bHudToggleEnabled = true;
	/**
	 * ウィンドウから取得した描画先寸法。
	 */
	Toolbox::int32 m_Width = 1280;
	Toolbox::int32 m_Height = 720;
};
} // namespace Dxf::GameplaySample
#endif
