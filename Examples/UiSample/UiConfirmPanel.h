// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UICONFIRMPANEL_H
#define DXF_UICONFIRMPANEL_H
#include "UiSampleState.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiButton.h"
namespace Dxf::UiSample
{
/**
 * タイトルへ戻る前の確認の組合せ。開閉とScene切替の判断はサンプルのShellに置く。
 */
class DUiConfirmPanel final : public DUiPanel
{
public:
	/**
	 * @param State 共有する表示用の状態。
	 */
	explicit DUiConfirmPanel(Toolbox::TSharedPtr<FUiSampleState> State);

protected:
	/**
	 * 文言と、はい／いいえのボタンを作る。
	 */
	void OnFirstAttach() override;
	/**
	 * ボタンの決定を操作の要求へつなぐ。
	 */
	void OnAttach() override;

private:
	/**
	 * 共有する表示用の状態。
	 */
	Toolbox::TSharedPtr<FUiSampleState> m_pState;
	/**
	 * はい（タイトルへ戻る）。
	 */
	TUiRef<DUiButton> m_Yes;
	/**
	 * いいえ（一時停止へ戻る）。
	 */
	TUiRef<DUiButton> m_No;
};
} // namespace Dxf::UiSample
#endif
