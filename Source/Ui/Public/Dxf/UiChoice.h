// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_CHOICE_H
#define DXF_UI_CHOICE_H
#include "Dxf/UiButton.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 小さな選択肢の切替。表示反映は無通知、決定・左右操作だけ選択を通知する。
 */
class DUiChoice : public DUiButton
{
public:
	/**
	 * 表示する選択肢を置き換え、先頭を選ぶ。空の場合は部品を無効にする。
	 * @param Options 所有する選択肢文字列。
	 */
	void SetOptions(Toolbox::TVector<Toolbox::FString> Options);
	/**
	 * 指定の選択肢を表示する。範囲外は例外で、操作通知は行わない。
	 * @param Index 0から始まる選択肢番号。
	 */
	void SetSelectedIndex(Toolbox::size_t Index);
	/**
	 * 現在の選択肢番号を返す。選択肢が空の場合は0。
	 */
	FORCEINLINE Toolbox::size_t GetSelectedIndex() const noexcept
	{
		return m_Selected;
	}

	/**
	 * 利用者の選択操作だけを通知する発行元を借用する。
	 */
	FORCEINLINE TUiSignal<Toolbox::size_t>& OnSelectionChanged() noexcept
	{
		return m_Changed;
	}

protected:
	/**
	 * 利用者の決定操作を現在値へ反映し、操作イベントを発行する。
	 */
	void OnActivated() override;
	/**
	 * Rootから届いた方向・決定操作を処理する。
	 * @param Event 処理済みの状態を返すイベント。
	 */
	void OnNavigationEvent(FUiNavigationEvent& Event) override;

private:
	Toolbox::TVector<Toolbox::FString> m_Options;
	Toolbox::size_t m_Selected = 0;
	TUiSignal<Toolbox::size_t> m_Changed;
	void Change_Internal(bool bNext);
};
} // namespace Dxf
// namespace Dxf
#endif
