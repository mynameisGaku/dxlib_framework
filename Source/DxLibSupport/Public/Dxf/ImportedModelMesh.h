// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_IMPORTED_MODEL_MESH_H
#define DXF_IMPORTED_MODEL_MESH_H
#include "Dxf/MathTypes.h"
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * .xに標準で渡せないメッシュ属性。DxLibのモデル確定前に追加する。
 */
struct FImportedModelMesh
{
	/**
	 * .x上のメッシュフレーム名。
	 */
	Toolbox::FString FrameName;
	/**
	 * 2組目以降のUV。各配列は.xの頂点順で、VはDirectX側へ変換済み。
	 */
	Toolbox::TVector<Toolbox::TVector<FVector2>> AdditionalUvs;
};
} // namespace Dxf
#endif
