// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_REBIND_STATE_H
#define DXF_INPUT_REBIND_STATE_H
namespace Dxf
{
/**
 * 再割当入力の採取状態。設定の適用と保存はこの状態とは別。
 */
enum class EInputRebindState
{
	/**
	 * 採取要求なし。
	 */
	Idle,
	/**
	 * 開始入力を含む対象装置の全ボタンの解放待ち。
	 */
	WaitingRelease,
	/**
	 * 解放後の最初の新しい押下待ち。
	 */
	WaitingPress,
	/**
	 * 単一入力を採取済み。Policyによる競合検査と適用は所有側が行う。
	 */
	Ready,
	/**
	 * 明示取消し・失焦・切断・複数同時押下で採取を終了した。
	 */
	Canceled
};
} // namespace Dxf
#endif
