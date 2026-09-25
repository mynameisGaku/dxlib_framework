// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_INPUT_DISPATCHER_H
#define DXF_UI_INPUT_DISPATCHER_H
#include "Dxf/UiElement.h"
#include "Dxf/UiInput.h"
namespace Dxf::Detail
{
struct FUiRootState;

/**
 * ポインターの仲介。同じレイアウト・クリップ・重なり順の最前面判定から、ホバー・押下・キャプチャ・ホイールを決める。
 * 各要素はOSの入力を読まず、ここから届く出来事だけを使う。
 */
class FUiInputDispatcher
{
public:
	/**
	 * 一フレームのポインターを処理する。
	 * @param State ルートの状態。
	 * @param Frame 入力。
	 * @param Result 結果（追記）。
	 */
	void Process(FUiRootState& State, const FUiPointerFrame& Frame, FUiInputResult& Result);
	/**
	 * 最前面の要素。
	 * @param State ルートの状態。
	 * @param Position ルートの論理座標。
	 */
	static DUiElement* HitTest(FUiRootState& State, FVector2 Position);
	/**
	 * ホバー中の最前面の要素。
	 */
	FORCEINLINE DUiElement* GetHovered() const noexcept
	{
		return m_Hovered.Get();
	}
	/**
	 * キャプチャ中の要素。
	 */
	FORCEINLINE DUiElement* GetCaptured() const noexcept
	{
		return m_Captured.Get();
	}
	/**
	 * 最後のポインターの位置（ルートの論理座標）。
	 */
	FORCEINLINE FVector2 GetLastPosition() const noexcept
	{
		return m_LastPosition;
	}
	/**
	 * 要素へキャプチャする（押下の配布中だけ成功する。すでに別の要素がキャプチャ中なら失敗）。
	 * @param State ルートの状態。
	 * @param Element 要素。
	 */
	bool Capture(FUiRootState& State, DUiElement& Element);
	/**
	 * キャプチャを解く（その要素がキャプチャ中なら）。
	 * @param State ルートの状態。
	 * @param Element 要素。
	 */
	void Release(FUiRootState& State, DUiElement& Element) noexcept;
	/**
	 * 切断した要素のホバー・キャプチャを外す。
	 * @param State ルートの状態。
	 * @param Element 要素。
	 */
	void OnElementDetached(FUiRootState& State, DUiElement& Element) noexcept;
	/**
	 * キャプチャ中の要素が受けられなくなっていれば（非表示・無効・切断）、キャプチャを失わせる。
	 * @param State ルートの状態。
	 */
	void ValidateCapture(FUiRootState& State) noexcept;
	/**
	 * ホバーとキャプチャを外す（ポインターがこのルートから離れた等）。
	 * @param State ルートの状態。
	 */
	void Reset(FUiRootState& State) noexcept;

private:
	/**
	 * ホバーの経路を更新し、Enter・Leaveを届ける。
	 * @param State ルートの状態。
	 * @param Hit 最前面の要素。
	 */
	void UpdateHover_Internal(FUiRootState& State, DUiElement* Hit);
	/**
	 * 出来事を対象から親へ届ける（有効で接続済みの要素だけ。処理済みで止める）。
	 * @param State ルートの状態。
	 * @param Target 対象。
	 * @param Event 出来事。
	 */
	static void Bubble_Internal(FUiRootState& State, DUiElement* Target, FUiPointerEvent& Event);
	/**
	 * 一つの要素へ届ける。
	 * @param State ルートの状態。
	 * @param Target 対象。
	 * @param Event 出来事。
	 */
	static void Deliver_Internal(FUiRootState& State, const TUiRef<DUiElement>& Target, FUiPointerEvent& Event);
	/**
	 * キャプチャを失わせる（CaptureLostを届ける）。
	 * @param State ルートの状態。
	 */
	void LoseCapture_Internal(FUiRootState& State) noexcept;
	/**
	 * ホバー中の最前面の要素。
	 */
	TUiRef<DUiElement> m_Hovered;
	/**
	 * ホバーの経路（最前面から祖先へ）。
	 */
	Toolbox::TVector<TUiRef<DUiElement>> m_HoverPath;
	/**
	 * キャプチャ中の要素。
	 */
	TUiRef<DUiElement> m_Captured;
	/**
	 * キャプチャを始めたボタン。
	 */
	EUiPointerButton m_CaptureButton = EUiPointerButton::Primary;
	/**
	 * 押下を配っている最中か（この間だけキャプチャできる）。
	 */
	bool m_bDeliveringDown = false;
	/**
	 * 押下を配っているボタン。
	 */
	EUiPointerButton m_DownButton = EUiPointerButton::Primary;
	/**
	 * 最後の位置。
	 */
	FVector2 m_LastPosition{-1, -1};
	/**
	 * 最後にポインターが面の上にあったか。
	 */
	bool m_bLastPresent = false;
};
} // namespace Dxf::Detail
#endif
