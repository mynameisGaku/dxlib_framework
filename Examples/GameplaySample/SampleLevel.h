// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_LEVEL_H
#define DXF_GAMEPLAY_SAMPLE_LEVEL_H
#include "Dxf/GameObject.h"
#include "Dxf/MathTypes.h"
#include "Toolbox/Array.h"
#include "Toolbox/CollisionShapes.h"
#include "Toolbox/Vector2.h"
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
 * プレイヤーの色（画素の確認に使う）。
 */
constexpr FColor PlayerColor = {255, 200, 40, 255};
/**
 * 歩行キャラクターの色。
 */
constexpr FColor WalkerColor = {70, 200, 230, 255};
/**
 * 箱の色（番号ごと）。プレイヤー・歩行キャラクターの色とは重ならない。
 * @param Index 箱の番号。
 */
FColor LevelBoxColor(Toolbox::size_t Index) noexcept;
/**
 * 箱の四隅（XY平面、反時計回り）を求める。
 * @param Box 箱。
 * @param Out 四隅の出力先。
 */
void LevelBoxCorners(const FLevelBox& Box, Toolbox::FVector2 (&Out)[4]) noexcept;
/**
 * 3Dの箱の形状（Z軸回りに回したOBB）を返す。
 * @param Box 箱。
 */
Toolbox::FOBB LevelBoxShape3D(const FLevelBox& Box) noexcept;

/**
 * 2Dの地形。一つのStaticのBodyに、箱ごとのColliderを付ける。
 */
class DLevel2D final : public DGameObject
{
protected:
	/**
	 * Bodyと箱のColliderを追加する。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
};
/**
 * 3Dの地形。
 */
class DLevel3D final : public DGameObject
{
protected:
	/**
	 * Bodyと箱のColliderを追加する。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
};
} // namespace Dxf::GameplaySample
#endif
