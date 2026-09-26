// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SAMPLE_WIDGETS_H
#define DXF_UI_SAMPLE_WIDGETS_H
#include "Dxf/UiPanel.h"
#include "Dxf/UiRoot.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiLabel.h"
namespace Dxf::UiSample
{
/**
 * サンプル画面で共用するボタンの寸法だけをまとめる。
 */
inline void SetupSampleButton(DUiButton& Button, const char* Name)
{
	Button.SetName(Name);
	Button.SetWidth(FUiLength::Fill());
	Button.SetHeight(FUiLength::Fixed(44));
}
/**
 * サンプルの見出しや説明行。
 */
inline void SetupSampleLabel(DUiLabel& Label, const char* Name, Toolbox::f32 Height = 30)
{
	Label.SetName(Name);
	Label.SetWidth(FUiLength::Fill());
	Label.SetHeight(FUiLength::Fixed(Height));
	Label.SetTextLayout(EUiTextWrap::NoWrap, EUiTextOverflow::Ellipsis);
}
} // namespace Dxf::UiSample
#endif
