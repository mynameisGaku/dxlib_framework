// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SCROLL_VIEW_H
#define DXF_UI_SCROLL_VIEW_H
#include "Dxf/UiElement.h"
namespace Dxf
{
/**
 * スクロールする軸。
 */
enum class EUiScrollAxes : Toolbox::uint8
{
	/**
	 * 縦だけ（既定）。
	 */
	Vertical = 1,
	/**
	 * 横だけ。
	 */
	Horizontal = 2,
	/**
	 * 縦と横。
	 */
	Both = 3
};

/**
 * ホイール・方向操作のスクロールの慣性（既定は無効）。
 * 有効なら移動量は変えずに、残りの距離を毎秒DecayPerSecondの割合の指数で減らしながら進む。
 * 経過時間だけで決まるため、30／60／144Hzのどれで更新しても同じ時刻の位置は同じになる。
 */
struct FUiScrollInertia
{
	/**
	 * 慣性を使うか。
	 */
	bool bEnabled = false;
	/**
	 * 残りの距離の減衰の速さ（1/秒、正の有限値）。
	 */
	Toolbox::f64 DecayPerSecond = 12;
	/**
	 * 残りがこの論理距離より小さくなったら到着とする（正の有限値）。
	 */
	Toolbox::f64 SnapDistance = 0.5;
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiScrollInertia&) const = default;
};

/**
 * 縦・横・両方向のスクロールの入れ物。内容の配置・クリップ・残りホイール・つまみ操作を一か所で扱う。
 * ホイールの1ノッチは表示寸法の割合、または指定した論理距離。縦のホイールは縦、Shift＋ホイール（横のノッチ）は横へ使い、
 * 使いきれなかった分は軸ごとに外側へ渡す。
 */
