// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_BUTTON_H
#define DXF_UI_BUTTON_H
#include "Dxf/UiLabel.h"
#include "Dxf/UiPressable.h"
namespace Dxf
{
/**
 * 文字を持つボタン（スタイルID "Button"）。ホバー・押下・無効・フォーカスで見た目が変わる。
 */
class DUiButton : public DUiPressable
{
public:
	/**
	 * @param Text 表示する文字。
	 */
	explicit DUiButton(Toolbox::FString Text = {});
	/**
	 * 表示する文字。
	 */
	FORCEINLINE const Toolbox::FString& GetText() const noexcept
	{
		return m_Text;
	}
	/**
	 * 表示する文字を変える。
	 * @param Text 文字。
	 */
	void SetText(Toolbox::FString Text);
	/**
	 * 文字の要素（最初の接続の後に有効）。
	 */
	FORCEINLINE DUiLabel* GetLabel() const noexcept
	{
		return m_Label.Get();
	}

protected:
	/**
	 * 文字の要素を一度だけ作る。
	 */
	void OnFirstAttach() override;
	/**
	 * 状態の見た目（文字色）を文字の要素へ伝える。
	 */
	void OnStyleResolved() override;

private:
	/**
	 * 表示する文字。
	 */
	Toolbox::FString m_Text;
	/**
	 * 文字の要素。
	 */
	TUiRef<DUiLabel> m_Label;
	/**
	 * 文字の要素へ伝えた文字色。
	 */
	FColor m_AppliedForeground{0, 0, 0, 0};
	/**
	 * 文字の要素へ伝えた文字の大きさ。
	 */
	Toolbox::f32 m_AppliedFontSize = -1;
	/**
	 * 文字の要素へ伝えた字体名。
	 */
	Toolbox::FString m_AppliedFontFamily;
};
} // namespace Dxf
#endif
