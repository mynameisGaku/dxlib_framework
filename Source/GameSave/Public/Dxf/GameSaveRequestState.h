// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMESAVEREQUESTSTATE_H
#define DXF_GAMESAVEREQUESTSTATE_H
namespace Dxf
{
/**
 * 保存公開とゲーム再開の完了を混同しない要求状態。
 */
enum class EGameSaveRequestState
{
	/**
	 * Worker開始前の受付。
	 */
	Pending,
	/**
	 * 不変文書をtempへ書き込んでいる。
	 */
	Writing,
	/**
	 * 面と互換性を読み取っている。
	 */
	Reading,
	/**
	 * 所有側での公開または再開計画採用を待つ。
	 */
	Prepared,
	/**
	 * 保存面の公開結果を検査中。
	 */
	Publishing,
	/**
	 * 保存面の新世代を確認できた。
	 */
	Committed,
	/**
	 * 保存の公開成否を確認できず、取消し成功とは扱えない。
	 */
	PublicationUnknown,
	/**
	 * 通常更新から候補Sceneの切替を要求した。
	 */
	TransitionRequested,
	/**
	 * 必須Readyまで確認したゲーム再開完了。
	 */
	Completed,
	/**
	 * 原因を保持して失敗した。
	 */
	Failed,
	/**
	 * 公開前に取り消した。
	 */
	Canceled,
	/**
	 * 後続の読込に置換された。
	 */
	Superseded
};
} // namespace Dxf
#endif
