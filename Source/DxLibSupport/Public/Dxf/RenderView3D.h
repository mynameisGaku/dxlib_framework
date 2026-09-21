// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_RENDER_VIEW_3D_H
#define DXF_RENDER_VIEW_3D_H
#include "Dxf/MathTypes.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 面の表示方式。法線・深度・GPU負荷の可視化とは区別する。
 */
enum class ESurfaceMode3D
{
	Solid, Wireframe, SolidWithEdges
};
/**
 * 表示だけの照明上書き。全消灯と照明計算の省略を区別する。
 */
enum class ELightingMode3D
{
	Normal, Unlit, LightsOff
};
/**
 * 深度検査と書き込みの指定。補助線はTestOnlyまたはAlwaysで重ねられる。
 */
enum class EDepthMode3D
{
	TestAndWrite, TestOnly, Always
};
/**
 * 一つのビューで使う表示上書き。元の形状・材質・ライトを変更しない。
 */
struct FDebugViewSettings3D
{
	/**
	 * 面・線の切り替え。
	 */
	ESurfaceMode3D Surface = ESurfaceMode3D::Solid;
	/**
	 * 照明の上書き。
	 */
	ELightingMode3D Lighting = ELightingMode3D::Normal;
	/**
	 * 重ねるメッシュ辺の色。
	 */
	FColor EdgeColor{32, 255, 96, 255};
};
/**
 * 全描画先を使う透視投影ビュー。位置はワールド単位、角度はラジアン。
 * Idは調査対象の識別値。設定は命令受付時に複写され、後の変更は遡及しない。
 * 初期実装の照明はCPUで評価するフラットな指向性光一灯と環境光。
 */
struct FRenderView3D
{
	/**
	 * ビューの識別値。
	 */
	Toolbox::uint64 Id = 0;
	/**
	 * カメラのワールド位置。
	 */
	Toolbox::FVector3 Eye{0, 0, -5};
	/**
	 * 注視点。Eyeと同じ位置は不可。
	 */
	Toolbox::FVector3 Target{0, 0, 0};
	/**
	 * 上方向。視線と平行は不可。
	 */
	Toolbox::FVector3 Up{0, 1, 0};
	/**
	 * 垂直画角。0より大きくπより小さい値。
	 */
	Toolbox::f32 VerticalFov = 1.0471975512f;
	/**
	 * 正の近接面距離。
	 */
	Toolbox::f32 NearPlane = 0.01f;
	/**
	 * NearPlaneより遠い遠方面距離。
	 */
	Toolbox::f32 FarPlane = 10000.0f;
	/**
	 * 光が進む向き。
	 */
	Toolbox::FVector3 LightDirection{0, -1, 1};
	/**
	 * 指向性ライトの色。
	 */
	FColor LightColor{220, 220, 220, 255};
	/**
	 * 環境光。LightsOffではこれも無効にする。
	 */
	FColor AmbientColor{32, 32, 32, 255};
	/**
	 * 指向性ライトを適用するか。
	 */
	bool bLightEnabled = true;
	/**
	 * ビュー固有の調査用上書き。
	 */
	FDebugViewSettings3D Debug;
};
/**
 * 有限値・カメラ基底・列挙値・照明を検証する。
 * @param View 検証する描画ビュー。
 */
bool IsValidRenderView3D(const FRenderView3D& View) noexcept;
}
#endif
