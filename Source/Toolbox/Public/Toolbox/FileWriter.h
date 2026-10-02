// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_FILEWRITER_H
#define TOOLBOX_FILEWRITER_H
#include "Toolbox/Platform.h"
namespace Toolbox
{
/**
 * 新規ファイルだけを所有して全量書き込む。既存ファイルはtruncateしない。
 */
class FFileWriter
{
public:
	/**
	 * @param Path 新規作成先。既存ならCreate例外。
	 */
	explicit FFileWriter(const FPath& Path);
	/**
	 * 残ったハンドルだけを閉じる。成功確認には明示Closeを使う。
	 */
	~FFileWriter() noexcept;
	/**
	 * ファイル所有権の複製を禁止する。
	 */
	FFileWriter(const FFileWriter&) = delete;
	/**
	 * ファイル所有権の複製を禁止する。
	 */
	FFileWriter& operator=(const FFileWriter&) = delete;
	/**
	 * @param Bytes 長さ付きの全入力。短いwriteは続け、失敗はWrite例外。
	 */
	void Write(FStringView Bytes);
	/**
	 * OSのflush結果を検査する。全FS・電源断の保証ではない。
	 */
	void Flush();
	/**
	 * 明示的に閉じ、Close失敗を通知する。
	 */
	void Close();

private:
	/**
	 * 実装だけがOS値へ変換する所有ハンドル。
	 */
	int64 m_Handle = -1;
};
} // namespace Toolbox
#endif
