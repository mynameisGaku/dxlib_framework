// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SCENE_CONTENT_DIAGNOSTIC_H
#define DXF_SCENE_CONTENT_DIAGNOSTIC_H
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 読み取りか検証に失敗した場所。実行中のIDや所有ポインタを持たない。
 */
struct FSceneContentDiagnostic
{
	/**
	 * ProjectRootからの定義パス。
	 */
	Toolbox::FString Path;
	/**
	 * 資源または定義の論理キー。
	 */
	Toolbox::FString Key;
	/**
	 * 値または参照の経路。
	 */
	Toolbox::FString Location;
	/**
	 * 原因の説明。
	 */
	Toolbox::FString Reason;
	/**
	 * 1始まりの行。元の位置が分からない実行時診断は0。
	 */
	Toolbox::uint32 Line = 0;
	/**
	 * 1始まりのバイト列位置。元の位置が分からない実行時診断は0。
	 */
	Toolbox::uint32 Column = 0;
};
} // namespace Dxf
#endif
