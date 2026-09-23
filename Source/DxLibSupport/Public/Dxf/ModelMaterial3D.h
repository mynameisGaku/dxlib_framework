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
};
} // namespace Dxf
#endif
