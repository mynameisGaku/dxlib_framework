// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_LAYOUT_ENGINE_H
#define DXF_UI_LAYOUT_ENGINE_H
#include "Dxf/UiElement.h"
#include "Dxf/UiLayoutContext.h"
namespace Dxf::Detail
{
struct FUiRootState;

/**
 * 寸法の計算（測定）と配置を分けて行い、変更のあった部分だけを計算し直す。
 * 測定: 子から親へ望む大きさを求める。配置: 親から子へ矩形とクリップを決める。
 */
class FUiLayoutEngine
{
public:
	/**
	 * 全重なり領域のレイアウトを行う（変更がなければ何もしない）。
	 * @param State ルートの状態。
	 */
	TResult<void> Run(FUiRootState& State);
	/**
	 * 子の望む大きさ（余白を含む）。
	 * @param Context 窓口。
	 * @param Element 要素。
	 * @param Available 使える大きさ。
	 */
	FUiSize MeasureChild(FUiLayoutContext& Context, DUiElement& Element, FUiSize Available);
	/**
	 * 子を割当範囲へ配置する。
	 * @param Context 窓口。
	 * @param Element 要素。
	 * @param Slot 割当範囲。
	 */
	void ArrangeChild(FUiLayoutContext& Context, DUiElement& Element, const FUiRect& Slot);
	/**
	 * 並べ方に従って子を測る（既定のOnMeasure）。
	 * @param Context 窓口。
	 * @param Element 入れ物。
	 * @param Available 内容に使える大きさ。
	 */
	static FUiSize MeasureStack(FUiLayoutContext& Context, DUiElement& Element, FUiSize Available);
	/**
	 * 並べ方に従って子を配置する（既定のOnArrange）。
	 * @param Context 窓口。
	 * @param Element 入れ物。
	 * @param Content 内容範囲。
	 */
	static void ArrangeStack(FUiLayoutContext& Context, DUiElement& Element, const FUiRect& Content);
	/**
	 * 回数の記録。
	 */
	FORCEINLINE const FUiLayoutStats& GetStats() const noexcept
	{
		return m_Stats;
	}
	/**
	 * 回数の記録（文字の計測の回数の加算に使う）。
	 */
	FORCEINLINE FUiLayoutStats& EditStats() noexcept
	{
		return m_Stats;
	}
	/**
	 * 最後のレイアウトの失敗の内容。
	 */
	FORCEINLINE const Toolbox::TVector<Toolbox::FString>& GetErrors() const noexcept
	{
		return m_Errors;
	}
	/**
	 * 表示面が変わったものとして、次のレイアウトで全体を計算し直す。
	 */
	FORCEINLINE void ForceFullLayout() noexcept
	{
		m_bForceFull = true;
	}

private:
	/**
	 * 要素の失敗を記録する。
	 * @param Element 要素。
	 * @param Message 内容。
	 */
	void RecordError_Internal(DUiElement& Element, const char* Message);
	/**
	 * 子孫の寸法と見た目を無効にする。
	 * @param Element 要素。
	 */
	static void InvalidateSubtree_Internal(DUiElement& Element) noexcept;
	/**
	 * 実行中のルートの状態。
	 */
	FUiRootState* m_pState = nullptr;
	/**
	 * 前回の表示面。
	 */
	FUiSurface m_LastSurface;
	/**
	 * 次のレイアウトで全体を計算し直すか。
	 */
	bool m_bForceFull = true;
	/**
	 * 配置中の、子へ渡すクリップ。
	 */
	FUiRect m_CurrentClip;
	/**
	 * 回数。
	 */
	FUiLayoutStats m_Stats;
	/**
	 * 失敗の内容。
	 */
	Toolbox::TVector<Toolbox::FString> m_Errors;
	/**
	 * 今回の実行で計算・配置をしたか。
	 */
	bool m_bWorked = false;
};
} // namespace Dxf::Detail
#endif
