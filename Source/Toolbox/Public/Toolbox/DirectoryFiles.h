// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_DIRECTORYFILES_H
#define TOOLBOX_DIRECTORYFILES_H
#include "Toolbox/Platform.h"
namespace Toolbox
{
/**
 * 通常ファイルの名前だけを返す。サブフォルダーは辿らず、切り捨て成功にしない。
 * @param Directory 読み取る明示領域。
 * @param Suffix 名前末尾のバイト列。空なら全通常ファイル。
 * @param MaxCount 返却件数の有限上限。
 */
TVector<FString> DirectoryFiles(const FPath& Directory, FStringView Suffix, size_t MaxCount);
} // namespace Toolbox
#endif
