// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_ELEMENT_H
#define DXF_UI_ELEMENT_H
#include "Dxf/Object.h"
#include "Dxf/ObjectHandle.h"
#include "Dxf/Result.h"
#include "Dxf/UiBinding.h"
#include "Dxf/UiEvents.h"
#include "Dxf/UiLayoutParams.h"
#include "Dxf/UiStyle.h"
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
class FUiRoot;
class FUiDrawContext;
class FUiLayoutContext;
namespace Detail
{
struct FUiRootState;
class FUiLayoutEngine;
class FUiTree;
class FUiInputDispatcher;
class FUiFocusManager;
class FUiTooltipService;
class FUiDrawBuilder;
} // namespace Detail

/**
 * UI要素への型付き・世代付きの参照。破棄を要求した要素・破棄したルートの要素は解決しない（nullptr）。
 */
template <typename T> using TUiRef = TObjectHandle<T>;

/**
 * 要素の接続の状態。
 */
enum class EUiAttachState : Toolbox::uint8
{
	/**
	 * どの重なり領域からも辿れない（作成直後・取り外し後）。
	 */
	Detached,
	/**
	 * 接続のフックを呼んでいる途中。
	 */
	Attaching,
	/**
	 * 重なり領域から辿れ、レイアウト・描画・入力の対象。
	 */
	Attached,
	/**
	 * 切断のフックを呼んでいる途中。
	 */
	Detaching
};

/**
 * 検査で意図した例外として扱う項目（宣言した理由とともに記録する）。
 */
enum class EUiCheckAllowance : Toolbox::uint8
{
	/**
	 * なし。
	 */
	None = 0,
	/**
	 * 文字が割当領域を超えてよい（省略・スクロールで扱う等）。
	 */
	TextOverflow = 1,
	/**
	 * 子が親の範囲からはみ出してよい（装飾・スクロールの内容）。
	 */
	LayoutOverflow = 2,
	/**
	 * 兄弟と重なってよい（重ね合わせの装飾・バッジ等）。
	 */
	Overlap = 4,
	/** 低解像度を意図する画像（理由の宣言が必須）。 */
	ImageResolution = 8
};

/**
 * UI要素の基底。ルート（FUiRoot）が作成・所有し、親子の木で並べる。
 * 構造と動作はC++の派生クラス、見た目はスタイルIDと型付きのスタイルで決める。
 * 子の構造はOnFirstAttachで一度だけ作り、毎フレーム作り直さない。
 * 所有スレッド（ルートを操作するスレッド）だけで操作する。
 */
class DUiElement : public DObject
{
public:
	~DUiElement() override;

	/**
	 * ルート内で一意の番号（作成順、0は未登録）。
	 */
	FORCEINLINE Toolbox::uint64 GetId() const noexcept
	{
		return m_Id;
	}
	/**
	 * 検査・診断で使う安定した名前。
	 */
	FORCEINLINE const Toolbox::FString& GetName() const noexcept
	{
		return m_Name;
	}
	/**
	 * 安定した名前を設定する。
	 * @param Name 名前。
	 */
	void SetName(Toolbox::FString Name)
	{
		m_Name = Toolbox::Move(Name);
	}
	/**
	 * スタイルID。空はルートの既定スタイル。
	 */
	FORCEINLINE const Toolbox::FString& GetStyleId() const noexcept
	{
		return m_StyleId;
	}
	/**
	 * スタイルIDを設定する（次のレイアウトで解決し直す）。
	 * @param StyleId スタイルID。
	 */
	void SetStyleId(Toolbox::FString StyleId);
	/**
	 * 要素だけの見た目の上書きを設定する（スタイルIDの値の後に反映する）。
	 * @param Patch 上書き。
	 */
	void SetStyleOverride(FUiStylePatch Patch);
	/**
	 * 現在の状態を反映した見た目。
	 */
	FORCEINLINE const FUiStyle& GetStyle() const noexcept
	{
		return m_Style;
	}
	/**
	 * 現在の状態（ポインター・押下・フォーカス・無効）。
	 */
	EUiStyleState GetStyleState() const noexcept;

