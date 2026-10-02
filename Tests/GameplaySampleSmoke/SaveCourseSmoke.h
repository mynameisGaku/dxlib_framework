// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAVE_COURSE_SMOKE_H
#define DXF_GAMEPLAY_SAVE_COURSE_SMOKE_H
#include "SmokeSupport.h"
namespace Dxf::GameplaySmoke
{
/**
 * 別プロセスの保存と再開を、実Application・実DxLibで独立して確認する。
 * @param Backends このプロセスのNative窓口。
 * @param Root 再配置した定義と資源の起点。
 * @param Output 今回専用の画像出力先。
 * @param SaveRoot この試験の保存領域。
 * @param Dimension 明示した2または3。
 * @param bRead trueは前プロセスの保存を読込、falseは新規進行を保存。
 */
void RunSaveCourseSmoke(FDxLibBackends& Backends, const char* Root, const Toolbox::FPath& Output, const Toolbox::FPath& SaveRoot, Toolbox::uint32 Dimension, bool bRead);
} // namespace Dxf::GameplaySmoke
#endif
