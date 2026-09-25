// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_PRESSABLE_H
#define DXF_UI_PRESSABLE_H
#include "Dxf/UiElement.h"
#include "Dxf/UiSignal.h"
namespace Dxf
{
/**
 * 押して決定する部品の基底。ポインターは押下で対象を確定（キャプチャ）し、同じ要素の上で離したときだけ決定する。
 * 別の要素の上で離す・非表示・無効・切断・キャプチャの喪失では決定しない。キーボード・パッドは「決定」の押下で一度だけ決定する
 * （押し続けの繰り返しでは決定しない）。クリックと決定は同じ意味（OnActivated）。
 */
class DUiPressable : public DUiElement
{
public:
	DUiPressable();
	/**
	 * 決定の通知（利用者の操作だけで発火し、表示の反映では発火しない）。
	 */
	FORCEINLINE TUiSignal<>& OnClicked() noexcept
	{
		return m_Clicked;
	}
	/**
	 * 決定した回数（診断・試験用）。
	 */
	FORCEINLINE Toolbox::uint64 GetActivationCount() const noexcept
	{
		return m_Activations;
	}

protected:
	/**
	 * 押下・移動・解放・キャプチャの喪失を処理する。
	 */
	void OnPointerEvent(FUiPointerEvent& Event) override;
	/**
	 * 決定の操作を処理する。
	 */
	void OnNavigationEvent(FUiNavigationEvent& Event) override;
	/**
	 * 決定したとき（既定はOnClickedを発火する）。
	 */
	virtual void OnActivated();

private:
	/**
	 * 決定の通知。
	 */
	TUiSignal<> m_Clicked;
	/**
	 * 決定した回数。
	 */
	Toolbox::uint64 m_Activations = 0;
};
} // namespace Dxf
#endif