	/**
	 * 所有するルート（作成時に決まる）。
	 */
	FORCEINLINE FUiRoot* GetRoot() const noexcept
	{
		return m_pRoot;
	}
	/**
	 * 親（なければnullptr）。
	 */
	FORCEINLINE DUiElement* GetParent() const noexcept
	{
		return m_pParent;
	}
	/**
	 * 子の数。
	 */
	FORCEINLINE Toolbox::size_t GetChildCount() const noexcept
	{
		return m_Children.Size();
	}
	/**
	 * 子（並び順）。
	 * @param Index 位置。
	 */
	FORCEINLINE DUiElement* GetChild(Toolbox::size_t Index) const noexcept
	{
		return Index < m_Children.Size() ? m_Children[Index] : nullptr;
	}
	/**
	 * 接続の状態。
	 */
	FORCEINLINE EUiAttachState GetAttachState() const noexcept
	{
		return m_AttachState;
	}
	/**
	 * 接続済みか。
	 */
	FORCEINLINE bool IsAttached() const noexcept
	{
		return m_AttachState == EUiAttachState::Attached;
	}
	/**
	 * 破棄を要求済みか（参照は解決しない）。
	 */
	FORCEINLINE bool IsDestroyRequested() const noexcept
	{
		return m_bDestroyRequested;
	}
	/**
	 * 自身への型付き参照。
	 */
	template <typename T = DUiElement> TUiRef<T> GetRef() const noexcept
	{
		return m_Self.template Cast<T>();
	}

	/**
	 * レイアウトの指定。
	 */
	FORCEINLINE const FUiLayoutParams& GetLayout() const noexcept
	{
		return m_Layout;
	}
	/**
	 * レイアウトの指定を置き換える（内側の余白は明示したものとして扱う）。
	 * @param Layout 指定。
	 */
	void SetLayout(const FUiLayoutParams& Layout);
	/**
	 * 幅の決め方。
	 * @param Width 指定。
	 */
	void SetWidth(FUiLength Width);
	/**
	 * 高さの決め方。
	 * @param Height 指定。
	 */
	void SetHeight(FUiLength Height);
	/**
	 * 大きさの下限と上限。
	 * @param Min 下限。
	 * @param Max 上限。
	 */
	void SetSizeLimits(FUiSize Min, FUiSize Max);
	/**
	 * 外側の余白。
	 * @param Margin 余白。
	 */
	void SetMargin(FUiThickness Margin);
	/**
	 * 内側の余白（スタイルの値より優先する）。
	 * @param Padding 余白。
	 */
	void SetPadding(FUiThickness Padding);
	/**
	 * 親の範囲の中での配置。
	 * @param Horizontal 横。
	 * @param Vertical 縦。
	 */
	void SetAlign(EUiAlign Horizontal, EUiAlign Vertical);
	/**
	 * 重ね合わせの親の内容範囲の左上からの位置で置く。
	 * @param Position 位置（論理単位）。
	 */
	void SetAbsolutePosition(FVector2 Position);
	/**
	 * 子の並べ方。
	 */
	FORCEINLINE EUiStackMode GetStackMode() const noexcept
	{
		return m_StackMode;
	}
	/**
	 * 子の並べ方を設定する。
	 * @param Mode 並べ方。
	 * @param Gap 子の間隔（論理単位）。
	 */
	void SetStack(EUiStackMode Mode, Toolbox::f32 Gap = 0);
	/**
	 * 子の間隔。
	 */
	FORCEINLINE Toolbox::f32 GetGap() const noexcept
	{
		return m_Gap;
	}
	/**
	 * 実際に使う内側の余白（明示した値、なければスタイルの値）。
	 */
	FUiThickness GetEffectivePadding() const noexcept;
	/**
	 * 配置後の矩形（ルートの論理座標）。
	 */
	FORCEINLINE const FUiRect& GetRect() const noexcept
	{
		return m_Rect;
	}
	/**
	 * 配置後の内容範囲（内側の余白を除いた矩形）。
	 */
	FUiRect GetContentRect() const noexcept;
	/**
	 * 最後の寸法の計算で求めた、望む大きさ（余白を除く）。
	 */
	FORCEINLINE FUiSize GetDesiredSize() const noexcept
	{
		return m_DesiredSize;
	}
	/**
	 * 描画・入力に効くクリップ（祖先のクリップの共通部分、ルートの論理座標）。
	 */
	FORCEINLINE const FUiRect& GetClipRect() const noexcept
	{
		return m_ClipRect;
	}
	/**
	 * 最後の寸法の計算で不正な値（NaN・無限大・負・下限>上限・計算の例外）があったか。描画・入力の対象外。
	 */
	FORCEINLINE bool HasLayoutError() const noexcept
	{
		return m_bLayoutInvalid;
	}
	/**
	 * 子を自身の矩形でクリップするか。
	 */
	FORCEINLINE bool IsClippingChildren() const noexcept
	{
		return m_bClipChildren;
	}
	/**
	 * 子を自身の矩形でクリップするかを設定する。
	 * @param bClip クリップするか。
	 */
	void SetClipChildren(bool bClip);

