// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_TOGGLE_H
#define DXF_UI_TOGGLE_H
#include "Dxf/UiButton.h"
namespace Dxf
{
/**
 * 二状態の操作部品。SetValueは表示反映だけで、利用者操作はOnValueChangedへ通知する。
 */
class DUiToggle : public DUiButton
{
public:
	/**
	 * 表示文字と初期チェック状態から操作部品を作る。
	 */
	explicit DUiToggle(Toolbox::FString Text = {}, bool bValue = false);
	/**
	 * ゲームの状態を表示へ反映する（通知しない）。
	 */
	void SetValue(bool bValue) noexcept;
	/**
	 * 現在の表示値を返す。
	 */
	FORCEINLINE bool GetValue() const noexcept
	{
		return m_bValue;
	}

	/**
	 * 利用者操作による値の変更通知を借用する。SetValueからは発火しない。
	 */
	FORCEINLINE TUiSignal<bool>& OnValueChanged() noexcept
	{
		return m_Changed;
	}

protected:
	/**
	 * 利用者の決定操作を現在値へ反映し、操作イベントを発行する。
	 */
	void OnActivated() override;
	/**
	 * 内容より手前のつまみ・チェック等を記録する。
	 * @param Context 論理座標の描画窓口。
	 */
	void OnDrawOverlay(FUiDrawContext& Context) const override;

private:
	/**
	 * 現在のチェック状態。
	 */
	bool m_bValue = false;
	/**
	 * 利用者の変更だけの通知。
	 */
	TUiSignal<bool> m_Changed;
};
} // namespace Dxf
// namespace Dxf
#endif
