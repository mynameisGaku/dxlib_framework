#pragma once
#include "Toolbox/Platform.h"
namespace Test
{
/**
 * 一時領域に検証用の作業場所を用意する。存在すれば作り直す。
 * @param Name 検証ごとの作業フォルダー名。
 */
Toolbox::FPath PrepareScratchDirectory(const char* Name);
/**
 * 内容をそのまま書き込む。親フォルダーは存在していること。
 * @param Path 書き込む先のパス。
 * @param Content 書き込むバイト列。
 */
void WriteScratchFile(const Toolbox::FPath& Path, const Toolbox::FString& Content);
} // namespace Test
