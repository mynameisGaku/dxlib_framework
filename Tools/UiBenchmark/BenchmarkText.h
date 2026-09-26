// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_BENCHMARK_TEXT_H
#define DXF_UI_BENCHMARK_TEXT_H
#include "Dxf/UiTextService.h"
namespace Dxf::UiBenchmark
{
/**
 * CPU測定用の一定幅字体。OS字体やGPUの費用ではない。
 */
class FBenchmarkText final : public IUiTextService
{
public:
	TResult<FFont> ResolveFont(const FUiFontKey&) override
	{
		return TResult<FFont>::Success(FFont{});
	}

	TResult<Toolbox::int32> MeasureWidth(const FFont&, const Toolbox::FString& Text) override
	{
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
};
} // namespace Dxf::UiBenchmark
#endif
