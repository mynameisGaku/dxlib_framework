// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_ASSET_TEXT_SERVICE_H
#define DXF_UI_ASSET_TEXT_SERVICE_H
#include "Dxf/AssetService.h"
#include "Dxf/UiTextService.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 資源管理（FAssetService）のフォントで文字を計測する窓口。描画と同じフォント資源・画素の大きさで計測する。
 * 使ったフォントを保持し（最大の数を超えたら古いものから手放す）、ルートが描く間は解放しない。
 */
class FUiAssetTextService final : public IUiTextService
{
public:
	/**
	 * @param Assets 資源管理（この窓口より長く生存すること）。
	 * @param Thickness フォントの太さ。
	 * @param bAntialias アンチエイリアスを使うか。
	 * @param MaxFonts 保持するフォントの最大数。
	 */
	explicit FUiAssetTextService(FAssetService& Assets, Toolbox::int32 Thickness = 4, bool bAntialias = true,
	                             Toolbox::size_t MaxFonts = 32);
	/**
	 * 字体名と画素の大きさのフォントを取得する。
	 * @param Key フォントの識別。
	 */
	TResult<FFont> ResolveFont(const FUiFontKey& Key) override;
	/**
	 * 一行の描画幅（画素）。
	 * @param Font フォント。
	 * @param Text 文字列。
	 */
	TResult<Toolbox::int32> MeasureWidth(const FFont& Font, const Toolbox::FString& Text) override;
	/**
	 * 行の送り（画素）。
	 * @param Font フォント。
	 */
	TResult<Toolbox::int32> GetLineHeight(const FFont& Font) override;
	/**
	 * 計測の回数。
	 */
	FORCEINLINE Toolbox::uint64 GetMeasureCount() const noexcept
	{
		return m_Measures;
	}
	/**
	 * 保持しているフォントの数。
	 */
	FORCEINLINE Toolbox::size_t GetFontCount() const noexcept
	{
		return m_Fonts.Size();
	}

private:
	/**
	 * 保持するフォント。
	 */
	struct FEntry
	{
		/**
		 * 識別。
		 */
		FUiFontKey Key;
		/**
		 * フォント。
		 */
		FFont Font;
	};
	/**
	 * 資源管理。
	 */
	FAssetService* m_pAssets;
	/**
	 * 太さ。
	 */
	Toolbox::int32 m_Thickness;
	/**
	 * アンチエイリアス。
	 */
	bool m_bAntialias;
	/**
	 * 保持の最大数。
	 */
	Toolbox::size_t m_MaxFonts;
	/**
	 * 保持するフォント（最近使った順に後ろ）。
	 */
	Toolbox::TVector<FEntry> m_Fonts;
	/**
	 * 計測の回数。
	 */
	Toolbox::uint64 m_Measures = 0;
};
} // namespace Dxf
#endif
