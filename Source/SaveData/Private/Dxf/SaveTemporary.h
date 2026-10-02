// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVE_TEMPORARY_PRIVATE_H
#define DXF_SAVE_TEMPORARY_PRIVATE_H
#include "Dxf/SaveError.h"
#include "Toolbox/String.h"
namespace Dxf::SavePrivate
{
/**
 * 新規作成済みの一時ファイルだけへ全量を書き、公開前に検証する。
 * @param Io この呼出しが所有する一時ファイルの操作。失敗時はClose後にRemoveする。
 * @param Bytes 公開予定の不変バイト。正常な確定面は操作しない。
 */
template <typename TIo>
void WriteTemporary(TIo& Io, Toolbox::FStringView Bytes)
{
	try
	{
		Io.Write(Bytes);
		Io.Flush();
		Io.Close();
		if (!Io.Matches(Bytes))
		{
			throw FSaveError(ESaveError::Corrupt, "Save temporary read-back mismatch");
		}
	}
	catch (...)
	{
		// 終了の失敗で元の工程の診断を消さない。
		try
		{
			Io.Close();
		}
		catch (...)
		{
		}
		Io.Remove();
		throw;
	}
}
} // namespace Dxf::SavePrivate
#endif
