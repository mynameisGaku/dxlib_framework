// SPDX-License-Identifier: NOASSERTION
#include "Support/TestFs.h"
#include <stdio.h>
namespace Test
{
// 一時領域に検証用の作業場所を用意する。存在すれば作り直す。
// @param Name 検証ごとの作業フォルダー名。
Toolbox::FPath PrepareScratchDirectory(const char* Name)
{
	// 検証用の作業場所。
	Toolbox::FPath Scratch = Toolbox::TemporaryDirectory() / "DxfAssetRootTest" / Name;
	// 削除時のエラーコード。
	Toolbox::int32 Error = 0;
	Toolbox::RemoveTree(Scratch, Error);
	// 既存ならfalseを返すだけなので結果を見ない。
	Toolbox::CreateDirectory(Toolbox::TemporaryDirectory() / "DxfAssetRootTest");
	if (!Toolbox::CreateDirectory(Scratch))
	{
		throw Toolbox::FException("Cannot prepare scratch directory");
	}
	return Scratch;
}
// 内容をそのまま書き込む。親フォルダーは存在していること。
// @param Path 書き込む先のパス。
// @param Content 書き込むバイト列。
void WriteScratchFile(const Toolbox::FPath& Path, const Toolbox::FString& Content)
{
	// 書き込み先のファイル。
	FILE* File = fopen(Path.ToUtf8().CStr(), "wb");
	if (File == nullptr)
	{
		throw Toolbox::FException("Cannot open scratch file for writing");
	}
	// 書き込んだバイト数。
	const size_t Written = Content.IsEmpty() ? 0 : fwrite(Content.Data(), 1, Content.Size(), File);
	// 正常に閉じたか。
	const bool Closed = fclose(File) == 0;
	if (Written != Content.Size() || !Closed)
	{
		throw Toolbox::FException("Cannot write scratch file");
	}
}
} // namespace Test
