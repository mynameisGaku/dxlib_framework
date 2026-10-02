// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_FILELOCK_H
#define TOOLBOX_FILELOCK_H
#include "Toolbox/Platform.h"
namespace Toolbox
{
/**
 * プロセス終了でもOSが解放する非待機の排他。lockファイル自体は残す。
 */
class FFileLock
{
public:
	/**
	 * @param Path 同じ対象のWriterが共有する絶対lockパス。競合はBusy例外。
	 */
	explicit FFileLock(const FPath& Path);
	/**
	 * 確保やファイル削除を伴わず、排他を解放する。
	 */
	~FFileLock() noexcept;
	/**
	 * OS排他の複製を禁止する。
	 */
	FFileLock(const FFileLock&) = delete;
	/**
	 * OS排他の複製を禁止する。
	 */
	FFileLock& operator=(const FFileLock&) = delete;

private:
	/**
	 * 実装内でだけOSハンドルへ変換する不透明値。
	 */
	int64 m_Handle = -1;
};
} // namespace Toolbox
#endif
