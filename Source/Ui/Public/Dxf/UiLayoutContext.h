// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_LAYOUT_CONTEXT_H
#define DXF_UI_LAYOUT_CONTEXT_H
#include "Dxf/UiSurface.h"
#include "Dxf/UiTextLayout.h"
namespace Dxf
{
class DUiElement;
namespace Detail
{
class FUiLayoutEngine;
}

/**
 * レイアウトの回数の記録（診断・性能測定用）。
 */
struct FUiLayoutStats
{
	/**
	 * レイアウトを実行した回数（変更がなく何もしなかった回を除く）。
	 */
	Toolbox::uint64 Passes = 0;
	/**
	 * 要素の寸法を計算した回数。
	 */
	Toolbox::uint64 Measures = 0;
	/**
	 * 要素を配置した回数。
	 */
	Toolbox::uint64 Arranges = 0;
	/**
	 * 文字の配置を計算した回数。
	 */
	Toolbox::uint64 TextLayouts = 0;
	/**
	 * 文字の幅を計測した回数。
	 */
	Toolbox::uint64 TextMeasurements = 0;
};

/**
 * 要素の寸法の計算と配置に渡す窓口。子の計算・配置はここから行う（寸法の記録と変更の管理のため）。
 */
class FUiLayoutContext
{
public:
	/**
	 * @param Engine レイアウトの実行者。
	 * @param Surface 表示面。
	 * @param Text 文字の窓口（なければ文字の大きさは0）。
	 * @param Stats 回数の記録先。
	 */
	FUiLayoutContext(Detail::FUiLayoutEngine& Engine, const FUiSurface& Surface, IUiTextService* Text,
	                 FUiLayoutStats& Stats) noexcept
	    : m_pEngine(&Engine), m_pSurface(&Surface), m_pText(Text), m_pStats(&Stats)
	{
	}
	FUiLayoutContext(const FUiLayoutContext&) = delete;
	FUiLayoutContext& operator=(const FUiLayoutContext&) = delete;
	/**
	 * 子の望む大きさを求める（余白を含む）。
	 * @param Child 子要素。
	 * @param Available 使える大きさ（UiUnboundedは上限なし）。
	 */
	FUiSize MeasureChild(DUiElement& Child, FUiSize Available);
	/**
	 * 子を割り当てた範囲へ配置する（余白と配置の指定を子の側で反映する）。
	 * @param Child 子要素。
	 * @param Slot 割り当てた範囲（ルートの論理座標）。
	 */
	void ArrangeChild(DUiElement& Child, const FUiRect& Slot);
	/**
	 * 字体を表示面の画素の大きさで解決する。
	 * @param Family 字体名。
	 * @param LogicalSize 論理単位の大きさ。
	 */
	TResult<FFont> ResolveFont(const Toolbox::FString& Family, Toolbox::f32 LogicalSize);
	/**
	 * 文字を配置する（回数を記録する）。
	 * @param Font 画素の大きさで解決したフォント。
	 * @param Text 文字列。
	 * @param Request 配置の指定（画素）。
	 */
	TResult<FUiTextLayoutResult> LayoutText(const FFont& Font, const Toolbox::FString& Text,
	                                        const FUiTextLayoutRequest& Request);
	/**
	 * 表示面。
	 */
	FORCEINLINE const FUiSurface& GetSurface() const noexcept
	{
		return *m_pSurface;
	}
	/**
	 * 文字の窓口があるか。
	 */
	FORCEINLINE bool HasTextService() const noexcept
	{
		return m_pText != nullptr;
	}
	/**
	 * 画素の長さを論理単位へ変える。
	 * @param Pixels 画素。
	 */
	FORCEINLINE Toolbox::f32 ToLogical(Toolbox::int32 Pixels) const noexcept
	{
		return static_cast<Toolbox::f32>(Pixels) / m_pSurface->GetScale();
	}
	/**
	 * 論理単位の長さを画素へ変える（切り捨て、UiUnbounded以上は-1）。
	 * @param Logical 論理単位。
	 */
	Toolbox::int32 ToPixels(Toolbox::f32 Logical) const noexcept;

private:
	/**
	 * レイアウトの実行者。
	 */
	Detail::FUiLayoutEngine* m_pEngine;
	/**
	 * 表示面。
	 */
	const FUiSurface* m_pSurface;
	/**
	 * 文字の窓口。
	 */
	IUiTextService* m_pText;
	/**
	 * 回数の記録先。
	 */
	FUiLayoutStats* m_pStats;
};
} // namespace Dxf
#endif
