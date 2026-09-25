// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_CHARACTER_SAMPLE_2D_SCENE_H
#define DXF_GAMEPLAY_SAMPLE_CHARACTER_SAMPLE_2D_SCENE_H
#include "SampleCharacters.h"
#include "Dxf/Font.h"
#include "Dxf/MathTypes.h"
#include "Dxf/PhysicsScene2D.h"
#include "Toolbox/Array.h"
namespace Dxf::GameplaySample
{
/**
 * 2Dのサンプル。地形とプレイヤーを置き、[Tab]で3Dへ切り替える。[N]で歩行キャラクターを追加、[M]で最後の一体を破棄、
 * [V]で2画面（左右に同じ地形を縮小して描く）を切り替える。更新・物理Step・移動の回数は描画の数に依存しない。
 */
class DCharacterSample2DScene final : public DPhysicsScene2D
{
public:
	/**
	 * 同時に置ける歩行キャラクターの数。
	 */
	static constexpr Toolbox::int32 MaxWalkers = 8;
	/**
	 * プレイヤーを返す。
	 */
	FORCEINLINE TObjectHandle<DPlayer2D> GetPlayer() const noexcept
	{
		return m_Player;
	}
	/**
	 * 1画面のときのワールド座標の点を画面の座標へ変える（描画と同じ変換）。
	 * @param X ワールドのX。
	 * @param Y ワールドのY。
	 */
	static FVector2 ToScreen(Toolbox::f32 X, Toolbox::f32 Y) noexcept;
	/**
	 * 2画面表示を切り替える。
	 */
	FORCEINLINE void ToggleSplit() noexcept
	{
		m_bSplit = !m_bSplit;
	}
	/**
	 * 2画面表示かを返す。
	 */
	FORCEINLINE bool IsSplit() const noexcept
	{
		return m_bSplit;
	}
	/**
	 * 歩行キャラクターを一体追加する（上限なら何もしない）。追加したらtrue。
	 */
	bool SpawnWalker();
	/**
	 * 最後に追加した歩行キャラクターを破棄する（いなければ何もしない）。破棄したらtrue。
	 */
	bool DestroyWalker() noexcept;
	/**
	 * 歩行キャラクターの数を返す。
	 */
	FORCEINLINE Toolbox::int32 GetWalkerCount() const noexcept
	{
		return m_WalkerCount;
	}
	/**
	 * 歩行キャラクターを返す。
	 * @param Index 0～GetWalkerCount()-1。
	 */
	FORCEINLINE TObjectHandle<DWalker2D> GetWalker(Toolbox::int32 Index) const noexcept
	{
		return m_Walkers[static_cast<Toolbox::size_t>(Index)];
	}
	/**
	 * 一時停止を除いて進めたフレーム時間の合計（アニメーション時間、秒）を返す。
	 */
	FORCEINLINE Toolbox::f64 GetAnimationSeconds() const noexcept
	{
		return m_AnimationSeconds;
	}
	/**
	 * アニメーション時間を進める（操作のオブジェクトが一時停止以外のフレームで呼ぶ）。
	 * @param Seconds フレーム時間。
	 */
	FORCEINLINE void AdvanceAnimation(Toolbox::f64 Seconds) noexcept
	{
		m_AnimationSeconds += Seconds;
	}

protected:
	/**
	 * 地形・プレイヤー・操作を置き、文字を読み込む。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 地形・キャラクター・状態を描く（2画面なら左右に一回ずつ）。
	 * @param Render 描画の実行環境。
	 */
	void OnDraw(FRenderContext& Render) const override;

private:
	/**
	 * 一つの画面を描く。
	 * @param Render 描画の実行環境。
	 * @param OriginX 画面の左端のX（ピクセル）。
	 * @param Scale 1メートルあたりのピクセル数。
	 */
	void DrawView_Internal(FRenderContext& Render, Toolbox::f32 OriginX, Toolbox::f32 Scale) const;
	/**
	 * プレイヤー。
	 */
	TObjectHandle<DPlayer2D> m_Player;
	/**
	 * 歩行キャラクター（先頭からGetWalkerCount()体）。
	 */
	Toolbox::TArray<TObjectHandle<DWalker2D>, MaxWalkers> m_Walkers;
	/**
	 * 歩行キャラクターの数。
	 */
	Toolbox::int32 m_WalkerCount = 0;
	/**
	 * これまでに追加した歩行キャラクターの数（開始位置をずらす）。
	 */
	Toolbox::int32 m_WalkersSpawned = 0;
	/**
	 * 状態表示の文字。
	 */
	FFont m_Font;
	/**
	 * 2画面表示か。
	 */
	bool m_bSplit = false;
	/**
	 * アニメーション時間（秒）。
	 */
	Toolbox::f64 m_AnimationSeconds = 0;
};
} // namespace Dxf::GameplaySample
#endif
