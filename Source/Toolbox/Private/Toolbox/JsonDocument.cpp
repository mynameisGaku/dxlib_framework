// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/JsonDocument.h"
#include "Toolbox/JsonError.h"
#include <stdlib.h>
#include <locale.h>
namespace Toolbox
{
namespace
{
// プロセスのlocaleを変更せず、JSONの小数点を常にピリオドとして解釈する。
class FNumberLocale
{
public:
	FNumberLocale()
	{
#ifdef _WIN32
		m_Locale = _create_locale(LC_NUMERIC, "C");
#else
		m_Locale = newlocale(LC_NUMERIC_MASK, "C", nullptr);
#endif
		if (m_Locale == nullptr)
		{
			throw FException("JSON numeric locale allocation failed");
		}
	}
	~FNumberLocale() noexcept
	{
#ifdef _WIN32
		_free_locale(m_Locale);
#else
		freelocale(m_Locale);
#endif
	}
	f64 Convert(const char* Text) const noexcept
	{
#ifdef _WIN32
		return _strtod_l(Text, nullptr, m_Locale);
#else
		return strtod_l(Text, nullptr, m_Locale);
#endif
	}

private:
#ifdef _WIN32
	_locale_t m_Locale = nullptr;
#else
	locale_t m_Locale = nullptr;
#endif
};
// 一要求だけで使用する字句と構造の読み取り状態。
class FReader
{
public:
	FReader(FStringView Bytes, FJsonLimits Limits, TVector<FJsonValue>& Values)
	    : m_Bytes(Bytes), m_Limits(Limits), m_Values(Values)
	{
	}
	void Read()
	{
		if (m_Limits.MaxBytes == 0 || m_Limits.MaxDepth == 0 || m_Limits.MaxDepth > 256 || m_Limits.MaxStringBytes == 0 || m_Limits.MaxValues == 0 || m_Bytes.Size() > m_Limits.MaxBytes)
		{
			Fail("JSON input limit");
		}
		if (m_Bytes.Size() >= 3 && static_cast<uint8>(m_Bytes[0]) == 0xef && static_cast<uint8>(m_Bytes[1]) == 0xbb && static_cast<uint8>(m_Bytes[2]) == 0xbf)
		{
			m_Offset = 3;
		}
		ReadValue(1);
		Space();
		if (m_Offset != m_Bytes.Size())
		{
			Fail("JSON trailing data");
		}
	}

private:
	[[noreturn]] void Fail(const char* Reason) const
	{
		throw FJsonError(Reason, m_Line, m_Column);
	}
	char Peek() const noexcept
	{
		return m_Offset < m_Bytes.Size() ? m_Bytes[m_Offset] : 0;
	}
	char Take()
	{
		if (m_Offset == m_Bytes.Size())
		{
			Fail("JSON unexpected end");
		}
		const char Value = m_Bytes[m_Offset++];
		if (Value == '\n')
		{
			++m_Line;
			m_Column = 1;
		}
		else
		{
			++m_Column;
		}
		return Value;
	}
	void Space()
	{
		while (Peek() == ' ' || Peek() == '\t' || Peek() == '\r' || Peek() == '\n')
		{
			Take();
		}
	}
	void Expect(char Value)
	{
		if (Take() != Value)
		{
			Fail("JSON unexpected token");
		}
	}
	void Literal(const char* Text)
	{
		for (size_t Index = 0; Text[Index] != 0; ++Index)
		{
			Expect(Text[Index]);
		}
	}
	uint32 Hex()
	{
		uint32 Value = 0;
		for (uint32 Index = 0; Index < 4; ++Index)
		{
			const char Digit = Take();
			Value <<= 4;
			if (Digit >= '0' && Digit <= '9')
			{
				Value += Digit - '0';
			}
			else if (Digit >= 'a' && Digit <= 'f')
			{
				Value += Digit - 'a' + 10;
			}
			else if (Digit >= 'A' && Digit <= 'F')
			{
				Value += Digit - 'A' + 10;
			}
			else
			{
				Fail("JSON invalid unicode escape");
			}
		}
		return Value;
	}
	void Append(FString& Text, uint32 Code)
	{
		// 上限検査はUTF-8の符号化で増える最大4バイトの確保より先に行う。
		const size_t Count = Code < 0x80 ? 1 : Code < 0x800 ? 2
		                                   : Code < 0x10000 ? 3
		                                                    : 4;
		if (Count > m_Limits.MaxStringBytes || Text.Size() > m_Limits.MaxStringBytes - Count)
		{
			Fail("JSON string limit");
		}
		if (Count == 1)
		{
			Text += static_cast<char>(Code);
		}
		else
		{
			Text += static_cast<char>((Count == 2 ? 0xc0 : Count == 3 ? 0xe0 : 0xf0) | (Code >> (6 * (Count - 1))));
			for (size_t Index = Count - 1; Index > 0; --Index)
			{
				Text += static_cast<char>(0x80 | ((Code >> (6 * (Index - 1))) & 63));
			}
		}
	}
	FString String()
	{
		Expect('"');
		FString Text;
		while (Peek() != '"')
		{
			uint32 Code = static_cast<uint8>(Take());
			if (Code < 0x20)
			{
				Fail("JSON unescaped control");
			}
			if (Code == '\\')
			{
				const char Escape = Take();
				if (Escape == 'u')
				{
					Code = Hex();
					if (Code >= 0xd800 && Code <= 0xdbff)
					{
						Expect('\\');
						Expect('u');
						const uint32 Low = Hex();
						if (Low < 0xdc00 || Low > 0xdfff)
						{
							Fail("JSON missing low surrogate");
						}
						Code = 0x10000 + ((Code - 0xd800) << 10) + Low - 0xdc00;
					}
					else if (Code >= 0xdc00 && Code <= 0xdfff)
					{
						Fail("JSON lone low surrogate");
					}
				}
				else if (Escape == '"' || Escape == '\\' || Escape == '/')
				{
					Code = Escape;
				}
				else if (Escape == 'b' || Escape == 'f' || Escape == 'n' || Escape == 'r' || Escape == 't')
				{
					Code = Escape == 'b' ? 8 : Escape == 'f' ? 12
					                       : Escape == 'n'   ? 10
					                       : Escape == 'r'   ? 13
					                                         : 9;
				}
				else
				{
					Fail("JSON invalid escape");
				}
			}
			else if (Code >= 0x80)
			{
				const uint32 Count = Code >= 0xc2 && Code <= 0xdf   ? 2
				                     : Code >= 0xe0 && Code <= 0xef ? 3
				                     : Code >= 0xf0 && Code <= 0xf4 ? 4
				                                                    : 0;
				if (Count == 0)
				{
					Fail("JSON invalid UTF-8");
				}
				Code &= Count == 2 ? 31 : Count == 3 ? 15
				                                     : 7;
				for (uint32 Index = 1; Index < Count; ++Index)
				{
					const uint32 Next = static_cast<uint8>(Take());
					if ((Next & 0xc0) != 0x80)
					{
						Fail("JSON invalid UTF-8 continuation");
					}
					Code = (Code << 6) | (Next & 63);
				}
				if ((Count == 2 && Code < 0x80) || (Count == 3 && Code < 0x800) || (Count == 4 && Code < 0x10000) || Code > 0x10ffff || (Code >= 0xd800 && Code <= 0xdfff))
				{
					Fail("JSON invalid UTF-8 scalar");
				}
			}
			Append(Text, Code);
		}
		Expect('"');
		return Text;
	}
	void Digits()
	{
		if (Peek() < '0' || Peek() > '9')
		{
			Fail("JSON expected digit");
		}
		while (Peek() >= '0' && Peek() <= '9')
		{
			Take();
		}
	}
	f64 Number()
	{
		const size_t Start = m_Offset;
		if (Peek() == '-')
		{
			Take();
		}
		if (Peek() == '0')
		{
			Take();
		}
		else
		{
			Digits();
		}
		if (Peek() == '.')
		{
			Take();
			Digits();
		}
		if (Peek() == 'e' || Peek() == 'E')
		{
			Take();
			if (Peek() == '+' || Peek() == '-')
			{
				Take();
			}
			Digits();
		}
		// strtodに渡す範囲を終端付きで保持する。文法は上で検査済み。
		const FString Token(m_Bytes.Data() + Start, m_Offset - Start);
		const f64 Value = m_NumberLocale.Convert(Token.CStr());
		if (!IsFinite(Value))
		{
			Fail("JSON number overflow");
		}
		return Value;
	}
	int32 ReadValue(uint32 Depth)
	{
		Space();
		if (Depth > m_Limits.MaxDepth || m_Values.Size() >= m_Limits.MaxValues || m_Values.Size() >= static_cast<size_t>(TNumericLimits<int32>::Max()))
		{
			Fail("JSON structure limit");
		}
		const int32 Index = static_cast<int32>(m_Values.Size());
		FJsonValue Value;
		Value.Line = m_Line;
		Value.Column = m_Column;
		m_Values.PushBack(Value);
		if (Peek() == '{' || Peek() == '[')
		{
			const bool Object = Take() == '{';
			m_Values[Index].Kind = Object ? EJsonKind::Object : EJsonKind::Array;
			const char End = Object ? '}' : ']';
			int32 Previous = -1;
			Space();
			if (Peek() != End)
			{
				for (;;)
				{
					FString Key;
					if (Object)
					{
						Space();
						Key = String();
						for (int32 Child = m_Values[Index].FirstChild; Child >= 0; Child = m_Values[Child].NextSibling)
						{
							if (m_Values[Child].Key == Key)
							{
								Fail("JSON duplicate decoded key");
							}
						}
						Space();
						Expect(':');
					}
					const int32 Child = ReadValue(Depth + 1);
					m_Values[Child].Key = Move(Key);
					m_Values[Child].Parent = Index;
					if (Previous < 0)
					{
						m_Values[Index].FirstChild = Child;
					}
					else
					{
						m_Values[Previous].NextSibling = Child;
					}
					Previous = Child;
					Space();
					if (Peek() == End)
					{
						break;
					}
					Expect(',');
				}
			}
			Expect(End);
		}
		else if (Peek() == '"')
		{
			m_Values[Index].Kind = EJsonKind::String;
			// 再確保で値の参照が壊れないようローカルへ完成させてから移す。
			m_Values[Index].Text = String();
		}
		else if (Peek() == 't' || Peek() == 'f')
		{
			const bool Boolean = Peek() == 't';
			Literal(Boolean ? "true" : "false");
			m_Values[Index].Kind = EJsonKind::Boolean;
			m_Values[Index].Boolean = Boolean;
		}
		else if (Peek() == 'n')
		{
			Literal("null");
		}
		else
		{
			const size_t Start = m_Offset;
			m_Values[Index].Number = Number();
			m_Values[Index].Text = FString(m_Bytes.Data() + Start, m_Offset - Start);
			m_Values[Index].Kind = EJsonKind::Number;
		}
		return Index;
	}
	FNumberLocale m_NumberLocale;
	FStringView m_Bytes;
	FJsonLimits m_Limits;
	TVector<FJsonValue>& m_Values;
	size_t m_Offset = 0;
	uint32 m_Line = 1;
	uint32 m_Column = 1;
};
} // namespace
FJsonDocument FJsonDocument::Parse(FStringView Bytes, FJsonLimits Limits)
{
	FJsonDocument Result;
	FReader Reader(Bytes, Limits, Result.m_Values);
	Reader.Read();
	return Result;
}
const FJsonValue& FJsonDocument::Get(int32 Index) const
{
	if (Index < 0 || static_cast<size_t>(Index) >= m_Values.Size())
	{
		throw FException("JSON index out of range");
	}
	return m_Values[Index];
}
int32 FJsonDocument::Find(int32 Object, FStringView Key) const
{
	if (Get(Object).Kind != EJsonKind::Object)
	{
		throw FException("JSON value is not an object");
	}
	for (int32 Child = Get(Object).FirstChild; Child >= 0; Child = Get(Child).NextSibling)
	{
		if (FStringView(Get(Child).Key.Data(), Get(Child).Key.Size()) == Key)
		{
			return Child;
		}
	}
	return -1;
}
size_t FJsonDocument::Size() const noexcept
{
	return m_Values.Size();
}
} // namespace Toolbox