	/**
	 * 表示の状態。
	 */
	FORCEINLINE EUiVisibility GetVisibility() const noexcept
	{
		return m_Visibility;
	}
	/**
	 * 表示の状態を設定する。非表示にするとポインターのキャプチャ・フォーカスを失う。
	 * @param Visibility 状態。
	 */
	void SetVisibility(EUiVisibility Visibility);
	/**
	 * 自身と祖先がすべて表示されているか。
	 */
	bool IsVisibleInTree() const noexcept;
	/**
	 * 自身が有効か。
	 */
	FORCEINLINE bool IsEnabled() const noexcept
	{
		return m_bEnabled;
	}
	/**
	 * 有効・無効を設定する。無効にするとキャプチャ・フォーカスを失う。
	 * @param bEnabled 有効か。
	 */
	void SetEnabled(bool bEnabled);
	/**
	 * 自身と祖先がすべて有効か。
	 */
	bool IsEnabledInTree() const noexcept;
	/**
	 * 当たり判定の方針。
	 */
	FORCEINLINE EUiHitTest GetHitTest() const noexcept
	{
		return m_HitTest;
	}
	/**
	 * 当たり判定の方針を設定する。
	 * @param HitTest 方針。
	 */
	FORCEINLINE void SetHitTest(EUiHitTest HitTest) noexcept
	{
		m_HitTest = HitTest;
	}
	/**
	 * キーボード・パッドのフォーカスを受けるか。
	 */
	FORCEINLINE bool IsFocusable() const noexcept
	{
		return m_bFocusable;
	}
	/**
	 * フォーカスを受けるかを設定する。
	 * @param bFocusable 受けるか。
	 */
	void SetFocusable(bool bFocusable);
	/**
	 * フォーカスがあるか。
	 */
	FORCEINLINE bool IsFocused() const noexcept
	{
		return m_bFocused;
	}
	/**
	 * ポインターが上にあるか（自身または子孫が最前面）。
	 */
	FORCEINLINE bool IsHovered() const noexcept
	{
		return m_bHovered;
	}
	/**
	 * 押されているか（押し始めた要素がキャプチャ中）。
	 */
	FORCEINLINE bool IsPressed() const noexcept
	{
		return m_bPressed;
	}
	/**
	 * ツールチップの文字（空は出さない）。
	 */
	FORCEINLINE const Toolbox::FString& GetTooltip() const noexcept
	{
		return m_Tooltip;
	}
	/**
	 * ツールチップの文字を設定する。
	 * @param Tooltip 文字。
	 */
	void SetTooltip(Toolbox::FString Tooltip);

