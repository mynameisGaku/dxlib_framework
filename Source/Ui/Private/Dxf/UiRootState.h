// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_ROOT_STATE_H
#define DXF_UI_ROOT_STATE_H
#include "Dxf/SlotMap.h"
#include "Dxf/UiRoot.h"
#include "UiFocusManager.h"
#include "UiInputDispatcher.h"
#include "UiLayoutEngine.h"
#include "UiTooltipService.h"
#include "UiTree.h"
#include "Toolbox/Array.h"
namespace Dxf::Detail
{
/**
 * 重なり領域の直下の入れ物。表示面いっぱいに広がり、自身は入力を受けない。
 */
class DUiLayerElement final : public DUiElement
{
public:
	DUiLayerElement()
	{
		SetHitTest(EUiHitTest::ChildrenOnly);
	}
};

/**
 * ルートの内部状態。各部品（木・レイアウト・入力・フォーカス・ツールチップ）はこの状態を共有して操作する。
 */
struct FUiRootState
{
	/**
	 * @param Owner 窓口。
	 * @param InSettings 設定。
	 */
	FUiRootState(FUiRoot& Owner, FUiRootSettings InSettings);
	/**
	 * 窓口。
	 */
	FUiRoot* pOwner;
	/**
	 * 表示アダプターが参照する生存印。
	 */
	Toolbox::TSharedPtr<FUiRootLifetime> Lifetime;
	/**
	 * 入力・更新への再入を拒否する。
	 */
	bool bProcessingInput = false;
	bool bUpdating = false;
	/**
	 * 生存する最前面のModal。窓口の生存に依存しない。
	 */
	DUiElement* GetTopModal() const noexcept
	{
		if (bShuttingDown)
		{
			return nullptr;
		}
		for (Toolbox::size_t Index = Modals.Size(); Index > 0; --Index)
		{
			DUiElement* Modal = Modals[Index - 1].Get();
			if (IsLive(Modal) && Modal->IsVisibleInTree())
			{
				return Modal;
			}
		}
		return nullptr;
	}
	/**
	 * 設定。
	 */
	FUiRootSettings Settings;
	/**
	 * 要素の格納領域（唯一の所有者）。
	 */
	TSlotMap<DUiElement> Elements;
	/**
	 * 次に割り当てる要素の番号。
	 */
	Toolbox::uint64 NextId = 1;
	/**
	 * 重なり領域の入れ物。
	 */
	Toolbox::TArray<DUiElement*, static_cast<Toolbox::size_t>(EUiLayer::Count)> Layers{};
	/**
	 * 木の操作。
	 */
	FUiTree Tree;
	/**
	 * レイアウト。
	 */
	FUiLayoutEngine Layout;
	/**
	 * ポインターの仲介。
	 */
	FUiInputDispatcher Input;
	/**
	 * フォーカスと操作。
	 */
	FUiFocusManager Focus;
	/**
	 * ツールチップ。
	 */
	FUiTooltipService Tooltip;
	/**
	 * Modalの積み重ね（最後が最前面）。
	 */
	Toolbox::TVector<TUiRef<DUiElement>> Modals;
	/**
	 * Modalを開く前のフォーカス（Modalsと同じ順）。
	 */
	Toolbox::TVector<TUiRef<DUiElement>> ModalReturnFocus;
	/**
	 * 投函の列。
	 */
	Toolbox::TSharedPtr<FUiPostQueue> PostQueue;
	/**
	 * スタイル。
	 */
	Toolbox::TSharedPtr<const FUiStyleSheet> Styles;
	/**
	 * 組込みの既定スタイル。
	 */
	Toolbox::TSharedPtr<const FUiStyleSheet> BuiltInStyles;
	/**
	 * 表示面。
	 */
	FUiSurface Surface;
	/**
	 * 経過秒数。
	 */
	Toolbox::f64 Elapsed = 0;
	/**
	 * 「戻る」の通知。
	 */
	TUiSignal<> CancelRequested;
	/**
	 * 毎フレームの更新を受ける要素。
	 */
	Toolbox::TVector<TUiRef<DUiElement>> Updating;
	/**
	 * 最後の描画の命令の数。
	 */
	Toolbox::size_t LastDrawItems = 0;
	/**
	 * 出来事・更新・通知を配っている深さ（この間は解放しない）。
	 */
	Toolbox::int32 DispatchDepth = 0;
	/**
	 * レイアウト・描画の記録中か（この間は木を変更しない）。
	 */
	bool bFrozen = false;
	/**
	 * 窓口の破棄中か。
	 */
	bool bShuttingDown = false;
	/**
	 * 現在のスタイル（未設定なら組込み）。
	 */
	const FUiStyleSheet& GetStyles() const noexcept
	{
		return Styles ? *Styles : *BuiltInStyles;
	}
	/**
	 * 重なり領域の入れ物。
	 * @param Layer 重なり領域。
	 */
	FORCEINLINE DUiElement* GetLayer(EUiLayer Layer) const noexcept
	{
		return Layers[static_cast<Toolbox::size_t>(Layer)];
	}
	/**
	 * 要素がこのルートのものか。
	 * @param Element 要素。
	 */
	FORCEINLINE bool Owns(const DUiElement* Element) const noexcept
	{
		return Element != nullptr && Element->m_pRoot == pOwner;
	}
	/**
	 * 要素が入力・描画の対象か（接続済み・破棄の要求なし・レイアウトが有効）。
	 * @param Element 要素。
	 */
	FORCEINLINE static bool IsLive(const DUiElement* Element) noexcept
	{
		return Element != nullptr && Element->m_AttachState == EUiAttachState::Attached &&
		       !Element->m_bDestroyRequested;
	}
	/**
	 * 要素の状態の変化を知らせる（見た目の解決と派生クラスの通知）。
	 * @param Element 要素。
	 */
	static void NotifyStateChanged(DUiElement& Element);
	/**
	 * 要素の見た目を解決する（状態を反映）。
	 * @param Element 要素。
	 */
	void ResolveStyle(DUiElement& Element);
	/**
	 * 要素のフィールドへ触る窓口（内部の部品が使う）。
	 */
	FORCEINLINE static bool& Hovered(DUiElement& Element) noexcept
	{
		return Element.m_bHovered;
	}
	/**
	 * 要素のフィールドへ触る窓口。
	 */
	FORCEINLINE static bool& Focused(DUiElement& Element) noexcept
	{
		return Element.m_bFocused;
	}
	/**
	 * 要素のフィールドへ触る窓口。
	 */
	FORCEINLINE static bool& Pressed(DUiElement& Element) noexcept
	{
		return Element.m_bPressed;
	}
	/**
	 * 出来事の配布の範囲（この間は解放しない）。
	 */
	class FDispatchScope
	{
	public:
		/**
		 * @param State 状態。
		 */
		explicit FDispatchScope(FUiRootState& State) noexcept : m_pState(&State)
		{
			++m_pState->DispatchDepth;
		}
		~FDispatchScope()
		{
			--m_pState->DispatchDepth;
		}
		FDispatchScope(const FDispatchScope&) = delete;
		FDispatchScope& operator=(const FDispatchScope&) = delete;

	private:
		/**
		 * 状態。
		 */
		FUiRootState* m_pState;
	};
};
} // namespace Dxf::Detail
#endif
