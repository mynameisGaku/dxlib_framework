#pragma once
#include "Dxf/CharacterMovementComponent2D.h"
#include "Dxf/CharacterMovementComponent3D.h"
#include "Dxf/Font.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/RenderView3D.h"
#include "Toolbox/Array.h"
namespace Dxf::GameplaySample
{
/**
 * 固定の地形の箱（2Dは回転矩形、3DはZ方向に厚みを持つOBB）。距離はメートル。
 */
struct FLevelBox
{
	/**
	 * XY平面での中心。
	 */
	Toolbox::FVector2 Center;
	/**
	 * XY平面での半幅。
	 */
	Toolbox::FVector2 Half;
	/**
	 * Z軸回りの角度（ラジアン、反時計回り）。
	 */
	Toolbox::f32 Angle = 0;
	/**
	 * 3DでのZ方向の中心。
	 */
	Toolbox::f32 CenterZ = 0;
	/**
	 * 3DでのZ方向の半幅。
	 */
	Toolbox::f32 HalfZ = 4;
	/**
	 * 3Dだけに置く箱か（二つの壁の角を作る奥の壁）。
	 */
	bool bOnly3D = false;
};
/**
 * 地形の箱の数。
 */
constexpr Toolbox::size_t LevelBoxCount = 9;
/**
 * 地形（床・低い段差・高い段差・低い天井・30度の坂・台・60度の急坂・壁・3Dの奥の壁）。
 */
const Toolbox::TArray<FLevelBox, LevelBoxCount>& GetLevelBoxes() noexcept;
/**
 * プレイヤーの開始位置のX（Yは床の上に接触余裕を足した0.52）。
 */
constexpr Toolbox::f32 StartX = 0;
/**
 * プレイヤーの開始位置のY。
 */
constexpr Toolbox::f32 StartY = 0.52f;

/**
 * 2Dのプレイヤー。入力を移動要求へ変え、DCharacterMovement2DComponentへ渡すだけ（物理計算はComponentが行う）。
 */
class DPlayer2D final : public DGameObject
{
public:
	/**
	 * 移動Componentを返す。初期化後だけ有効。
	 */
	DCharacterMovement2DComponent& GetCharacter() const;

protected:
	/**
	 * 移動Componentを追加する。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * A/Dで左右、Spaceでジャンプを要求する。
	 * @param Context フレーム更新の実行環境。
	 */
	void OnTick(const FTickContext& Context) override;

private:
	/**
	 * 移動Component。
	 */
	TObjectHandle<DCharacterMovement2DComponent> m_Character;
};
/**
 * 3Dのプレイヤー。A/DでX、W/SでZ、Spaceでジャンプを要求する。
 */
class DPlayer3D final : public DGameObject
{
public:
	/**
	 * 移動Componentを返す。初期化後だけ有効。
	 */
	DCharacterMovement3DComponent& GetCharacter() const;

protected:
	/**
	 * 移動Componentを追加する。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 入力を移動要求へ変える。
	 * @param Context フレーム更新の実行環境。
	 */
	void OnTick(const FTickContext& Context) override;

private:
	/**
	 * 移動Component。
	 */
	TObjectHandle<DCharacterMovement3DComponent> m_Character;
};

/**
 * 2Dのサンプル。地形とプレイヤーを置き、[Tab]で3Dへ切り替える。
 */
class DCharacterSample2DScene final : public DPhysicsScene2D
{
public:
	/**
	 * プレイヤーを返す。
	 */
	FORCEINLINE TObjectHandle<DPlayer2D> GetPlayer() const noexcept
	{
		return m_Player;
	}
	/**
	 * ワールド座標の点を画面の座標へ変える（描画と同じ変換）。
	 * @param X ワールドのX。
	 * @param Y ワールドのY。
	 */
	static FVector2 ToScreen(Toolbox::f32 X, Toolbox::f32 Y) noexcept;

protected:
	/**
	 * 地形・プレイヤー・操作を置き、文字を読み込む。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 地形・プレイヤー・状態を描く。
	 * @param Render 描画の実行環境。
	 */
	void OnDraw(FRenderContext& Render) const override;

private:
	/**
	 * プレイヤー。
	 */
	TObjectHandle<DPlayer2D> m_Player;
	/**
	 * 状態表示の文字。
	 */
	FFont m_Font;
};
/**
 * 3Dのサンプル。同じ地形をZ方向に厚みを持たせて置き、奥の壁で二つの面の角を作る。[V]で2画面、[Tab]で2Dへ切り替える。
 */
class DCharacterSample3DScene final : public DPhysicsScene3D
{
public:
	/**
	 * プレイヤーを返す。
	 */
	FORCEINLINE TObjectHandle<DPlayer3D> GetPlayer() const noexcept
	{
		return m_Player;
	}
	/**
	 * 2画面表示を切り替える（更新・物理Step・移動の回数は描画の数に依存しない）。
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
	 * 指定した画面の、現在のプレイヤーを追うビューを返す。
	 * @param Side 0は主画面、1は2画面表示の右側。
	 */
	FRenderView3D GetView(Toolbox::int32 Side) const;

protected:
	/**
	 * 地形・プレイヤー・操作を置き、文字を読み込む。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 地形・プレイヤー・状態を描く。
	 * @param Render 描画の実行環境。
	 */
	void OnDraw(FRenderContext& Render) const override;

private:
	/**
	 * プレイヤー。
	 */
	TObjectHandle<DPlayer3D> m_Player;
	/**
	 * 状態表示の文字。
	 */
	FFont m_Font;
	/**
	 * 2画面表示か。
	 */
	bool m_bSplit = false;
};
} // namespace Dxf::GameplaySample