	/**
	 * 検査の例外を宣言する（理由は検査結果に残る）。
	 * @param Allowance 例外の項目。
	 * @param Reason 理由。
	 */
	void DeclareCheckAllowance(EUiCheckAllowance Allowance, Toolbox::FString Reason);
	/**
	 * 宣言した例外か。
	 * @param Allowance 項目。
	 */
	FORCEINLINE bool HasCheckAllowance(EUiCheckAllowance Allowance) const noexcept
	{
		return (m_CheckAllowances & static_cast<Toolbox::uint8>(Allowance)) != 0;
	}
	/**
	 * 宣言した例外の理由。
	 */
	FORCEINLINE const Toolbox::FString& GetCheckAllowanceReason() const noexcept
	{
		return m_CheckReason;
	}

	/**
	 * 寸法の再計算を要求する（祖先にも伝える）。
	 */
	void InvalidateMeasure() noexcept;
	/**
	 * 配置のやり直しを要求する（寸法は変わらない）。
	 */
	void InvalidateArrange() noexcept;
	/**
	 * 見た目の解決のやり直しを要求する。
	 */
	void InvalidateStyle() noexcept;
	/**
	 * 接続期間に結び付けた購読・後始末。切断時に解放する。
	 */
	FORCEINLINE FUiScope& GetAttachScope() noexcept
	{
		return m_AttachScope;
	}
	/**
	 * 毎フレームの更新（OnUpdate）を受けるかを設定する。
	 * @param bWants 受けるか。
	 */
	void SetWantsUpdate(bool bWants);
	/**
	 * 毎フレームの更新を受けるか。
	 */
	FORCEINLINE bool WantsUpdate() const noexcept
	{
		return m_bWantsUpdate;
	}
	/**
	 * 最初の接続を終えたか。
	 */
	FORCEINLINE bool HasAttachedOnce() const noexcept
	{
		return m_bAttachedOnce;
	}

