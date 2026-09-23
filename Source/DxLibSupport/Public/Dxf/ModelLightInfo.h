// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MODEL_LIGHT_INFO_H
#define DXF_MODEL_LIGHT_INFO_H
#include "Dxf/RenderView3D.h"
#include "Dxf/Result.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * FBXの静止ライト。影は生成せず、GPUモデル用の光として明示適用する。
 */
struct FModelLightInfo
{
	/**
	 * 接続先ノードの名前。
	 */
	Toolbox::FString Name;
	/**
	 * 光源の形。
	 */
	EModelLightType Type = EModelLightType::Directional;
	/**
	 * モデル読込時の座標系・単位での位置。
	 */
	Toolbox::FVector3 Position;
	/**
	 * 光が進む方向。
	 */
	Toolbox::FVector3 Direction{0, -1, 0};
	/**
	 * 強度を含めたRGB。0以上で1を超えてよい。
	 */
	Toolbox::FVector3 Radiance{1, 1, 1};
	/**
	 * 円錐の内角（ラジアン、全角）。
	 */
	Toolbox::f32 InnerAngle = 0;
	/**
	 * 円錐の外角（ラジアン、全角）。
	 */
	Toolbox::f32 OuterAngle = 1;
	/**
	 * ファイルの発光有効設定。
	 */
	bool bEnabled = true;
	/**
	 * モデル用ライトを適用する。無効値ならビューを変更しない。
	 * @param View 適用先。CPU基本形状の指向性光は変更しない。
	 * @param Range 点・スポット光の到達範囲（ワールド単位）。
	 * @param Attenuation 定数・距離・距離二乗の減衰係数。FBXの光量単位からの自動換算はしない。
	 */
	TResult<void> ApplyTo(FRenderView3D& View, Toolbox::f32 Range = 10000,
	                      Toolbox::FVector3 Attenuation = {1, 0, 0}) const;
};
} // namespace Dxf
#endif
