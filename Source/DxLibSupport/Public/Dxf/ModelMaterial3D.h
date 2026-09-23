// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MODEL_MATERIAL_3D_H
#define DXF_MODEL_MATERIAL_3D_H
#include "Dxf/MathTypes.h"
namespace Dxf
{
/**
 * モデル全体に掛ける基本材質。ファイル内の各材質・テクスチャを保つ。
 */
struct FModelMaterial3D
{
	/**
	 * 拡散色へ掛ける色。現在は不透明のみ（Aは255）。
	 */
	FColor Tint{255, 255, 255, 255};
	/**
	 * ビューの指向性光と環境光をDxLibのモデル照明で評価するか。
	 * falseなら従来どおり照明なし。CPU基本形状の照明とは別に評価する。
	 */
	bool bLit = false;
	/**
	 * Direct3D11の金属度・粗さによるPBR照明を使う。照明ありとして扱う。
	 */
	bool bPbr = false;
	/**
	 * 金属度の上書き。-1はファイル値、指定値は0〜1。
	 */
	Toolbox::f32 Metallic = -1;
	/**
	 * 粗さの上書き。-1はファイル値、指定値は0〜1。描画時の下限は0.045。
	 */
	Toolbox::f32 Roughness = -1;
	/**
	 * PBRベーステクスチャのUV。-1はファイル値、0または1は上書き。
	 */
	Toolbox::int32 BaseColorUv = -1;
};
} // namespace Dxf
#endif