	/**
	 * 破棄を要求した要素・破棄中のルートの要素は参照で解決しない。
	 */
	bool IsHandleAccessible_Internal() const noexcept override;
	/**
	 * 基盤の呼出し: 寸法の計算。
	 */
	FORCEINLINE FUiSize Measure_Internal(FUiLayoutContext& Context, FUiSize Available)
	{
		return OnMeasure(Context, Available);
	}
	/**
	 * 基盤の呼出し: 子の配置。
	 */
	FORCEINLINE void Arrange_Internal(FUiLayoutContext& Context, const FUiRect& Content)
	{
		OnArrange(Context, Content);
	}
	/**
	 * 基盤の呼出し: 描画。
	 */
	FORCEINLINE void Draw_Internal(FUiDrawContext& Context) const
	{
		OnDraw(Context);
	}
	/**
	 * 基盤の呼出し: 子の後の描画。
	 */
	FORCEINLINE void DrawOverlay_Internal(FUiDrawContext& Context) const
	{
		OnDrawOverlay(Context);
	}
	/**
	 * 基盤の呼出し: 毎フレームの更新。
	 */
	/**
	 * レイアウト前の構造準備。更新対象だけに呼ぶ（仮想行の再利用など）。
	 */
	void PrepareLayout_Internal()
	{
		OnPrepareLayout();
	}
	FORCEINLINE void Update_Internal(const FUiUpdateContext& Context)
	{
		OnUpdate(Context);
	}
	/**
	 * 基盤の呼出し: ポインターの出来事。
	 */
	FORCEINLINE void Pointer_Internal(FUiPointerEvent& Event)
	{
		OnPointerEvent(Event);
	}
	/**
	 * 基盤の呼出し: 操作の出来事。
	 */
	FORCEINLINE void Navigation_Internal(FUiNavigationEvent& Event)
	{
		OnNavigationEvent(Event);
	}
	/**
	 * 基盤の呼出し: 見た目の解決後。
	 */
	FORCEINLINE void StyleResolved_Internal()
	{
		OnStyleResolved();
	}
	/**
	 * 基盤の呼出し: 状態（ポインター・押下・フォーカス・有効）の変化。
	 */
	FORCEINLINE void StateChanged_Internal()
	{
		OnStateChanged();
	}

protected:
	DUiElement() = default;
	/**
	 * 最初の接続の直前に一度だけ呼ぶ。子の構造をここで作る。
	 */
	virtual void OnFirstAttach()
	{
	}
	/**
	 * 接続のたびに呼ぶ（初回はOnFirstAttachの後）。購読をGetAttachScopeへ登録し、現在値を反映する。
	 */
	virtual void OnAttach()
	{
	}
	/**
	 * 切断のたびに呼ぶ（子の後）。この後に接続期間の購読・後始末を解放する。
	 */
	virtual void OnDetach() noexcept
	{
	}
	/**
	 * 内容の大きさ（内側の余白を除く）を求める。既定は子を並べ方に従って測る。
	 * @param Context レイアウトの窓口。
	 * @param Available 内容に使える大きさ（UiUnboundedは上限なし）。
	 */
	virtual FUiSize OnMeasure(FUiLayoutContext& Context, FUiSize Available);
	/**
	 * 子を内容範囲へ配置する。既定は並べ方に従う。
	 * @param Context レイアウトの窓口。
	 * @param Content 内容範囲（ルートの論理座標）。
	 */
	virtual void OnArrange(FUiLayoutContext& Context, const FUiRect& Content);
	/**
	 * 子より先に描く。既定は背景と枠。
	 * @param Context 描画の窓口。
	 */
	virtual void OnDraw(FUiDrawContext& Context) const;
	/**
	 * 子の後に描く（フォーカスの枠等）。
	 * @param Context 描画の窓口。
	 */
	virtual void OnDrawOverlay(FUiDrawContext& Context) const;
	/**
	 * 毎フレームの更新（SetWantsUpdate(true)の要素だけ）。
	 * @param Context 更新の情報。
	 */
	/**
	 * 測定前の構造準備。描画や時間の更新は行わない。
	 */
	virtual void OnPrepareLayout()
	{
	}
	virtual void OnUpdate(const FUiUpdateContext& Context)
	{
		(void)Context;
	}
	/**
	 * ポインターの出来事（処理したらbHandled）。
	 * @param Event 出来事。
	 */
	virtual void OnPointerEvent(FUiPointerEvent& Event)
	{
		(void)Event;
	}
	/**
	 * 操作の出来事（処理したらbHandled）。
	 * @param Event 出来事。
	 */
	virtual void OnNavigationEvent(FUiNavigationEvent& Event)
	{
		(void)Event;
	}
	/**
	 * 見た目を解決した後（文字の大きさ等の変化に合わせる）。
	 */
	virtual void OnStyleResolved()
	{
	}
	/**
	 * 状態（ポインター・押下・フォーカス・有効）が変わった後。
	 */
	virtual void OnStateChanged()
	{
	}
	/**
	 * 同じルートに子を作って末尾へ加える。
	 * @param Args 子のコンストラクタ引数。
	 */
	template <typename T, typename... TArgs> TUiRef<T> CreateChild(TArgs&&... Args);
	/**
	 * 作成済みの要素を子として末尾へ加える。
	 * @param Child 子。
	 */
	TResult<void> AddChild(const TUiRef<DUiElement>& Child);
	/**
	 * 押下の状態を設定する（押下の部品が使う）。
	 * @param bPressed 押されているか。
	 */
	void SetPressed_Internal(bool bPressed);
	/**
	 * ポインターをこの要素へ固定する（押下・ドラッグ）。成功したらtrue。
	 */
	bool CapturePointer();
	/**
	 * ポインターの固定を解く。
	 */
	void ReleasePointer() noexcept;
	/**
	 * ポインターを固定しているか。
	 */
	bool HasPointerCapture() const noexcept;

private:
	friend class FUiRoot;
	friend struct Detail::FUiRootState;
	friend class Detail::FUiLayoutEngine;
	friend class Detail::FUiTree;
	friend class Detail::FUiInputDispatcher;
	friend class Detail::FUiFocusManager;
	friend class Detail::FUiTooltipService;
	friend class Detail::FUiDrawBuilder;
	/**
	 * 所有するルート。
	 */
	FUiRoot* m_pRoot = nullptr;
	/**
	 * 自身への参照。
	 */
	TUiRef<DUiElement> m_Self;
	/**
	 * 親。
	 */
	DUiElement* m_pParent = nullptr;
	/**
	 * 子（並び順、所有はルートの格納領域）。
	 */
	Toolbox::TVector<DUiElement*> m_Children;
	/**
	 * 番号。
	 */
	Toolbox::uint64 m_Id = 0;
	/**
	 * 名前。
	 */
	Toolbox::FString m_Name;
	/**
	 * スタイルID。
	 */
	Toolbox::FString m_StyleId;
	/**
	 * 要素だけの上書き。
	 */
	FUiStylePatch m_StyleOverride;
	/**
	 * スタイルIDの定義（ルートのスタイルから解決した写し）。
	 */
	FUiStyleSet m_StyleSet;
	/**
	 * 状態を反映した見た目。
	 */
	FUiStyle m_Style;
	/**
	 * 見た目を解決し直すか。
	 */
	bool m_bStyleDirty = true;
	/**
	 * レイアウトの指定。
	 */
	FUiLayoutParams m_Layout;
	/**
	 * 内側の余白を明示したか。
	 */
	bool m_bPaddingExplicit = false;
	/**
	 * 子の並べ方。
	 */
	EUiStackMode m_StackMode = EUiStackMode::Overlay;
	/**
	 * 子の間隔。
	 */
	Toolbox::f32 m_Gap = 0;
	/**
	 * 配置後の矩形。
	 */
	FUiRect m_Rect;
	/**
	 * 描画・入力のクリップ。
	 */
	FUiRect m_ClipRect;
	/**
	 * 望む大きさ（余白を除く）。
	 */
	FUiSize m_DesiredSize;
	/**
	 * 最後に寸法を計算したときの使える大きさ。
	 */
	FUiSize m_LastAvailable{-1, -1};
	/**
	 * 最後に配置したときの割当範囲。
	 */
	FUiRect m_LastSlot{-1, -1, -1, -1};
	/**
	 * 寸法の再計算が必要か。
	 */
	bool m_bMeasureDirty = true;
	/**
	 * 配置のやり直しが必要か。
	 */
	bool m_bArrangeDirty = true;
	/**
	 * 寸法の計算に失敗したか（不正な値。描画・入力の対象外）。
	 */
	bool m_bLayoutInvalid = false;
	/**
	 * 子を自身の矩形でクリップするか。
	 */
	bool m_bClipChildren = false;
	/**
	 * 表示の状態。
	 */
	EUiVisibility m_Visibility = EUiVisibility::Visible;
	/**
	 * 有効か。
	 */
	bool m_bEnabled = true;
	/**
	 * 当たり判定の方針。
	 */
	EUiHitTest m_HitTest = EUiHitTest::ChildrenOnly;
	/**
	 * フォーカスを受けるか。
	 */
	bool m_bFocusable = false;
	/**
	 * フォーカスがあるか。
	 */
	bool m_bFocused = false;
	/**
	 * ポインターが上にあるか。
	 */
	bool m_bHovered = false;
	/**
	 * 押されているか。
	 */
	bool m_bPressed = false;
	/**
	 * ツールチップ。
	 */
	Toolbox::FString m_Tooltip;
	/**
	 * 検査の例外。
	 */
	Toolbox::uint8 m_CheckAllowances = 0;
	/**
	 * 検査の例外の理由。
	 */
	Toolbox::FString m_CheckReason;
	/**
	 * 接続の状態。
	 */
	EUiAttachState m_AttachState = EUiAttachState::Detached;
	/**
	 * 最初の接続を終えたか。
	 */
	bool m_bAttachedOnce = false;
	/**
	 * 破棄を要求したか。
	 */
	bool m_bDestroyRequested = false;
	/**
	 * 毎フレームの更新を受けるか。
	 */
	bool m_bWantsUpdate = false;
	/**
	 * 接続期間の購読・後始末。
	 */
	FUiScope m_AttachScope;
};
} // namespace Dxf
#endif
