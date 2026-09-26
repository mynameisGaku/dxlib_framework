// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_BENCHMARK_TIMED_TEXT_SERVICE_H
#define DXF_UI_BENCHMARK_TIMED_TEXT_SERVICE_H
#include "Dxf/UiTextService.h"
#include "Toolbox/Platform.h"
namespace Dxf::UiBenchmark
{
/**
 * 実字体の文字の窓口へ委譲し、計測の回数とCPU時間を数える（測定用）。
 */
class FTimedTextService final : public IUiTextService
{
public:
	/**
	 * @param Inner 委譲先（実字体）。
	 */
	explicit FTimedTextService(IUiTextService& Inner) : m_Inner(Inner)
	{
	}
	TResult<FFont> ResolveFont(const FUiFontKey& Key) override
	{
		return m_Inner.ResolveFont(Key);
	}
	TResult<Toolbox::int32> MeasureWidth(const FFont& Font, const Toolbox::FString& Text) override
	{
		const Toolbox::uint64 Begin = Toolbox::MonotonicNanoseconds();
		auto Result = m_Inner.MeasureWidth(Font, Text);
		m_Nanoseconds += Toolbox::MonotonicNanoseconds() - Begin;
		++m_Measures;
		return Result;
	}
	TResult<Toolbox::int32> GetLineHeight(const FFont& Font) override
	{
		return m_Inner.GetLineHeight(Font);
	}
	/**
	 * 計測の回数と累計のCPU時間（ナノ秒）。
	 */
	FORCEINLINE Toolbox::uint64 GetMeasures() const noexcept
	{
		return m_Measures;
	}
	FORCEINLINE Toolbox::uint64 GetNanoseconds() const noexcept
	{
		return m_Nanoseconds;
	}

private:
	/**
	 * 委譲先。
	 */
	IUiTextService& m_Inner;
	/**
	 * 計測の回数と累計のCPU時間。
	 */
	Toolbox::uint64 m_Measures = 0;
	Toolbox::uint64 m_Nanoseconds = 0;
};
} // namespace Dxf::UiBenchmark
#endif
