// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_POPUP_H
#define DXF_UI_POPUP_H
#include "Dxf/UiPanel.h"
#include "Dxf/UiSignal.h"
namespace Dxf
{
/**
 * Popup領域へ接続する画面。Modalは背景への操作を遮り、閉じると元のフォーカスへ戻す。
 * 内容は別の部品として設定する。Closeは切断だけで、再Openできる。
 */
class DUiPopup : public DUiPanel
{
public:
	explicit DUiPopup(bool bModal = true);
	void SetContent(const TUiRef<DUiElement>& Content);
	TResult<void> Open();
	void Close();
	FORCEINLINE bool IsOpen() const noexcept
	{
		return IsAttached();
	}

	FORCEINLINE TUiSignal<>& OnClosed() noexcept
	{
		return m_Closed;
	}

	void SetCloseOnBackdrop(bool bClose) noexcept
	{
		m_bCloseOnBackdrop = bClose;
	}

protected:
	void OnAttach() override;
	/**
	 * Rootから届いたポインター操作を処理する。
	 * @param Event 処理済み・キャプチャ等を反映するイベント。
	 */
	void OnPointerEvent(FUiPointerEvent& Event) override;
	/**
	 * Rootから届いた方向・決定操作を処理する。
	 * @param Event 処理済みの状態を返すイベント。
	 */
	void OnNavigationEvent(FUiNavigationEvent& Event) override;

private:
	TUiRef<DUiElement> m_Content;
	TUiSignal<> m_Closed;
	bool m_bModal = true;
	bool m_bCloseOnBackdrop = false;
};
} // namespace Dxf
// namespace Dxf
#endif
