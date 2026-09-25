// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_ROOT_H
#define DXF_UI_ROOT_H
#include "Dxf/UiDrawList.h"
#include "Dxf/UiElement.h"
#include "Dxf/UiInput.h"
#include "Dxf/UiLayoutContext.h"
#include "Dxf/UiPost.h"
#include "Dxf/UiSignal.h"
#include "Dxf/UiStyleSheet.h"
#include "Dxf/UiSurface.h"
#include "Toolbox/UniquePtr.h"
namespace Dxf
{
namespace Detail
{
struct FUiRootState;
}

/**
 * ルートの上限（過大な木を正常な空表示と区別して拒否する）。
 */
struct FUiRootLimits
{
	/**
	 * 要素の数の上限（破棄を要求して未解放のものを含む）。
	 */
	Toolbox::size_t MaxElements = 200000;
	/**
	 * 木の深さの上限（重なり領域の直下を1とする）。
	 */
	Toolbox::size_t MaxDepth = 64;
};

/**
 * ルートの設定。
 */
struct FUiRootSettings
{
	/**
	 * フォントの取得と文字の計測（なければ文字の大きさは0で描かない）。ルートより長く生存すること。
	 */
	IUiTextService* Text = nullptr;
	/**
	 * スタイル（なければ組込みの既定値だけ）。
	 */
	Toolbox::TSharedPtr<const FUiStyleSheet> Styles;
	/**
	 * 操作の繰り返し・遅延。
	 */
	FUiNavigationSettings Navigation;
	/**
	 * 上限。
	 */
	FUiRootLimits Limits;
	/**
	 * 診断の名前。
	 */
	Toolbox::FString Name = "UiRoot";
};

/**
 * ルートの状態の数（診断・性能測定・試験用）。
 */
struct FUiRootStats
{
	/**
	 * 保持している要素の数（未接続を含む）。
	 */
	Toolbox::size_t Elements = 0;
	/**
	 * 接続済みの要素の数。
	 */
	Toolbox::size_t AttachedElements = 0;
	/**
	 * 破棄を要求して未解放の要素の数。
	 */
	Toolbox::size_t PendingDestroy = 0;
	/**
	 * 接続期間の購読の合計。
	 */
	Toolbox::size_t AttachSubscriptions = 0;
	/**
	 * 毎フレームの更新を受ける要素の数。
	 */
	Toolbox::size_t UpdatingElements = 0;
	/**
	 * レイアウトの回数。
	 */
	FUiLayoutStats Layout;
	/**
	 * 最後の描画で記録した命令の数。
	 */
	Toolbox::size_t DrawItems = 0;
	/**
	 * 最後のレイアウトの失敗（寸法の不正・深さの超過など）の数。
	 */
	Toolbox::size_t LayoutErrors = 0;
};

/**
 * UIの木の窓口。要素の作成・所有、重なり領域、レイアウト、入力の仲介、フォーカス・キャプチャ・ツールチップ、描画の記録をまとめる。
 * 実装はそれぞれ独立した内部の部品（木・レイアウト・入力・フォーカス・ツールチップ・描画）に分けている。
 * 所有スレッドだけで操作する（Postだけは任意のスレッドから呼べる）。
 */
class FUiRoot
{
public:
	/**
	 * @param Settings 設定。
	 */
	explicit FUiRoot(FUiRootSettings Settings = {});
	/**
	 * 接続済みの要素を切断（OnDetach）してから、すべての要素を解放する。
	 */
	~FUiRoot();
	FUiRoot(const FUiRoot&) = delete;
	FUiRoot& operator=(const FUiRoot&) = delete;

	/**
	 * 未接続の要素を作る。上限を超えると例外。
	 * @param Args コンストラクタ引数。
	 */
	template <typename T, typename... TArgs> TUiRef<T> Create(TArgs&&... Args)
	{
		static_assert(Toolbox::IsBaseOf<DUiElement, T>);
		auto Element = Toolbox::MakeUnique<T>(Toolbox::Forward<TArgs>(Args)...);
		return Register_Internal(Toolbox::TUniquePtr<DUiElement>(Element.Release())).template Cast<T>();
	}
	/**
	 * 重なり領域の直下へ加える。接続済みの領域なら、その場で子の木を接続する（イベント中は安全な境界まで遅らせる）。
	 * @param Layer 重なり領域。
	 * @param Element 未接続で親のない要素。
	 */
	TResult<void> AddToLayer(EUiLayer Layer, const TUiRef<DUiElement>& Element);
	/**
	 * 子として加える。循環・二重の親・破棄済み・別のルート・上限の超過は失敗する。
	 * @param Parent 親。
	 * @param Child 親のない要素。
	 * @param Index 挿入位置（範囲外は末尾）。
	 */
	TResult<void> AddChild(const TUiRef<DUiElement>& Parent, const TUiRef<DUiElement>& Child,
	                       Toolbox::size_t Index = Toolbox::TNumericLimits<Toolbox::size_t>::Max());
	/**
	 * 親から取り外す（要素は破棄しない。接続済みなら子の木を切断する）。
	 * @param Element 要素。
	 */
	TResult<void> Remove(const TUiRef<DUiElement>& Element);
	/**
	 * 要素と子孫の破棄を要求する。参照はすぐに解決しなくなり、切断と解放は安全な境界で行う。
	 * @param Element 要素。
	 */
	bool Destroy(const TUiRef<DUiElement>& Element) noexcept;
	/**
	 * 重なり領域の直下の入れ物（常に接続済み）。
	 * @param Layer 重なり領域。
	 */
	DUiElement& GetLayer(EUiLayer Layer) noexcept;

