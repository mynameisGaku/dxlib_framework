// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_REBIND_CAPTURE_H
#define DXF_INPUT_REBIND_CAPTURE_H
#include "Dxf/InputRebindState.h"
#include "Dxf/InputBindingDescription.h"
#include "Dxf/InputSnapshot.h"
namespace Dxf
{
/**
 * 既存Snapshotから再割当入力だけを採取する。OS入力やInputMapの更新は行わない。
 */
class FInputRebindCapture
{
public:
	/**
	 * @param Device 採取する装置。
	 * @param PadSlot Gamepadの場合の0〜3。
	 * @param Current UI消費前のSnapshot。不正装置・失焦・対象Pad切断は取消しでfalse。
	 * 開始クリックを採用せず、対象装置を一度すべて解放してから採取する。
	 */
	bool Begin(EInputBindingDevice Device, Toolbox::uint32 PadSlot, const FInputSnapshot& Current) noexcept;
	/**
	 * @param Current 新しいSnapshot。失焦・切断・複数同時押下は取消しになる。
	 * Ready以降は変更しない。確保せず、ゲーム入力への配送は呼出し側が抑止する。
	 */
	void Poll(const FInputSnapshot& Current) noexcept;
	/**
	 * 採取待ちだけを取消す。適用済みの設定や保存ファイルを戻す操作ではない。
	 */
	void Cancel() noexcept;
	/**
	 * 現在の採取状態を返す。
	 */
	EInputRebindState GetState() const noexcept;
	/**
	 * @param Result Readyの入力を書き込む。未ReadyならResultを変更せずfalse。
	 */
	bool TryGetBinding(FInputBindingDescription& Result) const noexcept;

private:
	/**
	 * 対象装置・Pad番号と採取後のボタン。保存値を持つだけで実体は所有しない。
	 */
	FInputBindingDescription m_Binding;
	/**
	 * 終端状態を持つ採取要求。
	 */
	EInputRebindState m_State = EInputRebindState::Idle;
};
} // namespace Dxf
#endif
