// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MODEL_MORPH_INFO_H
#define DXF_MODEL_MORPH_INFO_H
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 読み込んだモデルに含まれる1つのモーフの情報。
 */
struct FModelMorphInfo
{
	/**
	 * ノード名とチャンネル名。
	 */
	Toolbox::FString Name;
	/**
	 * クリップを再生していない場合の変形量。
	 */
	Toolbox::f32 DefaultWeight = 0;
};
} // namespace Dxf
#endif
