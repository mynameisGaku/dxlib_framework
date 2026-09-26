// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_BENCHMARK_SCENE_H
#define DXF_UI_BENCHMARK_SCENE_H
#include "BenchmarkText.h"
#include "Dxf/UiListView.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiRoot.h"
#include "Dxf/UiStyleResource.h"
#include "Toolbox/UniquePtr.h"
namespace Dxf::UiBenchmark
{
/**
 * 測定の系列。
 */
enum class EBenchmarkMode : Toolbox::int32
{
	/**
	 * 何も変えない。
	 */
	Static,
	/**
	 * 1個の文字だけを毎フレーム変える。
	 */
	Value,
	/**
	 * 1個の幅だけを毎フレーム変える。
	 */
	Layout,
	/**
	 * 一覧を毎フレームスクロールする。
	 */
	ListScroll,
	/**
	 * スタイルを毎フレーム再読込する。
	 */
	StyleReload,
	/**
	 * ルートを毎フレーム作り直す。
	 */
	RootRecreate,
	/**
	 * 系列の数。
	 */
	Count
};

/**
 * ラベルの文字と配置（既定は固定幅の文字の系列と同じ）。
 */
struct FBenchmarkLabel
{
	/**
	 * 初期の文字と、値の変更の系列で交互に設定する文字。
	 */
	const char* Initial = "value 0";
	const char* Alternate = "value 1";
	/**
	 * ラベルの幅・高さ（論理単位）と、折返し・省略。
	 */
	Toolbox::f32 Width = 52;
	Toolbox::f32 Height = 20;
	EUiTextWrap Wrap = EUiTextWrap::NoWrap;
	EUiTextOverflow Overflow = EUiTextOverflow::Visible;
};

/**
 * 測定用のUI（絶対位置のラベルの格子、または一覧）。系列ごとの毎フレームの変更を行う。
 */
class FBenchmarkScene
{
public:
	/**
	 * @param Text 文字の窓口（字体と計測）。
	 * @param Mode 系列。
	 * @param Count ラベルの数、または一覧の項目数。
	 */
	FBenchmarkScene(IUiTextService& Text, EBenchmarkMode Mode, Toolbox::size_t Count, FBenchmarkLabel Label = {});
	/**
	 * 系列ごとの変更を行う。
	 * @param Frame フレームの番号。
	 */
	void Mutate(Toolbox::int32 Frame);
	/**
	 * ルート。
	 */
	FORCEINLINE FUiRoot& GetRoot() noexcept
	{
		return *m_pRoot;
	}
	/**
	 * 一覧の実体化した行の数（一覧でなければ0）。
	 */
	Toolbox::size_t GetRows() const noexcept;

private:
	/**
	 * ルートと部品を作る。
	 */
	void Build_Internal();
	/**
	 * 文字の窓口。
	 */
	IUiTextService& m_Text;
	/**
	 * ラベルの文字と配置。
	 */
	FBenchmarkLabel m_Label;
	/**
	 * 系列。
	 */
	EBenchmarkMode m_Mode;
	/**
	 * ラベルの数、または一覧の項目数。
	 */
	Toolbox::size_t m_Count;
	/**
	 * スタイルの資源（再読込の系列）。ルートより先に作り、後に破棄する。
	 */
	FUiStyleResource m_Styles;
	/**
	 * ルート。
	 */
	Toolbox::TUniquePtr<FUiRoot> m_pRoot;
	/**
	 * ラベル。
	 */
	Toolbox::TVector<TUiRef<DUiLabel>> m_Labels;
	/**
	 * 一覧。
	 */
	TUiRef<DUiListView> m_List;
};
} // namespace Dxf::UiBenchmark
#endif
