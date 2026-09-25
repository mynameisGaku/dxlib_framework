// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_FOCUS_MANAGER_H
#define DXF_UI_FOCUS_MANAGER_H
#include "Dxf/UiElement.h"
#include "Dxf/UiInput.h"
namespace Dxf::Detail
{
struct FUiRootState;

/**
 * キーボード・パッドのフォーカスと操作（決定・戻る・Tab・方向と、その繰り返し）。
 */
class FUiFocusManager
{
public:
	/**
	 * 一フレームの操作を処理する。押下は一度、方向とTabは押し続けで繰り返す（決定・戻るは繰り返さない）。
	 * 1フレームの繰り返しは最大1回。
	 * @param State ルートの状態。
	 * @param Frame 操作。
	 * @param DeltaSeconds 前回からの秒数。
	 * @param Result 結果（追記）。
	 */
	void Process(FUiRootState& State, const FUiNavigationFrame& Frame, Toolbox::f64 DeltaSeconds,
	             FUiInputResult& Result);
	/**
	 * フォーカスを移す（受けられない要素・Modalの外は失敗）。nullptrはフォーカスを外す。
	 * @param State ルートの状態。
	 * @param Element 要素。
	 */
	bool SetFocus(FUiRootState& State, DUiElement* Element);
	/**
	 * フォーカスのある要素。
	 */
	FORCEINLINE DUiElement* GetFocused() const noexcept
	{
		return m_Focused.Get();
	}
	/**
	 * 切断した要素のフォーカスを外す。
	 * @param State ルートの状態。
	 * @param Element 要素。
	 */
	void OnElementDetached(FUiRootState& State, DUiElement& Element) noexcept;
	/**
	 * フォーカスのある要素が受けられなくなっていれば外す。
	 * @param State ルートの状態。
	 */
	void Validate(FUiRootState& State) noexcept;
	/**
	 * 押し続けの状態を捨てる（操作の割当が変わった・ルートが切り替わった）。
	 */
	void ResetRepeat() noexcept;
	/**
	 * 要素がフォーカスを受けられるか（接続済み・表示・有効・フォーカス可・Modalの中）。
	 * @param State ルートの状態。
	 * @param Element 要素。
	 */
	static bool CanFocus(FUiRootState& State, const DUiElement& Element) noexcept;
	/**
	 * フォーカスを受けられる要素の一覧（Tabの順＝前順、Modal中はその中だけ）。
	 * @param State ルートの状態。
	 */
	static Toolbox::TVector<DUiElement*> CollectFocusable(FUiRootState& State);

private:
	/**
	 * 操作を一つ届ける。
	 * @param State ルートの状態。
	 * @param Command 操作。
	 * @param bRepeat 繰り返しか。
	 */
	bool Dispatch_Internal(FUiRootState& State, EUiNavigationCommand Command, bool bRepeat);
	/**
	 * 既定の動作（フォーカスの移動・戻る）。処理したらtrue。
	 * @param State ルートの状態。
	 * @param Command 操作。
	 */
	bool DefaultAction_Internal(FUiRootState& State, EUiNavigationCommand Command);
	/**
	 * フォーカスのある要素。
	 */
	TUiRef<DUiElement> m_Focused;
	/**
	 * 操作ごとの押し続けの秒数。
	 */
	Toolbox::TArray<Toolbox::f64, static_cast<Toolbox::size_t>(EUiNavigationCommand::Count)> m_Held{};
	/**
	 * 操作ごとの次の繰り返しの時刻（押し続けの秒数）。
	 */
	Toolbox::TArray<Toolbox::f64, static_cast<Toolbox::size_t>(EUiNavigationCommand::Count)> m_NextRepeat{};
};
} // namespace Dxf::Detail
#endif
