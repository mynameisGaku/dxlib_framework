// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_BENCHMARK_TEXT_H
#define DXF_UI_BENCHMARK_TEXT_H
#include "Dxf/UiTextService.h"
namespace Dxf::UiBenchmark
{
/**
 * CPU測定用の一定幅字体（1文字10、行の高さ20）。OS字体やGPUの費用ではない。
 * 字体は代替の資源で作った有効なもので、描画命令のキュー受付まで通る。
 */
class FBenchmarkText final : public IUiTextService
{
public:
	/**
	 * @param Font 解決に返す字体。
	 */
	explicit FBenchmarkText(FFont Font) : m_Font(Toolbox::Move(Font))
	{
	}
	TResult<FFont> ResolveFont(const FUiFontKey&) override
	{
		return TResult<FFont>::Success(m_Font);
	}

	TResult<Toolbox::int32> MeasureWidth(const FFont&, const Toolbox::FString& Text) override
	{
		++m_Measures;
		Toolbox::int32 Count = 0;
		for (char Byte : Text)
		{
			if ((static_cast<Toolbox::uint8>(Byte) & 0xC0) != 0x80)
			{
				++Count;
			}
		}
		return TResult<Toolbox::int32>::Success(Count * 10);
	}

	TResult<Toolbox::int32> GetLineHeight(const FFont&) override
	{
		return TResult<Toolbox::int32>::Success(20);
	}
	/**
	 * 文字の計測の回数。
	 */
	FORCEINLINE Toolbox::uint64 GetMeasures() const noexcept
	{
		return m_Measures;
	}

private:
	/**
	 * 解決に返す字体。
	 */
	FFont m_Font;
	/**
	 * 文字の計測の回数。
	 */
	Toolbox::uint64 m_Measures = 0;
};
} // namespace Dxf::UiBenchmark
#endif
