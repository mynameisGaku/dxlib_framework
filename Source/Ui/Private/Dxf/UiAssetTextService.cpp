// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiAssetTextService.h"
namespace Dxf
{
// 資源管理で作る。
FUiAssetTextService::FUiAssetTextService(FAssetService& Assets, Toolbox::int32 Thickness, bool bAntialias,
                                         Toolbox::size_t MaxFonts)
    : m_pAssets(&Assets), m_Thickness(Thickness), m_bAntialias(bAntialias), m_MaxFonts(MaxFonts < 1 ? 1 : MaxFonts)
{
}
// フォントを取得する。
TResult<FFont> FUiAssetTextService::ResolveFont(const FUiFontKey& Key)
{
	for (Toolbox::size_t Index = 0; Index < m_Fonts.Size(); ++Index)
	{
		if (m_Fonts[Index].Key == Key && m_Fonts[Index].Font.IsValid())
		{
			// 最近使った順の末尾へ移す。
			FEntry Entry = Toolbox::Move(m_Fonts[Index]);
			m_Fonts.Erase(m_Fonts.Begin() + Index);
			m_Fonts.PushBack(Toolbox::Move(Entry));
			return TResult<FFont>::Success(m_Fonts.Back().Font);
		}
	}
	FFontOptions Options;
	Options.Family = Key.Family;
	Options.Size = Key.PixelSize;
	Options.Thickness = m_Thickness;
	Options.bAntialias = m_bAntialias;
	auto Loaded = m_pAssets->LoadFont(Options);
	if (!Loaded)
	{
		return Loaded;
	}
	while (m_Fonts.Size() >= m_MaxFonts)
	{
		m_Fonts.Erase(m_Fonts.Begin());
	}
	m_Fonts.PushBack({Key, Loaded.Value()});
	return Loaded;
}
// 一行の描画幅。
TResult<Toolbox::int32> FUiAssetTextService::MeasureWidth(const FFont& Font, const Toolbox::FString& Text)
{
	++m_Measures;
	return m_pAssets->MeasureTextWidth(Font, Text);
}
// 行の送り。
TResult<Toolbox::int32> FUiAssetTextService::GetLineHeight(const FFont& Font)
{
	return m_pAssets->GetFontLineHeight(Font);
}
} // namespace Dxf
