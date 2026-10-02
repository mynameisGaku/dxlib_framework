// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_FILEPUBLICATION_H
#define TOOLBOX_FILEPUBLICATION_H
#include "Toolbox/Platform.h"
namespace Toolbox
{
/**
 * 同じ保存領域内の一時ファイルを指定面へ公開する。失敗後の名前状態は呼出し側で検証する。
 * @param Temporary 自分が新規作成した一時ファイル。
 * @param Destination 最新正常面とは別の公開先。
 */
void PublishFile(const FPath& Temporary, const FPath& Destination);
/**
 * 自分が新規作成した通常ファイルだけを後始末する。ディレクトリは削除しない。
 * @param Path 所有を確認済みの一時ファイル。
 */
bool RemoveOwnedFile(const FPath& Path) noexcept;
/**
 * OSのユーザー領域を取得する。失敗時にCWDへ切り替えない。
 */
FPath UserDataDirectory();
} // namespace Toolbox
#endif
