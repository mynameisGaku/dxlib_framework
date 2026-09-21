// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_RENDER_DEBUG_SCENE_H
#define DXF_RENDER_DEBUG_SCENE_H
#include "Dxf/GameScene.h"
#include "Dxf/RigidBody3D.h"
#include "Dxf/DebugCamera3D.h"
#include "Dxf/DebugStepController.h"
#include "Dxf/DebugSnapshotHistory.h"
#include "Dxf/PhysicsDebugDisplay3D.h"
#include "Dxf/Font.h"
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
	 * 観察登録の実Collider ID型。
	 */
	using FWatch = TPhysicsDebugWatch3D<FColliderId3D>;
	/**
	 * Worldと観察登録を一組で作り直す。失敗時は既存Worldを維持する。
	 */
	void ResetSimulation_Internal();
	/**
	 * @param Input マウス・キー操作。
	 * @param Seconds 実経過秒数。
	 */
	void UpdateCamera_Internal(const FInputSnapshot& Input, Toolbox::f64 Seconds);
	/**
	 * @param ForceHistory 手送り・停止では現在の実状態も履歴へ保存する。
	 */
	void Capture_Internal(bool ForceHistory);
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
	 * 作成時のCollider定義を保持する登録。
	 */
	Toolbox::TVector<FWatch> m_Watches;
	/**
	 * 固定更新直後の実状態。
	 */
	FPhysicsDebugSnapshot3D m_Live;
	/**
	 * 観察のみの履歴から選択した値。
	 */
	FPhysicsDebugSnapshot3D m_Selected;
	/**
	 * 10Hz、最大120件を基準に記録する履歴。手送り時は各tickを保存する。
	 */
	FDebugSnapshotHistory m_History;
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
	 * 物理の重ね表示設定。
	 */
	FPhysicsDebugDisplaySettings3D m_Display;
	/**
	 * 画面の説明用フォント。
	 */
	FFont m_Font;
	/**
	 * 実行済み固定更新番号。
	 */
	Toolbox::uint64 m_Tick = 0;
	/**
	 * 最後に履歴へ保存した更新番号。
	 */
	Toolbox::uint64 m_HistoryTick = 0;
	/**
	 * 履歴選択の古さ。0は最新のliveを使う。
	 */
	Toolbox::size_t m_HistoryAge = 0;
	/**
	 * 実行した物理更新の合計ゲーム秒数。
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
