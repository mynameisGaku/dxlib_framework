// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_EFILEOPERATION_H
#define TOOLBOX_EFILEOPERATION_H
namespace Toolbox
{
/**
 * ファイル処理の失敗位置。公開失敗は名前の不変性を意味しない。
 */
enum class EFileOperation
{
	/**
	 * 排他取得が他の利用者と競合した。
	 */
	Busy,
	/**
	 * ファイルの読み取りまたは属性取得に失敗した。
	 */
	Read,
	/**
	 * 読み取り前にファイルの有限上限を超えた。
	 */
	SizeLimit,
	/**
	 * 一時ファイルを新規作成できなかった。
	 */
	Create,
	/**
	 * 全バイトを書き込めなかった。
	 */
	Write,
	/**
	 * OSの書き込み完了要求が失敗した。
	 */
	Flush,
	/**
	 * 明示したcloseが失敗した。
	 */
	Close,
	/**
	 * ファイル名の公開が失敗した。
	 */
	Publish,
	/**
	 * 保存領域を取得できなかった。
	 */
	Root
};
} // namespace Toolbox
#endif
