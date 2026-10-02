// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/JsonWriter.h"
#include "Toolbox/Algorithm.h"
namespace Toolbox
{
namespace
{
// 復号後のUTF-8バイト順で辞書を正準化する。
bool LessKey(const FString& Left, const FString& Right)
{
	// 共通する比較範囲。
	const size_t Count = Min(Left.Size(), Right.Size());
	for (size_t I = 0; I < Count; ++I)
	{
		if (Left[I] != Right[I])
		{
			return static_cast<uint8>(Left[I]) < static_cast<uint8>(Right[I]);
		}
	}
	return Left.Size() < Right.Size();
}
// 一回の出力だけを所有する有限サイズの書き込み。
class FWriter
{
public:
	FWriter(const FJsonDocument& Document, size_t Limit) : m_Document(Document), m_Limit(Limit)
	{
		if (Limit == 0)
		{
			throw FException("JSON output limit must be positive");
		}
	}
	void Append(const FString& Text)
	{
		if (Text.Size() > m_Limit - m_Output.Size())
		{
			throw FException("JSON output byte limit");
		}
		m_Output += Text;
	}
	void Value(int32 Index, uint32 Depth)
	{
		if (Depth > 256)
		{
			throw FException("JSON output depth limit");
		}
		// 入力の型付き値。
		const FJsonValue& Item = m_Document.Get(Index);
		switch (Item.Kind)
		{
		case EJsonKind::Null:
			Append("null");
			break;
		case EJsonKind::Boolean:
			Append(Item.Boolean ? "true" : "false");
			break;
		case EJsonKind::Number:
			Append(Item.Text);
			break;
		case EJsonKind::String:
			Append(FJsonWriter::Quote(Item.Text));
			break;
		case EJsonKind::Array:
		case EJsonKind::Object:
		{
			// 辞書だけを並べ替え、意味を持つ配列順は残す。
			TVector<int32> Children;
			for (int32 Child = Item.FirstChild; Child >= 0; Child = m_Document.Get(Child).NextSibling)
			{
				Children.PushBack(Child);
			}
			if (Item.Kind == EJsonKind::Object && Children.Size() > 1)
			{
				Sort(Children.begin(), Children.end(), [&](int32 Left, int32 Right)
				     {
					     return LessKey(m_Document.Get(Left).Key, m_Document.Get(Right).Key);
				     });
			}
			Append(Item.Kind == EJsonKind::Object ? "{" : "[");
			// 区切りを一度だけ挿入するための位置。
			bool First = true;
			for (int32 Child : Children)
			{
				if (!First)
				{
					Append(",");
				}
				First = false;
				if (Item.Kind == EJsonKind::Object)
				{
					Append(FJsonWriter::Quote(m_Document.Get(Child).Key));
					Append(":");
				}
				Value(Child, Depth + 1);
			}
			Append(Item.Kind == EJsonKind::Object ? "}" : "]");
			break;
		}
		}
	}
	FString Finish()
	{
		return Move(m_Output);
	}

private:
	// 呼出し中だけ参照する不変入力。
	const FJsonDocument& m_Document;
	// 出力前に検査するバイト上限。
	size_t m_Limit;
	// 出力バイトの唯一所有者。
	FString m_Output;
};
} // namespace
FString FJsonWriter::Write(const FJsonDocument& Document, int32 Index, size_t MaxBytes)
{
	// 書き込み中の一時所有値。
	FWriter Writer(Document, MaxBytes);
	Writer.Value(Index, 1);
	return Writer.Finish();
}
FString FJsonWriter::Quote(FStringView Text)
{
	// 制御文字の十六進表記。
	constexpr const char* Hex = "0123456789abcdef";
	// escape後の文字列。
	FString Out("\"");
	for (size_t I = 0; I < Text.Size(); ++I)
	{
		// 符号付きcharに依存しない入力バイト。
		const uint8 Byte = static_cast<uint8>(Text[I]);
		if (Byte == '"' || Byte == '\\')
		{
			Out += '\\';
			Out += static_cast<char>(Byte);
		}
		else if (Byte < 0x20)
		{
			Out += "\\u00";
			Out += Hex[Byte >> 4];
			Out += Hex[Byte & 15];
		}
		else
		{
			Out += static_cast<char>(Byte);
		}
	}
	Out += '"';
	// 既存のUTF-8・surrogate検証を共有し、文法を二重実装しない。
	FJsonLimits Limits;
	Limits.MaxBytes = Out.Size();
	Limits.MaxStringBytes = Max(Text.Size(), size_t(1));
	FJsonDocument::Parse(Out, Limits);
	return Out;
}
FString FJsonWriter::Unsigned(uint64 Value)
{
	// uint64の十進20桁を逆向きに組み立てる領域。
	char Digits[20];
	// 次の書き込み位置。
	size_t Position = 20;
	do
	{
		Digits[--Position] = static_cast<char>('0' + Value % 10);
		Value /= 10;
	} while (Value != 0);
	return FString(Digits + Position, 20 - Position);
}
uint64 FJsonWriter::ParseUnsigned(FStringView Text)
{
	if (Text.IsEmpty() || (Text.Size() > 1 && Text[0] == '0'))
	{
		throw FException("Unsigned decimal requires canonical digits");
	}
	// 次の桁を加える前にoverflowを検査する累積値。
	uint64 Value = 0;
	for (size_t I = 0; I < Text.Size(); ++I)
	{
		if (Text[I] < '0' || Text[I] > '9')
		{
			throw FException("Unsigned decimal requires digits");
		}
		// 検証済みの一桁。
		const uint64 Digit = static_cast<uint64>(Text[I] - '0');
		if (Value > (TNumericLimits<uint64>::Max() - Digit) / 10)
		{
			throw FException("Unsigned decimal overflow");
		}
		Value = Value * 10 + Digit;
	}
	return Value;
}
} // namespace Toolbox
