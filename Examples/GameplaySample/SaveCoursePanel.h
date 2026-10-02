// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_SAVE_COURSE_PANEL_H
#define DXF_SAMPLE_SAVE_COURSE_PANEL_H
#include "InteractionUi.h"
#include "Dxf/InputRebindCapture.h"
#include "Dxf/UserSettings.h"
#include "Dxf/UiSlider.h"
#include "Dxf/GameSaveRequestState.h"
namespace Dxf::GameplaySample
{
class ASaveCourseGameInstance;
/**
 * Sceneが所有する既存F1メニューの保存操作。I/O・Scene採用は長期GameInstanceへ要求する。
 */
class FSaveCoursePanel final : public IInputRouter
{
public:
	/**
	 * @param Scene 非所有の生存Scene。Panelより長く生存する。
	 * @param Game Application所有の長期窓口。Scene終了を越えて要求を保持する。
	 * @param Assets 既存の文字資源窓口。
	 * @param Dimension このゲームの2Dまたは3D。
	 * @param SpawnCargo Controllerへ送る動的生成要求。
	 * @param DestroyCargo 論理履歴へ送る明示的な個体除去要求。
	 * @param Door 解錠済みの専任Controllerへ開閉目標を渡す。
	 */
	FSaveCoursePanel(DScene& Scene, ASaveCourseGameInstance& Game, FAssetService& Assets, Toolbox::uint32 Dimension, Toolbox::TFunction<void()> SpawnCargo, Toolbox::TFunction<void()> DestroyCargo, Toolbox::TFunction<bool(Toolbox::uint32, bool)> Door);
	/**
	 * Sceneが生存中に入力予約を外す。UI購読は既存Scopeで終了する。
	 */
	~FSaveCoursePanel() override;
	/**
	 * @param Context 既存Snapshot。再割当中と閉鎖Frameの入力を消費する。
	 */
	FInputSnapshot RouteInput(const FTickContext& Context) override;
	/**
	 * @param Render 既存UIをゲーム描画後に一度重ねる。更新・保存は行わない。
	 */
	void Draw(FRenderContext& Render);
	/**
	 * 表示だけを二つのViewへ分けるか。
	 */
	bool IsSplit() const noexcept;
	/**
	 * 描画先幅。奇数寸法の分割にも元の画素値を使う。
	 */
	Toolbox::int32 GetWidth() const noexcept;
	/**
	 * 描画先高さ。
	 */
	Toolbox::int32 GetHeight() const noexcept;

private:
	/**
	 * @param Panel 既存のスクロールする設定画面。
	 * @param Scope 購読はScene寿命へ合わせる。
	 */
	void Build(DUiPanel& Panel, FUiScope& Scope);
	/**
	 * @param Device Keyboard／Mouse／Gamepadの採取装置。
	 * @param PadSlot 対象Pad、0〜3。
	 */
	void BeginRebind(EInputBindingDevice Device, Toolbox::uint32 PadSlot);
	/**
	 * @param Settings 全体候補。適用・保存は別操作にする。
	 */
	void Apply(FUserSettings Settings);
	/**
	 * @param Action UI操作の不正入力を診断へ残す。ゲーム実行時例外は対象外。
	 */
	void Invoke(Toolbox::TFunction<void()> Action);
	/**
	 * UIの要求状態が変わったときだけ文字列を作る。
	 */
	void UpdateStatus();
	/**
	 * 終了時に入力を外す非所有のScene。
	 */
	DScene* m_pScene;
	/**
	 * Applicationが最後に終了する要求所有者。
	 */
	ASaveCourseGameInstance* m_pGame;
	/**
	 * 既存UIの入力・描画・Modal・Scope。
	 */
	FInteractionUi m_Ui;
	/**
	 * 開始クリックを含むUI消費前の値。Workerへ渡さない。
	 */
	FInputSnapshot m_Input;
	/**
	 * 既存の解放待ちと単一押下採取。
	 */
	FInputRebindCapture m_Rebind;
	/**
	 * 次の所有側適用へ送る編集値。
	 */
	FUserSettings m_Editable;
	/**
	 * 物理を変更せず、Controllerへ論理目標だけを送る非所有のScene操作。
	 */
	Toolbox::TFunction<bool(Toolbox::uint32, bool)> m_Door;
	/**
	 * 保存系列と現在の状態表示。
	 */
	TUiRef<DUiLabel> m_Status;
	/**
	 * 再割当対象を選ぶ既存ボタンの説明。
	 */
	TUiRef<DUiLabel> m_ActionLabel;
	/**
	 * 既存スライダーの表示。既定へ戻すときはイベントなしで更新する。
	 */
	TUiRef<DUiSlider> m_Volume;
	/**
	 * 描画とhit testへ共通反映する利用者倍率。
	 */
	TUiRef<DUiSlider> m_Scale;
	/**
	 * 指定系列だけを対象とする上書き確認。
	 */
	TUiRef<DUiPopup> m_Overwrite;
	/**
	 * 確認する一系列。Cancelでは保存要求を出さない。
	 */
	Toolbox::FString m_PendingSlot;
	/**
	 * UI要求の失敗や再割当競合の診断。
	 */
	Toolbox::FString m_Error;
	/**
	 * 更新で準備したHUD。Drawは文書を採取しない。
	 */
	Toolbox::FString m_Text;
	/**
	 * 元のScene次元。
	 */
	Toolbox::uint32 m_Dimension;
	/**
	 * manual1〜3の選択番号。
	 */
	Toolbox::uint32 m_Slot = 0;
	/**
	 * 許可Actionの選択番号。
	 */
	Toolbox::uint32 m_Action = 0;
	/**
	 * Viewは表示だけを変更する。
	 */
	bool m_bSplit = false;
	/**
	 * Readyの入力を一度だけ適用する。
	 */
	bool m_bRebinding = false;
	/**
	 * 倍率のドラッグ中は入力座標を維持し、つまみの解放後に一度適用する。
	 */
	bool m_bScalePending = false;
	/**
	 * 通常表示で文書全体をencodeしない更新印。
	 */
	bool m_bDirty = true;
	/**
	 * 前回の進行要求状態。
	 */
	EGameSaveRequestState m_LastState = EGameSaveRequestState::Canceled;
	/**
	 * 前回の独立設定要求状態。
	 */
	EGameSaveRequestState m_LastSettingsState = EGameSaveRequestState::Canceled;
	/**
	 * 前回の採取待ち状態。
	 */
	bool m_bLastCapturing = false;
	/**
	 * 前回の設定未保存状態。
	 */
	bool m_bLastSettingsDirty = true;
	/**
	 * 同じ状態でも新要求の診断を更新する通し番号。
	 */
	Toolbox::uint64 m_LastSequence = 0;
	/**
	 * 更新されたスロット一覧の番号。
	 */
	Toolbox::uint64 m_LastSlotRevision = 0;
	/**
	 * 所有側が適用した設定の番号。
	 */
	Toolbox::uint64 m_LastSettingsRevision = 0;
	/**
	 * 要求前の失敗でも表示を更新する直前の診断。
	 */
	Toolbox::FString m_LastDiagnostic;
};
} // namespace Dxf::GameplaySample
#endif
