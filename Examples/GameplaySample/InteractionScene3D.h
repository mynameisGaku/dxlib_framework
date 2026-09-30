// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_SCENE_3D_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_SCENE_3D_H
#include "InteractionCourse.h"
#include "InteractionUi.h"
#include "JointCourse3D.h"
#include "Dxf/Font.h"
#include "Dxf/RenderView3D.h"
namespace Dxf::GameplaySample
{
/**
 * 3Dの接触・Trigger・動く床のサンプル。2Dと同じ配置をZ=0の平面に奥行き付きで置く（回転する床は鉛直軸回りに回る）。
 * [Tab]で2Dへ、[I]でキャラクター移動のサンプルへ切り替える。[R]で最後のチェックポイントへ戻る、[V]で2画面、[P]で一時停止。
 */
class DInteraction3DScene final : public DPhysicsScene3D, private IInteractionHost<FInteraction3D>
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
	FORCEINLINE const TInteractionCourse<FInteraction3D>& GetCourse() const noexcept
	{
		return m_Course;
	}
	/**
	 * プレイヤー（初期化前・破棄後はnullptr）。
	 */
	FORCEINLINE DPlayer3D* GetPlayer() const noexcept
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
	 * 描画のView（2画面なら左右）。
	 * @param Side 0（左・1画面）または1（右）。
	 */
	FRenderView3D GetView(Toolbox::int32 Side) const;
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
	FJointCourse3D& GetJointCourse() noexcept
	{
		return m_JointCourse;
	}
	/**
	 * 描画と観察用の仕掛け参照。
	 */
	const FJointCourse3D& GetJointCourse() const noexcept
	{
		return m_JointCourse;
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

private:
	FInteractionRules& GetRules() noexcept override
	{
		return m_Rules;
	}
	Toolbox::TOptional<FBodyId3D> GetPlayerBody() const noexcept override;
	void RespawnPlayer() override
	{
		Respawn();
	}
	DPlayer3D* GetPlayerObject() const noexcept override
	{
		return GetPlayer();
	}
	/**
	 * ゲームの規則と状態。
	 */
	FInteractionRules m_Rules;
	/**
	 * 既存コースに置いた距離拘束の仕掛け。
	 */
	FJointCourse3D m_JointCourse;
	/**
	 * Scene専用の設定画面。物理と独立した表示・入力処理。
	 */
	mutable FInteractionUi m_Ui;
	/**
	 * 置いたオブジェクト。
	 */
	TInteractionCourse<FInteraction3D> m_Course;
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
