// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_FILE_WRITE_LOOP_PRIVATE_H
#define TOOLBOX_FILE_WRITE_LOOP_PRIVATE_H
#include "Toolbox/FileError.h"
#include "Toolbox/String.h"
namespace Toolbox::FilePrivate
{
/**
 * 実際に書けた長さだけ進め、短いwriteを全量成功と扱わない。
 * @param Bytes 全量を書き込む長さ付き入力。
 * @param Write 一回のOS操作。失敗は例外で伝え、成功時の実バイト数を返す。
 */
template <typename TWrite>
void WriteAll(FStringView Bytes, TWrite Write)
{
	// 書き込み済みの位置。
	size_t Offset = 0;
	while (Offset < Bytes.Size())
	{
		// OSとCランタイム双方で扱える有限の一回分。
		const uint32 Chunk = static_cast<uint32>(Min(Bytes.Size() - Offset, size_t(1024 * 1024)));
		const uint32 Written = Write(Bytes.Data() + Offset, Chunk);
		if (Written == 0 || Written > Chunk)
		{
			throw FFileError(EFileOperation::Write, 0, "Invalid file write length");
		}
		Offset += Written;
	}
}
} // namespace Toolbox::FilePrivate
#endif
