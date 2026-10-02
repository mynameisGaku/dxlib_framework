// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_ESAVEERROR_H
#define DXF_ESAVEERROR_H
namespace Dxf
{
/**
 * 保存の採用を止める原因。OSの個別工程はToolboxのFileErrorで区別する。
 */
enum class ESaveError
{
	/**
	 * 形式や値が不正。
	 */
	InvalidDocument,
	/**
	 * 未対応の保存形式バージョン。
	 */
	UnsupportedVersion,
	/**
	 * ゲームまたはContentが異なる。
	 */
	IncompatibleContent,
	/**
	 * 面の完全性が不正。
	 */
	Corrupt,
	/**
	 * 同世代の異なる面が存在する。
	 */
	Conflict,
	/**
	 * 世代が最大値に到達した。
	 */
	GenerationOverflow,
	/**
	 * 保存Rootまたはslotが不正。
	 */
	InvalidLocation,
	/**
	 * 公開したかどうかを確認できない。
	 */
	PublicationUnknown,
	/**
	 * 公開・取消し済みの要求に再操作した。
	 */
	TerminalRequest
};
} // namespace Dxf
#endif
