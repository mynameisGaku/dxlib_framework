// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_DEFINITION_STAMP_H
#define DXF_CONTENT_DEFINITION_STAMP_H
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 準備に実際に使った一つの定義ファイルの記録。資源ファイルの内容までは識別しない。
 */
struct FContentDefinitionStamp
{
	/**
	 * ProjectRootを除いた正規化済み論理パス。再配置しても変わらない。
	 */
	Toolbox::FString Path;
	/**
	 * BOMを含む読込済み生バイトの長さ。
	 */
	Toolbox::uint64 ByteCount = 0;
	/**
	 * 読込済み生バイトのSHA-256。後からディスクを読み直さない。
	 */
	Toolbox::FString Sha256;
};
} // namespace Dxf
#endif
