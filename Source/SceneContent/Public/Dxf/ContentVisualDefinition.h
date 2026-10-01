// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_VISUAL_DEFINITION_H
#define DXF_CONTENT_VISUAL_DEFINITION_H
#include "Dxf/MathTypes.h"
#include "Dxf/ModelMaterial3D.h"
#include "Toolbox/Vector3.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 表示の種類。Noneなら表示Componentを作らない。
 */
enum class EContentVisualKind : Toolbox::uint8
{
	/**
	 * 表示なし。
	 */
	None,
	/**
	 * 箱。
	 */
	Box,
	/**
	 * 円か球。
	 */
	Sphere,
	/**
	 * 2D画像。
	 */
	Texture,
	/**
	 * 3Dモデル。
	 */
	Model
};
/**
 * 物理とは独立した表示条件。資源indexは検証後の表を指す。
 */
struct FContentVisualDefinition
{
	/**
	 * 表示の種類。
	 */
	EContentVisualKind Kind = EContentVisualKind::None;
	/**
	 * 箱の半径。2DではXYだけ使用する。
	 */
	Toolbox::FVector3 HalfExtents{0.5f, 0.5f, 0.5f};
	/**
	 * 円か球の半径。
	 */
	Toolbox::f32 Radius = 0.5f;
	/**
	 * 表示の色。
	 */
	FColor Color;
	/**
	 * 資源index。未指定は-1。
	 */
	Toolbox::int32 Asset = -1;
	/**
	 * 任意ラベルのFont。未指定はラベルなし。
	 */
	Toolbox::int32 Font = -1;
	/**
	 * Worldラベル。
	 */
	Toolbox::FString Label;
	/**
	 * 安定した描画順。
	 */
	Toolbox::int32 Layer = 0;
	/**
	 * モデル個体だけの拡大率。物理には反映しない。
	 */
	Toolbox::FVector3 ModelScale{1, 1, 1};
	/**
	 * 初期再生クリップ。未指定は-1。
	 */
	Toolbox::int32 Animation = -1;
	/**
	 * 個体の基本材質指定。
	 */
	FModelMaterial3D Material;
};
} // namespace Dxf
#endif
