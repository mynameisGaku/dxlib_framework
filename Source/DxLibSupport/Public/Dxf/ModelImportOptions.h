// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MODEL_IMPORT_OPTIONS_H
#define DXF_MODEL_IMPORT_OPTIONS_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * FBXの読み込み設定を管理する型。
 */
struct FModelImportOptions
{
	/**
	 * 1ワールド単位を何メートルとして扱うか。0ならファイルの長さの単位をそのまま使う。
	 */
	Toolbox::f64 TargetUnitMeters = 0.0;
	/**
	 * アニメーションをサンプリングする1秒あたりのキー数。
	 */
	Toolbox::uint32 SamplesPerSecond = 30;
};
} // namespace Dxf
#endif
