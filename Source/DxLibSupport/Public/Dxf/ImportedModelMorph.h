// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_IMPORTED_MODEL_MORPH_H
#define DXF_IMPORTED_MODEL_MORPH_H
#include "Toolbox/Vector3.h"
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 1メッシュの1モーフ。差分は変換後の基本頂点と同じ順序・座標系。
 */
struct FImportedModelMorph
{
	/**
	 * 利用者に表示するノード名とチャンネル名。
	 */
	Toolbox::FString Name;
	/**
	 * 対象となる.xのメッシュフレーム名。
	 */
	Toolbox::FString FrameName;
	/**
	 * ファイルに記録された初期の変形量。
	 */
	Toolbox::f32 DefaultWeight = 0;
	/**
	 * 基本位置に足す差分。
	 */
	Toolbox::TVector<Toolbox::FVector3> PositionOffsets;
	/**
	 * 基本法線に足す差分。空なら法線は基本形状のまま。
	 */
	Toolbox::TVector<Toolbox::FVector3> NormalOffsets;
};
} // namespace Dxf
#endif