class DUiScrollView : public DUiElement
{
public:
	/**
	 * 縦方向のスクロール領域を作る。
	 */
	DUiScrollView();
	/**
	 * 未接続で同じRootの内容を設定する。古い内容は切断し、破棄はしない。
	 */
	void SetContent(const TUiRef<DUiElement>& Content);
	/**
	 * スクロールする軸を設定する。無効にした軸の位置は0へ戻す。
	 * @param Axes 軸。
	 */
	void SetScrollAxes(EUiScrollAxes Axes);
	/**
	 * スクロールする軸。
	 */
	FORCEINLINE EUiScrollAxes GetScrollAxes() const noexcept
	{
		return m_Axes;
	}
	/**
	 * 慣性を設定する（進行中の移動は新しい設定で続ける）。
	 * @param Inertia 設定。
	 */
	void SetInertia(const FUiScrollInertia& Inertia);
	/**
	 * 慣性の設定。
	 */
	FORCEINLINE const FUiScrollInertia& GetInertia() const noexcept
	{
		return m_Inertia;
	}
	/**
	 * 縦の論理距離の位置。配置済みなら端へ制限する。慣性の残りは捨てる。
	 */
	void SetScrollOffset(Toolbox::f64 Offset);
	/**
	 * 横の論理距離の位置。配置済みなら端へ制限する。慣性の残りは捨てる。
	 */
	void SetScrollOffsetX(Toolbox::f64 Offset);
	/**
	 * 縦の表示開始位置を、内容の先頭からの論理距離で返す。
	 */
	FORCEINLINE Toolbox::f64 GetScrollOffset() const noexcept
	{
		return m_Offset[1];
	}
	/**
	 * 横の表示開始位置を、内容の左端からの論理距離で返す。
	 */
	FORCEINLINE Toolbox::f64 GetScrollOffsetX() const noexcept
	{
		return m_Offset[0];
	}
	/**
	 * 内容と表示領域から求めた縦の最大スクロール距離を返す。
	 */
	FORCEINLINE Toolbox::f64 GetScrollMaximum() const noexcept
	{
		return GetMaximum_Internal(1);
	}
	/**
	 * 内容と表示領域から求めた横の最大スクロール距離を返す。
	 */
	FORCEINLINE Toolbox::f64 GetScrollMaximumX() const noexcept
	{
		return GetMaximum_Internal(0);
	}
	/**
	 * 慣性でまだ進む距離（横, 縦）。慣性が無効なら0。
	 */
	FORCEINLINE FVector2 GetPendingScroll() const noexcept
	{
		return {static_cast<Toolbox::f32>(GetPending_Internal(0)), static_cast<Toolbox::f32>(GetPending_Internal(1))};
	}
	/**
	 * ホイールの1ノッチの距離。
	 * @param Amount 表示寸法の割合（bRelative）または論理距離。
	 * @param bRelative 表示寸法に対する割合か。
	 */
	void SetWheelStep(Toolbox::f64 Amount, bool bRelative = true);
	/**
	 * スクロールバーの太さ（論理単位）。
	 */
	FORCEINLINE Toolbox::f32 GetScrollBarWidth() const noexcept
	{
		return m_BarWidth;
	}
	/**
	 * 縦のつまみの矩形（スクロールしないなら空）。
	 */
	FUiRect GetScrollThumbRect() const noexcept;
	/**
	 * 横のつまみの矩形（スクロールしないなら空）。
	 */
	FUiRect GetScrollThumbRectX() const noexcept;

protected:
	/**
	 * 内容を測り、スクロールする軸は内容の寸法を覚える。
	 */
	FUiSize OnMeasure(FUiLayoutContext& Context, FUiSize Available) override;
	/**
	 * 表示領域を決め、内容を位置だけずらして配置する。
	 */
	void OnArrange(FUiLayoutContext& Context, const FUiRect& Content) override;
	/**
	 * スクロールバーを描く。
	 */
	void OnDrawOverlay(FUiDrawContext& Context) const override;
	/**
	 * ホイール・つまみの操作。
	 */
	void OnPointerEvent(FUiPointerEvent& Event) override;
	/**
	 * 方向操作によるスクロール（横は横の軸があるときだけ）。
	 */
	void OnNavigationEvent(FUiNavigationEvent& Event) override;
	/**
	 * 慣性の移動を時間で進める。
	 */
	void OnUpdate(const FUiUpdateContext& Context) override;
	/**
	 * スクロールする内容を測る（派生で置換える）。
	 * @param Available 使える寸法。スクロールする軸は無制限。
	 */
	virtual FUiSize MeasureScrollable(FUiLayoutContext& Context, FUiSize Available);
	/**
	 * スクロールする内容を配置する（派生で置換える）。
	 * @param View バーを除いた表示領域。
	 */
	virtual void ArrangeScrollable(FUiLayoutContext& Context, const FUiRect& View);
	/**
	 * 縦の内容の高さを派生が直接設定する（仮想化した一覧）。
	 */
	FORCEINLINE void SetContentHeight_Internal(Toolbox::f64 Height) noexcept
	{
		m_ContentSize[1] = Height;
	}

private:
	/**
	 * 軸（0=横、1=縦）がスクロールするか。
	 */
	FORCEINLINE bool HasAxis_Internal(Toolbox::size_t Axis) const noexcept
	{
		return (static_cast<Toolbox::uint8>(m_Axes) & (Axis == 0 ? 2u : 1u)) != 0;
	}
	/**
	 * 軸の最大スクロール距離。
	 */
	FORCEINLINE Toolbox::f64 GetMaximum_Internal(Toolbox::size_t Axis) const noexcept
	{
		return HasAxis_Internal(Axis) ? Toolbox::Max(0.0, m_ContentSize[Axis] - m_ViewSize[Axis]) : 0.0;
	}
	/**
	 * 慣性で残っている距離。
	 */
	FORCEINLINE Toolbox::f64 GetPending_Internal(Toolbox::size_t Axis) const noexcept
	{
		return m_bMoving[Axis] ? m_Target[Axis] - m_Offset[Axis] : 0.0;
	}
	/**
	 * 軸の慣性の移動を止める。
	 */
	FORCEINLINE void Stop_Internal(Toolbox::size_t Axis) noexcept
	{
		m_bMoving[Axis] = false;
		m_Target[Axis] = m_Offset[Axis];
	}
	/**
	 * 軸の位置を設定する（慣性の残りは呼び出し側が扱う）。
	 */
	void SetOffset_Internal(Toolbox::size_t Axis, Toolbox::f64 Offset);
	/**
	 * 軸へノッチ数だけスクロールし、使ったノッチ数を返す（慣性なら目標だけ進める）。
	 */
	Toolbox::f64 ScrollBy_Internal(Toolbox::size_t Axis, Toolbox::f64 Notches);
	/**
	 * 軸の1ノッチの論理距離。
	 */
	Toolbox::f64 GetStep_Internal(Toolbox::size_t Axis) const noexcept;
	/**
	 * バーを除いた表示領域。
	 */
	FUiRect GetViewRect_Internal() const noexcept;
	/**
	 * つまみを指の位置へ動かす。
	 */
	void DragTo_Internal(FVector2 Position);
	/**
	 * 慣性の残りの有無で毎フレームの更新を切り替える。
	 */
	void RefreshUpdate_Internal();
	/**
	 * 内容。
	 */
	TUiRef<DUiElement> m_Content;
	/**
	 * スクロールする軸。
	 */
	EUiScrollAxes m_Axes = EUiScrollAxes::Vertical;
	/**
	 * 慣性の設定。
	 */
	FUiScrollInertia m_Inertia;
	/**
	 * 軸ごとの位置・内容の寸法・表示の寸法・慣性の到着点と移動中か（0=横、1=縦）。
	 */
	Toolbox::f64 m_Offset[2] = {};
	Toolbox::f64 m_ContentSize[2] = {};
	Toolbox::f64 m_ViewSize[2] = {};
	Toolbox::f64 m_Target[2] = {};
	bool m_bMoving[2] = {};
	/**
	 * ホイールの1ノッチの距離。
	 */
	Toolbox::f64 m_WheelStep = 0.25;
	/**
	 * スクロールバーの太さ。
	 */
	Toolbox::f32 m_BarWidth = 12;
	/**
	 * つまみを掴んだ位置（つまみの先頭から）。
	 */
	Toolbox::f32 m_GrabOffset = 0;
	/**
	 * 掴んでいる軸（0=横、1=縦）。
	 */
	Toolbox::size_t m_DragAxis = 1;
	/**
	 * ホイールの距離が表示寸法の割合か。
	 */
	bool m_bRelativeWheel = true;
};
} // namespace Dxf
#endif