	/**
	 * 表示面を設定する（倍率・大きさが変われば次のレイアウトで全体を計算し直す）。
	 * @param Surface 表示面。
	 */
	void SetSurface(const FUiSurface& Surface);
	/**
	 * 表示面。
	 */
	const FUiSurface& GetSurface() const noexcept;
	/**
	 * 変更のあった部分だけ寸法と配置を計算し直す。表示できない面では何もしない。
	 */
	TResult<void> Layout();
	/**
	 * 一フレームの入力を処理する（ヒットテスト・ポインター・キャプチャ・フォーカス・操作）。1フレームに1回呼ぶ。
	 * @param Frame 入力。
	 */
	TResult<FUiInputResult> ProcessInput(const FUiInputFrame& Frame);
	/**
	 * 毎フレームの更新（要素のOnUpdate・ツールチップ・投函された処理）。秒数はSceneのポーズに依らない。
	 * @param DeltaSeconds 前回からの秒数。
	 */
	TResult<void> Update(Toolbox::f64 DeltaSeconds);
	/**
	 * 重なり領域の順に描画を記録する。
	 * @param List 記録先（追記する）。
	 */
	TResult<void> BuildDrawList(FUiDrawList& List);

	/**
	 * 最前面の要素（当たり判定の方針・クリップ・重なり順・Modalを反映）。なければnullptr。
	 * @param Position ルートの論理座標。
	 */
	DUiElement* HitTest(FVector2 Position);
	/**
	 * フォーカスを移す。受けられない要素・Modalの外の要素は失敗する。
	 * @param Element 要素（空はフォーカスを外す）。
	 */
	bool SetFocus(const TUiRef<DUiElement>& Element);
	/**
	 * フォーカスのある要素。
	 */
	DUiElement* GetFocused() const noexcept;
	/**
	 * ポインターを固定している要素。
	 */
	DUiElement* GetCaptured() const noexcept;
	/**
	 * ポインターが上にある最前面の要素。
	 */
	DUiElement* GetHovered() const noexcept;
	/**
	 * 表示中のツールチップの対象（なければnullptr）。
	 */
	DUiElement* GetTooltipTarget() const noexcept;
	/**
	 * 最前面のModal（なければnullptr）。
	 */
	DUiElement* GetTopModal() const noexcept;
	/**
	 * Modalとして扱う要素を登録する（Popup領域の要素）。閉じるまで背後の操作を遮る。
	 * @param Element 要素。
	 */
	void PushModal_Internal(const TUiRef<DUiElement>& Element);
	/**
	 * Modalの登録を外し、開く前のフォーカスへ戻す。
	 * @param Element 要素。
	 */
	void PopModal_Internal(const TUiRef<DUiElement>& Element) noexcept;

	/**
	 * スタイルを差し替える（要素の状態・フォーカス・スクロール位置は保ち、見た目だけ解決し直す）。
	 * @param Styles 新しいスタイル（空は組込みの既定値）。
	 */
	void SetStyleSheet(Toolbox::TSharedPtr<const FUiStyleSheet> Styles);
	/**
	 * 現在のスタイル。
	 */
	const FUiStyleSheet& GetStyleSheet() const noexcept;
	/**
	 * 文字の窓口。
	 */
	IUiTextService* GetTextService() const noexcept;
	/**
	 * 所有スレッドで次の更新の境界に実行する処理を投函する（任意のスレッドから呼べる）。
	 * ルートが破棄されると実行しない。
	 * @param Callback 処理。
	 */
	void Post(Toolbox::TFunction<void()> Callback);
	/**
	 * 投函の窓口の写し（非同期処理がルートより長く生きる場合に使う。ルートの破棄後は投函しても実行しない）。
	 */
	FUiPostHandle GetPostHandle() const noexcept;
	/**
	 * 操作の「戻る」を誰も処理しなかったときの通知。
	 */
	TUiSignal<>& OnCancelRequested() noexcept;
	/**
	 * 状態の数。
	 */
	FUiRootStats GetStats() const noexcept;
	/**
	 * 最後のレイアウトの失敗の内容（なければ空）。
	 */
	const Toolbox::TVector<Toolbox::FString>& GetLayoutErrors() const noexcept;
	/**
	 * 経過秒数（Updateで進む）。
	 */
	Toolbox::f64 GetElapsedSeconds() const noexcept;
	/**
	 * 設定。
	 */
	const FUiRootSettings& GetSettings() const noexcept;

	/**
	 * 基盤の呼出し: 要素を登録する。
	 * @param Element 要素。
	 */
	TUiRef<DUiElement> Register_Internal(Toolbox::TUniquePtr<DUiElement> Element);
	/**
	 * 基盤の呼出し: 内部状態。
	 */
	FORCEINLINE Detail::FUiRootState& GetState_Internal() noexcept
	{
		return *m_pState;
	}

private:
	/**
	 * 内部状態（木・レイアウト・入力・フォーカス・ツールチップ・描画・投函）。
	 */
	Toolbox::TUniquePtr<Detail::FUiRootState> m_pState;
};

// 同じルートに子を作って末尾へ加える。
template <typename T, typename... TArgs> TUiRef<T> DUiElement::CreateChild(TArgs&&... Args)
{
	TUiRef<T> Child = m_pRoot->template Create<T>(Toolbox::Forward<TArgs>(Args)...);
	auto Added = m_pRoot->AddChild(m_Self, Child.template Cast<DUiElement>());
	if (!Added)
	{
		m_pRoot->Destroy(Child.template Cast<DUiElement>());
		throw Toolbox::FException(Added.Error().Message.CStr());
	}
	return Child;
}
} // namespace Dxf
#endif
