// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_SCENE_2D_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_SCENE_2D_H
#include "InteractionCourse.h"
#include "InteractionUi.h"
#include "ContentCoursePortal.h"
#include "JointCourse2D.h"
#include "MechanismCourse2D.h"
#include "Dxf/Font.h"
#include "Dxf/MathTypes.h"
namespace Dxf::GameplaySample
{
/**
 * 2Dの接触・Trigger・動く床のサンプル。触れる箱・取得物・圧力板と扉・横に動く床・昇降床・傾く床・チェックポイント・危険領域を置く。
 * [Tab]で3Dへ、[I]でキャラクター移動のサンプルへ切り替える。[R]で最後のチェックポイントへ戻る、[V]で2画面、[P]で一時停止。
 * Worldのイベントはこのシーンの初期化で有効化する（接続の責任はシーン）。
 */
class DInteraction2DScene final : public DPhysicsScene2D, private IInteractionHost<FInteraction2D>
{
public:
	/**
	 * ゲームの規則と状態。
	 */
	FORCEINLINE FInteractionRules& GetGameRules() noexcept
	{
		return m_Rules;
	}
	FORCEINLINE const FInteractionRules& GetGameRules() const noexcept
	{
		return m_Rules;
	}
	/**
	 * 置いたオブジェクト。
	 */
	FORCEINLINE const TInteractionCourse<FInteraction2D>& GetCourse() const noexcept
	{
		return m_Course;
	}
	/**
	 * プレイヤー（初期化前・破棄後はnullptr）。
	 */
	FORCEINLINE DPlayer2D* GetPlayer() const noexcept
	{
		return m_Course.Player.Get();
	}
	/**
	 * プレイヤーを最後のチェックポイントへ戻す。
	 */
	void Respawn();
	/**
	 * 現在乗っている移動床を通常の寿命管理で破棄する。静止地面や空中なら何もしない。
	 */
	void RemoveSupport();
	/**
	 * 描画と同じカメラから画面座標を求める。
	 * @param World 世界の位置（メートル）。
	 * @param Side 0（左・全画面）または1（右）。
	 */
	FVector2 ToScreen(Toolbox::FVector2 World, Toolbox::int32 Side) const;
	/**
	 * 2画面表示を切り替える。
	 */
	FORCEINLINE void ToggleSplit() noexcept
	{
		m_bSplit = !m_bSplit;
	}
	/**
	 * 2画面表示か。
	 */
	FORCEINLINE bool IsSplit() const noexcept
	{
		return m_bSplit;
	}
	/**
	 * キャラクターの遊び方（形状・押し合い）。
	 */
	FORCEINLINE const FInteractionMode& GetMode() const noexcept override
	{
		return m_Mode;
	}
	/**
	 * 円／球とカプセルを切り替える（設定画面のボタンと同じ）。
	 */
	FORCEINLINE void ToggleShape() noexcept
	{
		m_Mode.ToggleShape();
	}
	/**
	 * 押し合いの有無を切り替える（設定画面のボタンと同じ）。
	 */
	FORCEINLINE void TogglePush() noexcept
	{
		m_Mode.TogglePush();
	}

	/**
	 * Sceneが所有する距離拘束の仕掛け。操作は更新側だけで行う。
	 */
	FJointCourse2D& GetJointCourse() noexcept
	{
		return m_JointCourse;
	}
	/**
	 * 描画と観察用の仕掛け参照。
	 */
	const FJointCourse2D& GetJointCourse() const noexcept
	{
		return m_JointCourse;
	}

	/**
	 * 実描画サイズの幅。
	 */
	Toolbox::int32 GetDisplayWidth() const noexcept
	{
		return m_Ui.GetWidth();
	}
	/**
	 * 実描画サイズの高さ。
	 */
	Toolbox::int32 GetDisplayHeight() const noexcept
	{
		return m_Ui.GetHeight();
	}

protected:
	/**
	 * Worldのイベントを有効化し、配置・操作を置き、文字を読み込む。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 配置・プレイヤー・状態を描く（2画面なら左右に一回ずつ）。
	 * @param Render 描画の実行環境。
	 */
	void OnDraw(FRenderContext& Render) const override;

public:
	/**
	 * 操作と観察で共有する装置群。Sceneは生成と寿命だけを取りまとめる。
	 */
	FMechanismCourse2D& GetMechanismCourse() noexcept
	{
		return m_MechanismCourse;
	}
	/**
	 * 描画専用の装置参照。
	 */
	const FMechanismCourse2D& GetMechanismCourse() const noexcept
	{
		return m_MechanismCourse;
	}

private:
	/**
	 * データコースの準備は独立したObjectが担当し、従来コースを準備失敗で終了しない。
	 */
	TObjectHandle<DContentCoursePortal> m_ContentPortal;
	FInteractionRules& GetRules() noexcept override
	{
		return m_Rules;
	}
	Toolbox::TOptional<FBodyId2D> GetPlayerBody() const noexcept override;
	void RespawnPlayer() override
	{
		Respawn();
	}
	DPlayer2D* GetPlayerObject() const noexcept override
	{
		return GetPlayer();
	}
	/**
	 * 一つの画面を描く。
	 * @param Render 描画の実行環境。
	 * @param Side 0（左・全画面）または1（右）。
	 */
	void DrawView_Internal(FRenderContext& Render, Toolbox::int32 Side) const;
	/**
	 * ゲームの規則と状態。
	 */
	FInteractionRules m_Rules;
	/**
	 * 既存コースに置いた距離拘束の仕掛け。
	 */
	FJointCourse2D m_JointCourse;
	/**
	 * 子の装置を構成する非所有の窓口。
	 */
	FMechanismCourse2D m_MechanismCourse;
	/**
	 * Scene専用の設定画面。物理と独立した表示・入力処理。
	 */
	mutable FInteractionUi m_Ui;
	/**
	 * 置いたオブジェクト。
	 */
	TInteractionCourse<FInteraction2D> m_Course;
	/**
	 * 状態表示の文字。
	 */
	FFont m_Font;
	/**
	 * 2画面表示か。
	 */
	bool m_bSplit = false;
	/**
	 * キャラクターの遊び方。
	 */
	FInteractionMode m_Mode;
};
} // namespace Dxf::GameplaySample
#endif
