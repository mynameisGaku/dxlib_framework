// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_IMPORTED_MODEL_CLIP_H
#define DXF_IMPORTED_MODEL_CLIP_H
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 変換後のアニメーションクリップを管理する型。
 */
struct FImportedModelClip
{
	/**
	 * FBX上のクリップ名（UTF-8）。
	 */
	Toolbox::FString Name;
	/**
	 * クリップの長さ（秒）。
	 */
	Toolbox::f64 DurationSeconds = 0.0;
	/**
	 * モーフ番号ごとの等間隔標本。両端を含む。
	 */
	Toolbox::TVector<Toolbox::TVector<Toolbox::f32>> MorphWeights;
};
} // namespace Dxf
#endif
