// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiTextLayout.h"
#include "Dxf/Utf8.h"
namespace Dxf
{
namespace
{
// 省略記号（U+2026）。
constexpr const char* Ellipsis_Internal = "\xE2\x80\xA6";

// UTF-8の先頭バイトから、その文字のバイト数を求める（検証済みの文字列が前提）。
Toolbox::size_t CodepointLength_Internal(unsigned char Lead) noexcept
{
	if (Lead < 0x80)
	{
		return 1;
	}
	if (Lead < 0xE0)
	{
		return 2;
	}
	if (Lead < 0xF0)
	{
		return 3;
	}
	return 4;
}
// 末尾の空白を除いた文字列。
Toolbox::FString TrimRight_Internal(const Toolbox::FString& Text)
{
	Toolbox::size_t End = Text.Size();
	while (End > 0 && Text[End - 1] == ' ')
	{
		--End;
	}
	return Text.Substr(0, End);
}
// 文字の計測と回数の記録。
class FMeasurer
{
public:
	FMeasurer(IUiTextService& Service, const FFont& Font, FUiTextLayoutResult& Result)
	    : m_pService(&Service), m_pFont(&Font), m_pResult(&Result)
	{
	}
	// 一行の幅（画素）。
	Toolbox::int32 Width(const Toolbox::FString& Text)
	{
		if (Text.IsEmpty())
		{
			return 0;
		}
		++m_pResult->Measurements;
		auto Measured = m_pService->MeasureWidth(*m_pFont, Text);
		if (!Measured)
		{
			throw Toolbox::FException(Measured.Error().Message.CStr());
		}
		return Measured.Value();
	}

private:
	// 計測の窓口。
	IUiTextService* m_pService;
	// 計測に使うフォント。
	const FFont* m_pFont;
	// 回数を記録する結果。
	FUiTextLayoutResult* m_pResult;
};
// 行を最大幅に収まるよう末尾から削り、省略記号を付ける。
FUiTextLine Ellipsize_Internal(FMeasurer& Measure, const Toolbox::FString& Text, Toolbox::int32 MaxWidth)
{
	// 文字の境界（先頭からのバイト位置）。
	Toolbox::TVector<Toolbox::size_t> Boundaries;
	for (Toolbox::size_t Index = 0; Index < Text.Size();)
	{
		Boundaries.PushBack(Index);
		Index += CodepointLength_Internal(static_cast<unsigned char>(Text[Index]));
	}
	Boundaries.PushBack(Text.Size());
	// 省略記号を付けて収まる最長の接頭辞を二分探索する（幅は文字数に対して単調とみなす）。
	Toolbox::size_t Low = 0;
	Toolbox::size_t High = Boundaries.Size() - 1;
	while (Low < High)
	{
		const Toolbox::size_t Middle = (Low + High + 1) / 2;
		const Toolbox::FString Candidate = TrimRight_Internal(Text.Substr(0, Boundaries[Middle])) + Ellipsis_Internal;
		if (Measure.Width(Candidate) <= MaxWidth)
		{
			Low = Middle;
		}
		else
		{
			High = Middle - 1;
		}
	}
	FUiTextLine Line;
	Toolbox::FString Cut = TrimRight_Internal(Text.Substr(0, Boundaries[Low])) + Ellipsis_Internal;
	Line.Width = Measure.Width(Cut);
	Line.Text = FSharedText(Toolbox::Move(Cut));
	return Line;
}
// 折返しの単位（ASCIIの語と後続の空白、またはASCII以外の一文字と後続の空白）へ分ける。
Toolbox::TVector<Toolbox::FString> Tokens_Internal(const Toolbox::FString& Paragraph)
{
	Toolbox::TVector<Toolbox::FString> Tokens;
	Toolbox::size_t Index = 0;
	while (Index < Paragraph.Size())
	{
		const Toolbox::size_t Begin = Index;
		const auto Lead = static_cast<unsigned char>(Paragraph[Index]);
		if (Lead < 0x80 && Lead != ' ')
		{
			while (Index < Paragraph.Size() && static_cast<unsigned char>(Paragraph[Index]) < 0x80 &&
			       Paragraph[Index] != ' ')
			{
				++Index;
			}
		}
		else if (Lead >= 0x80)
		{
			Index += CodepointLength_Internal(Lead);
		}
		while (Index < Paragraph.Size() && Paragraph[Index] == ' ')
		{
			++Index;
		}
		Tokens.PushBack(Paragraph.Substr(Begin, Index - Begin));
	}
	return Tokens;
}
// 一段落を折り返して行へ加える。
void WrapParagraph_Internal(FMeasurer& Measure, const Toolbox::FString& Paragraph, Toolbox::int32 MaxWidth,
                            Toolbox::TVector<Toolbox::FString>& Lines)
{
	Toolbox::FString Line;
	for (const auto& Token : Tokens_Internal(Paragraph))
	{
		const Toolbox::FString Candidate = Line + Token;
		if (Measure.Width(TrimRight_Internal(Candidate)) <= MaxWidth)
		{
			Line = Candidate;
			continue;
		}
		if (!Line.IsEmpty())
		{
			Lines.PushBack(TrimRight_Internal(Line));
			Line = {};
			if (Measure.Width(TrimRight_Internal(Token)) <= MaxWidth)
			{
				Line = Token;
				continue;
			}
		}
		// 一語が幅を超える: 文字の境界で分ける（各行に少なくとも一文字）。
		for (Toolbox::size_t Index = 0; Index < Token.Size();)
		{
			const Toolbox::size_t Length = CodepointLength_Internal(static_cast<unsigned char>(Token[Index]));
			const Toolbox::FString Character = Token.Substr(Index, Length);
			const Toolbox::FString Next = Line + Character;
			if (!Line.IsEmpty() && Measure.Width(TrimRight_Internal(Next)) > MaxWidth)
			{
				Lines.PushBack(TrimRight_Internal(Line));
				Line = Character == " " ? Toolbox::FString() : Character;
			}
			else
			{
				Line = Next;
			}
			Index += Length;
		}
	}
	Lines.PushBack(TrimRight_Internal(Line));
}
} // namespace

// 文字列を行へ配置する。
TResult<FUiTextLayoutResult> LayoutUiText(IUiTextService& Service, const FFont& Font, const Toolbox::FString& Text,
                                          const FUiTextLayoutRequest& Request)
{
	if (!IsValidUtf8(Text))
	{
		return TResult<FUiTextLayoutResult>::Failure(EErrorCode::InvalidArgument, "UI text is not valid UTF-8");
	}
	FUiTextLayoutResult Result;
	auto LineHeight = Service.GetLineHeight(Font);
	if (!LineHeight)
	{
		return TResult<FUiTextLayoutResult>::Failure(LineHeight.Error());
	}
	if (LineHeight.Value() < 0)
	{
		return TResult<FUiTextLayoutResult>::Failure(EErrorCode::BackendFailure, "Negative line height");
	}
	Result.LineHeight = LineHeight.Value();
	try
	{
		FMeasurer Measure(Service, Font, Result);
		const bool bBounded = Request.MaxWidth >= 0;
		// 段落（明示した改行）ごとの行。
		Toolbox::TVector<Toolbox::FString> Raw;
		Toolbox::size_t Begin = 0;
		for (Toolbox::size_t Index = 0; Index <= Text.Size(); ++Index)
		{
			if (Index < Text.Size() && Text[Index] != '\n')
			{
				continue;
			}
			Toolbox::size_t End = Index;
			if (End > Begin && Text[End - 1] == '\r')
			{
				--End;
			}
			const Toolbox::FString Paragraph = Text.Substr(Begin, End - Begin);
			if (Request.Wrap == EUiTextWrap::Wrap && bBounded)
			{
				WrapParagraph_Internal(Measure, Paragraph, Request.MaxWidth, Raw);
			}
			else
			{
				Raw.PushBack(Paragraph);
			}
			Begin = Index + 1;
		}
		// 行数の上限。
		Toolbox::size_t Count = Raw.Size();
		bool bCutLines = false;
		if (Request.MaxLines > 0 && Count > Request.MaxLines)
		{
			Count = Request.MaxLines;
			bCutLines = true;
			Result.bTruncated = true;
		}
		for (Toolbox::size_t Index = 0; Index < Count; ++Index)
		{
			FUiTextLine Line;
			Line.Width = Measure.Width(Raw[Index]);
			const bool bLastCut = bCutLines && Index + 1 == Count;
			if (Request.Overflow == EUiTextOverflow::Ellipsis && bBounded &&
			    (Line.Width > Request.MaxWidth || bLastCut))
			{
				// 収まる最長の接頭辞に省略記号を付ける（行数で切った最後の行は、全体が収まっても記号を付ける）。
				Line = Ellipsize_Internal(Measure, Raw[Index], Request.MaxWidth);
				Result.bTruncated = true;
			}
			else
			{
				// 行の文字は一度だけ共有の所有へ移す。
				Line.Text = FSharedText(Toolbox::Move(Raw[Index]));
			}
			if (bBounded && Line.Width > Request.MaxWidth)
			{
				Result.bOverflowed = true;
			}
			Result.Width = Toolbox::Max(Result.Width, Line.Width);
			Result.Lines.PushBack(Toolbox::Move(Line));
		}
		Result.Height = static_cast<Toolbox::int32>(Result.Lines.Size()) * Result.LineHeight;
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<FUiTextLayoutResult>::Failure(EErrorCode::BackendFailure, Error.What());
	}
	return TResult<FUiTextLayoutResult>::Success(Toolbox::Move(Result));
}
} // namespace Dxf
