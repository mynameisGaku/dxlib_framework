// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_RENDER_DEBUG_SCENE_H
#define DXF_RENDER_DEBUG_SCENE_H
#include "Dxf/GameScene.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Dxf/DebugCamera3D.h"
#include "Dxf/DebugStepController.h"
#include "Dxf/PhysicsDebugRecorder3D.h"
#include "Dxf/PhysicsDebugDisplay2D.h"
#include "Dxf/PhysicsDebugDisplay3D.h"
#include "Dxf/Font.h"
#include "Dxf/PhysicsDebugPicking3D.h"
namespace Dxf::RenderDebug
{
/**
 * 基本3D、固定更新状態、カメラ、表示切替を同時に確認する独立サンプル。
 * StarterとSandboxの振る舞いは変更しない。物理の再実行で履歴を作らない。
 */
class ARenderDebugScene final : public DGameScene
{
public:
	/**
	 * @param Jobs Applicationが所有し、Sceneより長く存続する共有JobSystem。
	 */
	explicit ARenderDebugScene(Toolbox::FJobSystem& Jobs);
	/**
	 * 描画・選択で共有する採取値。借用は次の更新まで。Worldを再採取しない。
	 */
	const FPhysicsDebugSnapshot3D& GetDisplaySnapshot() const noexcept;
	/**
	 * 今回描画する準備済みビューを返す。
	 */
	const FRenderView3D& GetDisplayView() const noexcept;
	/**
	 * 選択中の採取元ID。Live Worldでの生存保証ではない。
	 */
	Toolbox::TOptional<FColliderId3D> GetPickedCollider() const;
	/**
	 * サンプルが実際にStepへ渡した秒数。観察のためのStepは行わない。
	 */
	Toolbox::f64 GetSimulationSeconds() const noexcept;
	/**
	 * 保存された履歴件数。観察OFFでも既存履歴を保持する。
	 */
	Toolbox::size_t GetHistoryCount() const noexcept;

protected:
	/**
	 * @param Context フォント資源の取得に使用する初期化Context。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * @param Context ゲーム停止に影響されない、このSceneの入力と実経過時間。
	 */
	void OnTick(const FTickContext& Context) override;
	/**
	 * @param Render 次元別描画入口。3Dビューはフレーム中に一度だけ設定する。
	 */
	void OnDraw(FRenderContext& Render) const override;
	/**
	 * 観察値とWorldを順序どおり破棄する。
	 */
	void OnDeinitialize() noexcept override;
private:
	/**
	 * 半透明三枚の重なりを比較表示するか。F7で切り替える。
	 */
	bool m_bTransparencyDemo = false;

	/**
	 * 2D・3D Worldと観察値を作り直す。失敗時は既存Worldを維持する。
	 */
	void ResetSimulation_Internal();
	/**
	 * @param Input マウス・キー操作。
	 * @param Seconds 実経過秒数。
	 */
	void UpdateCamera_Internal(const FInputSnapshot& Input, Toolbox::f64 Seconds);
	/**
	 * 正常Step完了後にWorldから明示採取する。観察無効時はWorldへ触れない。
	 * @param ForceHistory 手送り・停止では現在の実状態も履歴へ保存する。
	 */
	void Capture_Internal(bool ForceHistory);
	/**
	 * 2D表示が有効な場合だけ2D Worldを採取する。
	 */
	void Capture2D_Internal();
	/**
	 * @param Render 2D描画入口。
	 */
	void DrawPanel_Internal(FRender2DContext& Render) const;
	/**
	 * 借用する共有JobSystem。
	 */
	Toolbox::FJobSystem* m_pJobs;
	/**
	 * サンプルの3D World。描画機能を知らない既存の本体実装。
	 */
	Toolbox::TUniquePtr<FPhysicsWorld3D> m_pWorld;
	/**
	 * 2D観察の最小例に使う2D World。
	 */
	Toolbox::TUniquePtr<FPhysicsWorld2D> m_pWorld2D;
	/**
	 * Worldが採取した値の最新値と、6 Stepごと最大120件の履歴。手送り時は各Stepを保存する。
	 */
	FPhysicsDebugRecorder3D m_Recorder;
	/**
	 * 観察のみの履歴から選択した値。
	 */
	FPhysicsDebugSnapshot3D m_Selected;
	/**
	 * 2D表示が有効な間だけ採取した2D観察値。
	 */
	FPhysicsDebugSnapshot2D m_Live2D;
	/**
	 * 2D観察の表示位置と縮尺。
	 */
	FPhysicsDebugView2D m_View2D;
	/**
	 * ゲーム側の停止と固定時間の計画。
	 */
	FDebugStepController m_Simulation;
	/**
	 * 表示専用のカメラ。
	 */
	FDebugCamera3D m_Camera;
	/**
	 * カメラ以外のビュー設定。
	 */
	FRenderView3D m_View;
	/**
	 * 更新後に一度準備し、クリックと描画で共有するビュー。
	 */
	FRenderView3D m_DrawView;
	/**
	 * 世代を含む選択ID。履歴Snapshotのm_Selectedとは別の状態。
	 */
	Toolbox::TOptional<FColliderId3D> m_PickedCollider;
	/**
	 * 表示用ビューを確定する。初期化時と更新の末尾で呼ぶ。
	 */
	void PrepareView_Internal();
	/**
	 * 表示対象のIDを照合し、必要なら同じSnapshotをクリック選択する。
	 * @param Input 現フレームの入力。採取やWorld更新は行わない。
	 */
	void UpdatePicking_Internal(const FInputSnapshot& Input);

	/**
	 * 物理の重ね表示設定。
	 */
	FPhysicsDebugDisplaySettings3D m_Display;
	/**
	 * 画面の説明用フォント。
	 */
	FFont m_Font;
	/**
	 * 履歴選択の古さ。0は最新のliveを使う。
	 */
	Toolbox::size_t m_HistoryAge = 0;
	/**
	 * サンプルの時計。実際に正常完了したStepへ渡した秒数の合計。
	 */
	Toolbox::f64 m_SimSeconds = 0;
	/**
	 * 最後の物理StepのCPU経過マイクロ秒。GPU計測ではない。
	 */
	Toolbox::uint64 m_PhysicsMicros = 0;
	/**
	 * 最後の計画で捨てたゲーム秒数。
	 */
	Toolbox::f64 m_DroppedSeconds = 0;
	/**
	 * 画面説明を表示するか。
	 */
	bool m_bPanel = true;
	/**
	 * 物理の観察線を表示するか。
	 */
	bool m_bOverlay = false;
	/**
	 * 2D観察の最小例を表示・採取するか。F9で切り替える。
	 */
	bool m_b2D = false;
	/**
	 * ゲーム側だけを0.25倍速にするか。
	 */
	bool m_bSlow = false;
	/**
	 * マウスの連続差分が有効か。
	 */
	bool m_bMouseTracked = false;
	/**
	 * 前回のマウスX座標。
	 */
	Toolbox::int32 m_PreviousMouseX = 0;
	/**
	 * 前回のマウスY座標。
	 */
	Toolbox::int32 m_PreviousMouseY = 0;
};
}
#endif
