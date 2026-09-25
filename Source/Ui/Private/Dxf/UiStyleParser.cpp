// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiStyleParser.h"
#include "Dxf/Utf8.h"
namespace Dxf
{
namespace
{
// 行の位置（資源名と行番号）。
struct FLocation
{
	// 資源名。
	Toolbox::FString Source;
	// 行番号（1から）。
	Toolbox::uint32 Line = 0;
};
// 読んだトークンの定義。
struct FRawToken
{
	// 名前。
	Toolbox::FString Name;
	// 値の文字列。
	Toolbox::FString Value;
	// 位置。
	FLocation Where;
};
// 読んだ定義の一行。
struct FRawEntry
{
	// キー（状態の接頭辞を含む）。
	Toolbox::FString Key;
	// 値の文字列。
	Toolbox::FString Value;
	// 位置。
	FLocation Where;
};
// 読んだスタイルの定義。
struct FRawStyle
{
	// スタイルID（既定の定義は空）。
	Toolbox::FString Id;
	// 既定の定義か。
	bool bDefault = false;
	// 定義の行。
	Toolbox::TVector<FRawEntry> Entries;
	// 位置。
	FLocation Where;
};
// 位置付きの失敗。
TResult<FUiStyleSheet> Error_Internal(const FLocation& Where, const Toolbox::FString& Key, const char* Message)
{
	Toolbox::FString Text = Where.Source + ":" + Toolbox::ToString(Where.Line) + ": ";
	if (!Key.IsEmpty())
	{
		Text += Toolbox::FString("key '") + Key + "': ";
	}
	Text += Message;
	return TResult<FUiStyleSheet>::Failure(EErrorCode::InvalidArgument, Text);
}
// 前後の空白を除く。
Toolbox::FString Trim_Internal(const Toolbox::FString& Text)
{
	Toolbox::size_t Begin = 0;
	Toolbox::size_t End = Text.Size();
	while (Begin < End && (Text[Begin] == ' ' || Text[Begin] == '\t'))
	{
		++Begin;
	}
	while (End > Begin && (Text[End - 1] == ' ' || Text[End - 1] == '\t' || Text[End - 1] == '\r'))
	{
		--End;
	}
	return Text.Substr(Begin, End - Begin);
}
// 先頭が一致するか。
bool StartsWith_Internal(const Toolbox::FString& Text, const char* Prefix)
{
	Toolbox::size_t Index = 0;
	for (; Prefix[Index] != '\0'; ++Index)
	{
		if (Index >= Text.Size() || Text[Index] != Prefix[Index])
		{
			return false;
		}
	}
	return true;
}
// 名前に使える文字か（英数字・._-）。
bool IsNameCharacter_Internal(char Character)
{
	return (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z') ||
	       (Character >= '0' && Character <= '9') || Character == '.' || Character == '_' || Character == '-';
}
// 名前として正しいか。
bool IsName_Internal(const Toolbox::FString& Text)
{
	if (Text.IsEmpty())
	{
		return false;
	}
	for (Toolbox::size_t Index = 0; Index < Text.Size(); ++Index)
	{
		if (!IsNameCharacter_Internal(Text[Index]))
		{
			return false;
		}
	}
	return true;
}
// 数値を読む（符号・整数部・小数部のみ。指数は受けない）。
bool ParseNumber_Internal(const Toolbox::FString& Text, Toolbox::f32& Out)
{
	Toolbox::size_t Index = 0;
	bool bNegative = false;
	if (Index < Text.Size() && (Text[Index] == '-' || Text[Index] == '+'))
	{
		bNegative = Text[Index] == '-';
		++Index;
	}
	Toolbox::f64 Value = 0;
	Toolbox::size_t Digits = 0;
	while (Index < Text.Size() && Text[Index] >= '0' && Text[Index] <= '9')
	{
		Value = Value * 10 + (Text[Index] - '0');
		++Index;
		++Digits;
	}
	if (Index < Text.Size() && Text[Index] == '.')
	{
		++Index;
		Toolbox::f64 Scale = 0.1;
		while (Index < Text.Size() && Text[Index] >= '0' && Text[Index] <= '9')
		{
			Value += (Text[Index] - '0') * Scale;
			Scale *= 0.1;
			++Index;
			++Digits;
		}
	}
	if (Digits == 0 || Index != Text.Size() || Digits > 18)
	{
		return false;
	}
	Out = static_cast<Toolbox::f32>(bNegative ? -Value : Value);
	return Toolbox::IsFinite(Out);
}
// 16進の一桁。
Toolbox::int32 Hex_Internal(char Character)
{
	if (Character >= '0' && Character <= '9')
	{
		return Character - '0';
	}
	if (Character >= 'a' && Character <= 'f')
	{
		return Character - 'a' + 10;
	}
	if (Character >= 'A' && Character <= 'F')
	{
		return Character - 'A' + 10;
	}
	return -1;
}
// 色を読む（#RRGGBB・#RRGGBBAA）。
bool ParseColor_Internal(const Toolbox::FString& Text, FColor& Out)
{
	if ((Text.Size() != 7 && Text.Size() != 9) || Text[0] != '#')
	{
		return false;
	}
	Toolbox::uint8 Channels[4] = {0, 0, 0, 255};
	for (Toolbox::size_t Channel = 0; Channel < (Text.Size() - 1) / 2; ++Channel)
	{
		const Toolbox::int32 High = Hex_Internal(Text[1 + Channel * 2]);
		const Toolbox::int32 Low = Hex_Internal(Text[2 + Channel * 2]);
		if (High < 0 || Low < 0)
		{
			return false;
		}
		Channels[Channel] = static_cast<Toolbox::uint8>(High * 16 + Low);
	}
	Out = {Channels[0], Channels[1], Channels[2], Channels[3]};
	return true;
}
// 空白で区切る。
Toolbox::TVector<Toolbox::FString> Split_Internal(const Toolbox::FString& Text)
{
	Toolbox::TVector<Toolbox::FString> Parts;
	Toolbox::FString Current;
	for (Toolbox::size_t Index = 0; Index < Text.Size(); ++Index)
	{
		if (Text[Index] == ' ' || Text[Index] == '\t')
		{
			if (!Current.IsEmpty())
			{
				Parts.PushBack(Current);
				Current = {};
			}
			continue;
		}
		Current.PushBack(Text[Index]);
	}
	if (!Current.IsEmpty())
	{
		Parts.PushBack(Current);
	}
	return Parts;
}
// 読込の本体（位置の管理とトークンの解決）。
class FParser
{
public:
	explicit FParser(const FUiStyleLimits& Limits) : m_Limits(Limits)
	{
	}
	// 資源を行へ分けて定義を集める。
	TResult<FUiStyleSheet> Collect(const FUiStyleSource& Source)
	{
		FLocation Where{Source.Name, 0};
		if (Source.Text.Size() > m_Limits.MaxSourceBytes)
		{
			return Error_Internal(Where, {}, "style source exceeds the size limit");
		}
		if (!IsValidUtf8(Source.Text))
		{
			return Error_Internal(Where, {}, "style source is not valid UTF-8");
		}
		bool bHeader = false;
		FRawStyle* Open = nullptr;
		Toolbox::size_t Begin = 0;
		for (Toolbox::size_t Index = 0; Index <= Source.Text.Size(); ++Index)
		{
			if (Index < Source.Text.Size() && Source.Text[Index] != '\n')
			{
				continue;
			}
			++Where.Line;
			const Toolbox::FString Line = Trim_Internal(Source.Text.Substr(Begin, Index - Begin));
			Begin = Index + 1;
			if (Line.IsEmpty() || Line[0] == '#')
			{
				continue;
			}
			if (!bHeader)
			{
				const auto Parts = Split_Internal(Line);
				if (Parts.Size() != 2 || !(Parts[0] == "dxfui-style"))
				{
					return Error_Internal(Where, {}, "missing 'dxfui-style 1' header");
				}
				if (!(Parts[1] == "1"))
				{
					return Error_Internal(Where, {}, "unsupported style format version");
				}
				bHeader = true;
				continue;
			}
			if (StartsWith_Internal(Line, "source "))
			{
				if (Open != nullptr)
				{
					return Error_Internal(Where, {}, "source directive inside a block");
				}
				const Toolbox::FString Name = Trim_Internal(Line.Substr(7));
				if (Name.Size() < 2 || Name[0] != '"' || Name.Back() != '"')
				{
					return Error_Internal(Where, {}, "source name must be quoted");
				}
				Where.Source = Name.Substr(1, Name.Size() - 2);
				Where.Line = 0;
				continue;
			}
			if (Open != nullptr)
			{
				if (Line == "}")
				{
					Open = nullptr;
					continue;
				}
				const Toolbox::size_t Equals = FindEquals_Internal(Line);
				if (Equals == 0)
				{
					return Error_Internal(Where, {}, "expected 'key = value'");
				}
				FRawEntry Entry{Trim_Internal(Line.Substr(0, Equals)), Trim_Internal(Line.Substr(Equals + 1)), Where};
				if (!IsName_Internal(Entry.Key) || Entry.Value.IsEmpty())
				{
					return Error_Internal(Where, Entry.Key, "invalid key or empty value");
				}
				Open->Entries.PushBack(Toolbox::Move(Entry));
				continue;
			}
			if (StartsWith_Internal(Line, "token "))
			{
				const Toolbox::FString Body = Line.Substr(6);
				const Toolbox::size_t Equals = FindEquals_Internal(Body);
				if (Equals == 0)
				{
					return Error_Internal(Where, {}, "expected 'token name = value'");
				}
				FRawToken Token{Trim_Internal(Body.Substr(0, Equals)), Trim_Internal(Body.Substr(Equals + 1)), Where};
				if (!IsName_Internal(Token.Name) || Token.Value.IsEmpty())
				{
					return Error_Internal(Where, Token.Name, "invalid token name or empty value");
				}
				for (const auto& Existing : m_Tokens)
				{
					if (Existing.Name == Token.Name)
					{
						return Error_Internal(Where, Token.Name, "duplicate token");
					}
				}
				if (m_Tokens.Size() >= m_Limits.MaxTokens)
				{
					return Error_Internal(Where, Token.Name, "token count exceeds the limit");
				}
				m_Tokens.PushBack(Toolbox::Move(Token));
				continue;
			}
			const auto Parts = Split_Internal(Line);
			if (Parts.Size() == 2 && Parts[0] == "default" && Parts[1] == "{")
			{
				if (m_bHasDefault)
				{
					return Error_Internal(Where, {}, "duplicate default block");
				}
				m_bHasDefault = true;
				FRawStyle Style;
				Style.bDefault = true;
				Style.Where = Where;
				m_Styles.PushBack(Toolbox::Move(Style));
				Open = &m_Styles.Back();
				continue;
			}
			if (Parts.Size() == 3 && Parts[0] == "style" && Parts[2] == "{")
			{
				if (!IsName_Internal(Parts[1]))
				{
					return Error_Internal(Where, Parts[1], "invalid style id");
				}
				for (const auto& Existing : m_Styles)
				{
					if (!Existing.bDefault && Existing.Id == Parts[1])
					{
						return Error_Internal(Where, Parts[1], "duplicate style id");
					}
				}
				if (m_Styles.Size() >= m_Limits.MaxStyles)
				{
					return Error_Internal(Where, Parts[1], "style count exceeds the limit");
				}
				FRawStyle Style;
				Style.Id = Parts[1];
				Style.Where = Where;
				m_Styles.PushBack(Toolbox::Move(Style));
				Open = &m_Styles.Back();
				continue;
			}
			return Error_Internal(Where, {}, "unknown statement");
		}
		if (!bHeader)
		{
			return Error_Internal(Where, {}, "missing 'dxfui-style 1' header");
		}
		if (Open != nullptr)
		{
			return Error_Internal(Open->Where, Open->Id, "block is not closed");
		}
		return TResult<FUiStyleSheet>::Success({});
	}
	// トークンを解決して表を作る。
	TResult<FUiStyleSheet> Build()
	{
		FUiStyleSheet Sheet;
		for (const auto& Token : m_Tokens)
		{
			FUiToken Value;
			auto Resolved = ResolveToken_Internal(Token.Name, Token.Where, 0, Value);
			if (!Resolved)
			{
				return Resolved;
			}
			auto Added = Sheet.AddToken(Token.Name, Value);
			if (!Added)
			{
				return Error_Internal(Token.Where, Token.Name, Added.Error().Message.CStr());
			}
		}
		for (const auto& Raw : m_Styles)
		{
			FUiStyleSet Set;
			for (const auto& Entry : Raw.Entries)
			{
				auto Applied = ApplyEntry_Internal(Entry, Set);
				if (!Applied)
				{
					return Applied;
				}
			}
			if (Raw.bDefault)
			{
				Sheet.SetDefault(Toolbox::Move(Set));
				continue;
			}
			auto Added = Sheet.AddStyle(Raw.Id, Toolbox::Move(Set));
			if (!Added)
			{
				return Error_Internal(Raw.Where, Raw.Id, Added.Error().Message.CStr());
			}
		}
		return TResult<FUiStyleSheet>::Success(Toolbox::Move(Sheet));
	}

private:
	// '='の位置（なければ0）。
	static Toolbox::size_t FindEquals_Internal(const Toolbox::FString& Text)
	{
		for (Toolbox::size_t Index = 1; Index < Text.Size(); ++Index)
		{
			if (Text[Index] == '=')
			{
				return Index;
			}
		}
		return 0;
	}
	// トークンを解決する（参照を辿る。循環・未定義は失敗）。
	TResult<FUiStyleSheet> ResolveToken_Internal(const Toolbox::FString& Name, const FLocation& Where,
	                                             Toolbox::size_t Depth, FUiToken& Out)
	{
		if (Depth > m_Limits.MaxTokenDepth || Depth > m_Tokens.Size())
		{
			return Error_Internal(Where, Name, "token reference cycle or too deep");
		}
		const FRawToken* Found = nullptr;
		for (const auto& Token : m_Tokens)
		{
			if (Token.Name == Name)
			{
				Found = &Token;
			}
		}
		if (Found == nullptr)
		{
			return Error_Internal(Where, Name, "undefined token");
		}
		if (Found->Value[0] == '@')
		{
			return ResolveToken_Internal(Found->Value.Substr(1), Found->Where, Depth + 1, Out);
		}
		FColor Color;
		if (ParseColor_Internal(Found->Value, Color))
		{
			Out.bColor = true;
			Out.Color = Color;
			return TResult<FUiStyleSheet>::Success({});
		}
		Toolbox::f32 Number = 0;
		if (ParseNumber_Internal(Found->Value, Number))
		{
			Out.bColor = false;
			Out.Number = Number;
			return TResult<FUiStyleSheet>::Success({});
		}
		return Error_Internal(Found->Where, Name, "token value must be a color (#RRGGBB[AA]) or a number");
	}
	// 値（リテラルかトークン）を色として読む。
	TResult<FUiStyleSheet> ReadColor_Internal(const FRawEntry& Entry, const Toolbox::FString& Text, FColor& Out)
	{
		if (!Text.IsEmpty() && Text[0] == '@')
		{
			FUiToken Token;
			auto Resolved = ResolveToken_Internal(Text.Substr(1), Entry.Where, 0, Token);
			if (!Resolved)
			{
				return Resolved;
			}
			if (!Token.bColor)
			{
				return Error_Internal(Entry.Where, Entry.Key, "token type mismatch (expected a color)");
			}
			Out = Token.Color;
			return TResult<FUiStyleSheet>::Success({});
		}
		if (!ParseColor_Internal(Text, Out))
		{
			return Error_Internal(Entry.Where, Entry.Key, "invalid color (expected #RRGGBB or #RRGGBBAA)");
		}
		return TResult<FUiStyleSheet>::Success({});
	}
	// 値（リテラルかトークン）を0以上の数値として読む。
	TResult<FUiStyleSheet> ReadNumber_Internal(const FRawEntry& Entry, const Toolbox::FString& Text, Toolbox::f32& Out)
	{
		if (!Text.IsEmpty() && Text[0] == '@')
		{
			FUiToken Token;
			auto Resolved = ResolveToken_Internal(Text.Substr(1), Entry.Where, 0, Token);
			if (!Resolved)
			{
				return Resolved;
			}
			if (Token.bColor)
			{
				return Error_Internal(Entry.Where, Entry.Key, "token type mismatch (expected a number)");
			}
			Out = Token.Number;
		}
		else if (!ParseNumber_Internal(Text, Out))
		{
			return Error_Internal(Entry.Where, Entry.Key, "invalid number");
		}
		if (!(Out >= 0))
		{
			return Error_Internal(Entry.Where, Entry.Key, "number must not be negative");
		}
		return TResult<FUiStyleSheet>::Success({});
	}
	// 一行を定義へ反映する。
	TResult<FUiStyleSheet> ApplyEntry_Internal(const FRawEntry& Entry, FUiStyleSet& Set)
	{
		FUiStylePatch* Patch = nullptr;
		Toolbox::FString Key = Entry.Key;
		const char* States[] = {"hover.", "pressed.", "focus.", "disabled."};
		FUiStylePatch* Targets[] = {&Set.Hover, &Set.Pressed, &Set.Focus, &Set.Disabled};
		for (Toolbox::size_t Index = 0; Index < 4; ++Index)
		{
			if (StartsWith_Internal(Key, States[Index]))
			{
				Patch = Targets[Index];
				Key = Key.Substr(Toolbox::FString(States[Index]).Size());
			}
		}
		// 基本値も一旦上書きとして読み、最後に基本値へ反映する。
		FUiStylePatch Base;
		FUiStylePatch& Target = Patch != nullptr ? *Patch : Base;
		if (Key == "background" || Key == "foreground" || Key == "border-color")
		{
			FColor Color;
			auto Read = ReadColor_Internal(Entry, Entry.Value, Color);
			if (!Read)
			{
				return Read;
			}
			(Key == "background"   ? Target.Background
			 : Key == "foreground" ? Target.Foreground
			                       : Target.BorderColor) = Color;
		}
		else if (Key == "border-width" || Key == "font-size" || Key == "opacity")
		{
			Toolbox::f32 Number = 0;
			auto Read = ReadNumber_Internal(Entry, Entry.Value, Number);
			if (!Read)
			{
				return Read;
			}
			if (Key == "opacity" && Number > 1)
			{
				return Error_Internal(Entry.Where, Entry.Key, "opacity must be between 0 and 1");
			}
			if (Key == "font-size" && !(Number > 0))
			{
				return Error_Internal(Entry.Where, Entry.Key, "font size must be positive");
			}
			(Key == "border-width" ? Target.BorderWidth
			 : Key == "font-size"  ? Target.FontSize
			                       : Target.Opacity) = Number;
		}
		else if (Key == "font-family")
		{
			if (Entry.Value.Size() < 3 || Entry.Value[0] != '"' || Entry.Value.Back() != '"')
			{
				return Error_Internal(Entry.Where, Entry.Key, "font family must be a quoted string");
			}
			Target.FontFamily = Entry.Value.Substr(1, Entry.Value.Size() - 2);
		}
		else if (Key == "padding")
		{
			const auto Parts = Split_Internal(Entry.Value);
			if (Parts.Size() != 1 && Parts.Size() != 2 && Parts.Size() != 4)
			{
				return Error_Internal(Entry.Where, Entry.Key, "padding takes 1, 2 or 4 numbers");
			}
			Toolbox::f32 Values[4] = {0, 0, 0, 0};
			for (Toolbox::size_t Index = 0; Index < Parts.Size(); ++Index)
			{
				auto Read = ReadNumber_Internal(Entry, Parts[Index], Values[Index]);
				if (!Read)
				{
					return Read;
				}
			}
			// 1個: 四辺、2個: 左右 上下、4個: 左 上 右 下。
			if (Parts.Size() == 1)
			{
				Target.Padding = FUiThickness::All(Values[0]);
			}
			else if (Parts.Size() == 2)
			{
				Target.Padding = FUiThickness::Symmetric(Values[0], Values[1]);
			}
			else
			{
				Target.Padding = FUiThickness{Values[0], Values[1], Values[2], Values[3]};
			}
		}
		else
		{
			return Error_Internal(Entry.Where, Entry.Key, "unknown key");
		}
		if (Patch == nullptr)
		{
			Base.ApplyTo(Set.Base);
		}
		return TResult<FUiStyleSheet>::Success({});
	}
	// 上限。
	FUiStyleLimits m_Limits;
	// トークン。
	Toolbox::TVector<FRawToken> m_Tokens;
	// スタイル（ポインターを保つため、確保し直さないよう読込前に予約しない。Backで最後の要素を指す）。
	Toolbox::TVector<FRawStyle> m_Styles;
	// 既定の定義があるか。
	bool m_bHasDefault = false;
};
} // namespace

// 複数の資源を読む。
TResult<FUiStyleSheet> ParseUiStyleSheet(const Toolbox::TVector<FUiStyleSource>& Sources, const FUiStyleLimits& Limits)
{
	try
	{
		FParser Parser(Limits);
		for (const auto& Source : Sources)
		{
			auto Collected = Parser.Collect(Source);
			if (!Collected)
			{
				return Collected;
			}
		}
		return Parser.Build();
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<FUiStyleSheet>::Failure(EErrorCode::InvalidArgument, Error.What());
	}
}
// 一つの資源を読む。
TResult<FUiStyleSheet> ParseUiStyleSheet(const Toolbox::FString& Name, const Toolbox::FString& Text,
                                         const FUiStyleLimits& Limits)
{
	Toolbox::TVector<FUiStyleSource> Sources;
	Sources.PushBack({Name, Text});
	return ParseUiStyleSheet(Sources, Limits);
}
} // namespace Dxf
