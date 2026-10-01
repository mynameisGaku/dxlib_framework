// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ContentSchemaReader.h"
#include "Toolbox/ProjectPaths.h"
namespace Dxf::ContentPrivate
{
FSchemaReader::FSchemaReader(const Toolbox::FJsonDocument& Document, Toolbox::FString Path, FSceneContentLimits Limits)
    : m_Document(Document), m_Path(Toolbox::Move(Path)), m_Limits(Limits)
{
}
[[noreturn]] void FSchemaReader::Fail(Toolbox::int32 Index, const char* Reason) const
{
	const auto& Value = m_Document.Get(Index >= 0 ? Index : 0);
	Toolbox::TVector<Toolbox::int32> Route;
	for (auto Current = Index >= 0 ? Index : 0; Current >= 0; Current = Get(Current).Parent)
	{
		Route.PushBack(Current);
	}
	Toolbox::FString Location("$");
	for (auto Offset = Route.Size(); Offset > 1; --Offset)
	{
		const auto Child = Route[Offset - 2];
		const auto Parent = Get(Child).Parent;
		if (Get(Parent).Kind == Toolbox::EJsonKind::Array)
		{
			Toolbox::uint32 Element = 0;
			for (auto Sibling = Get(Parent).FirstChild; Sibling != Child; Sibling = Get(Sibling).NextSibling)
			{
				++Element;
			}
			Location += "[" + Toolbox::ToString(Element) + "]";
		}
		else
		{
			Location += "/" + Get(Child).Key;
		}
	}
	throw FSceneContentError({m_Path, Value.Key, Location, Reason, Value.Line, Value.Column});
}
const Toolbox::FJsonValue& FSchemaReader::Get(Toolbox::int32 Index) const
{
	return m_Document.Get(Index);
}
Toolbox::int32 FSchemaReader::Find(Toolbox::int32 Object, const char* Key) const
{
	if (Get(Object).Kind != Toolbox::EJsonKind::Object)
	{
		Fail(Object, "Expected object");
	}
	return m_Document.Find(Object, Key);
}
Toolbox::int32 FSchemaReader::Required(Toolbox::int32 Object, const char* Key) const
{
	const auto Index = Find(Object, Key);
	if (Index < 0)
	{
		Fail(Object, Key);
	}
	return Index;
}
void FSchemaReader::Fields(Toolbox::int32 Object, Toolbox::TInitializerList<const char*> Keys) const
{
	if (Get(Object).Kind != Toolbox::EJsonKind::Object)
	{
		Fail(Object, "Expected object");
	}
	for (auto Child = Get(Object).FirstChild; Child >= 0; Child = Get(Child).NextSibling)
	{
		bool Known = false;
		for (const char* Key : Keys)
		{
			Known = Known || Get(Child).Key == Key;
		}
		if (!Known)
		{
			Fail(Child, "Unknown field");
		}
	}
}
void FSchemaReader::Array(Toolbox::int32 Index) const
{
	if (Get(Index).Kind != Toolbox::EJsonKind::Array)
	{
		Fail(Index, "Expected array");
	}
}
const FContentParameterValue* FSchemaReader::Parameter(Toolbox::int32 Index, EContentParameterKind Kind) const
{
	if (Get(Index).Kind != Toolbox::EJsonKind::Object)
	{
		return nullptr;
	}
	Fields(Index, {"parameter"});
	const auto Name = Id(Required(Index, "parameter"));
	for (const auto& Value : m_Parameters)
	{
		if (Value.Id == Name)
		{
			if (Value.Kind != Kind)
			{
				Fail(Index, "Parameter type mismatch");
			}
			return &Value;
		}
	}
	Fail(Index, "Unknown parameter");
}
Toolbox::FString FSchemaReader::String(Toolbox::int32 Index) const
{
	if (Get(Index).Kind != Toolbox::EJsonKind::String)
	{
		Fail(Index, "Expected string");
	}
	const auto& Text = Get(Index).Text;
	for (Toolbox::size_t Byte = 0; Byte < Text.Size(); ++Byte)
	{
		if (Text.Data()[Byte] == 0)
		{
			Fail(Index, "Embedded NUL is not permitted");
		}
	}
	return Text;
}
Toolbox::FString FSchemaReader::Id(Toolbox::int32 Index) const
{
	const auto Value = Parameter(Index, EContentParameterKind::Asset);
	return ValidateId(Value ? Value->Asset : String(Index), Index);
}
Toolbox::FString FSchemaReader::KeyId(Toolbox::int32 Index) const
{
	return ValidateId(Get(Index).Key, Index);
}
Toolbox::FString FSchemaReader::ValidateId(Toolbox::FString Text, Toolbox::int32 Index) const
{
	if (Text.IsEmpty() || Text.Size() > m_Limits.MaxIdBytes)
	{
		Fail(Index, "ID length limit");
	}
	for (Toolbox::size_t Byte = 0; Byte < Text.Size(); ++Byte)
	{
		const char Character = Text.Data()[Byte];
		const bool Letter =
		    (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z') || Character == '_';
		if (!Letter && (Byte == 0 || !((Character >= '0' && Character <= '9') || Character == '-')))
		{
			Fail(Index, "ID must be an ASCII identifier");
		}
	}
	return Text;
}
Toolbox::FString FSchemaReader::Path(Toolbox::int32 Index) const
{
	const auto Text = String(Index);
	if (Text.IsEmpty() || Text.Data()[0] == '/' || Text.Data()[0] == '\\')
	{
		Fail(Index, "Content path must be ProjectRoot-relative");
	}
	for (const char Character : Text)
	{
		if (Character == ':')
		{
			Fail(Index, "Absolute or drive-relative path is not permitted");
		}
	}
	// 実Rootを知らない純粋検証でも、同じ既存字句規則で外への..を拒否する。
	Toolbox::FAssetPathResolver Resolver;
#ifdef _WIN32
	Resolver.SetRoot(Toolbox::FPath("C:/ContentValidation"));
#else
	Resolver.SetRoot(Toolbox::FPath("/ContentValidation"));
#endif
	Toolbox::FPath Resolved;
	if (!Resolver.Resolve(Text, Resolved))
	{
		Fail(Index, "Content path escapes ProjectRoot");
	}
	return Text;
}
Toolbox::f64 FSchemaReader::Number(Toolbox::int32 Index) const
{
	if (const auto Value = Parameter(Index, EContentParameterKind::Number))
	{
		return Value->Number;
	}
	if (Get(Index).Kind != Toolbox::EJsonKind::Number)
	{
		Fail(Index, "Expected finite number");
	}
	return Get(Index).Number;
}
Toolbox::f32 FSchemaReader::Scalar(Toolbox::int32 Index) const
{
	const auto Value = static_cast<Toolbox::f32>(Number(Index));
	if (!Toolbox::IsFinite(Value))
	{
		Fail(Index, "Value cannot be represented by f32");
	}
	return Value;
}
Toolbox::uint32 FSchemaReader::Integer(Toolbox::int32 Index, Toolbox::uint32 Maximum) const
{
	if (Get(Index).Kind == Toolbox::EJsonKind::Number)
	{
		for (const char C : Get(Index).Text)
		{
			if (C == '.' || C == 'e' || C == 'E')
			{
				Fail(Index, "Integer requires an integer token");
			}
		}
	}
	const auto Value = Number(Index);
	if (Value < 0 || Value > Maximum || Toolbox::Floor(Value) != Value)
	{
		Fail(Index, "Expected bounded nonnegative integer");
	}
	return static_cast<Toolbox::uint32>(Value);
}
bool FSchemaReader::Boolean(Toolbox::int32 Index) const
{
	if (const auto Value = Parameter(Index, EContentParameterKind::Boolean))
	{
		return Value->Boolean;
	}
	if (Get(Index).Kind != Toolbox::EJsonKind::Boolean)
	{
		Fail(Index, "Expected boolean");
	}
	return Get(Index).Boolean;
}
Toolbox::FVector2 FSchemaReader::Vector2(Toolbox::int32 Index) const
{
	if (const auto Value = Parameter(Index, EContentParameterKind::Vector2))
	{
		return Value->Vector2;
	}
	Array(Index);
	const auto X = Get(Index).FirstChild;
	const auto Y = X >= 0 ? Get(X).NextSibling : -1;
	if (Y < 0 || Get(Y).NextSibling >= 0)
	{
		Fail(Index, "Expected two vector components");
	}
	return {Scalar(X), Scalar(Y)};
}
Toolbox::FVector3 FSchemaReader::Vector3(Toolbox::int32 Index) const
{
	if (const auto Value = Parameter(Index, EContentParameterKind::Vector3))
	{
		return Value->Vector3;
	}
	Array(Index);
	const auto X = Get(Index).FirstChild;
	const auto Y = X >= 0 ? Get(X).NextSibling : -1;
	const auto Z = Y >= 0 ? Get(Y).NextSibling : -1;
	if (Z < 0 || Get(Z).NextSibling >= 0)
	{
		Fail(Index, "Expected three vector components");
	}
	return {Scalar(X), Scalar(Y), Scalar(Z)};
}
Toolbox::FQuaternion FSchemaReader::Rotation(Toolbox::int32 Index) const
{
	Array(Index);
	const auto X = Get(Index).FirstChild;
	const auto Y = X >= 0 ? Get(X).NextSibling : -1;
	const auto Z = Y >= 0 ? Get(Y).NextSibling : -1;
	const auto W = Z >= 0 ? Get(Z).NextSibling : -1;
	if (W < 0 || Get(W).NextSibling >= 0)
	{
		Fail(Index, "Expected Quaternion [x,y,z,w]");
	}
	const Toolbox::FQuaternion Result{Scalar(X), Scalar(Y), Scalar(Z), Scalar(W)};
	try
	{
		return Result.Normalized();
	}
	catch (const Toolbox::FException&)
	{
		Fail(Index, "Invalid Quaternion norm");
	}
}
FColor FSchemaReader::Color(Toolbox::int32 Index) const
{
	if (const auto Value = Parameter(Index, EContentParameterKind::Color))
	{
		return Value->Color;
	}
	Array(Index);
	Toolbox::uint8 Bytes[4];
	auto Child = Get(Index).FirstChild;
	for (Toolbox::uint32 Offset = 0; Offset < 4; ++Offset)
	{
		if (Child < 0)
		{
			Fail(Index, "Expected four color components");
		}
		Bytes[Offset] = static_cast<Toolbox::uint8>(Integer(Child, 255));
		Child = Get(Child).NextSibling;
	}
	if (Child >= 0)
	{
		Fail(Index, "Expected four color components");
	}
	return {Bytes[0], Bytes[1], Bytes[2], Bytes[3]};
}
void FSchemaReader::Parameters(Toolbox::int32 Index, const Toolbox::TVector<FContentParameterValue>& Overrides)
{
	if (Index >= 0)
	{
		if (Get(Index).Kind != Toolbox::EJsonKind::Object)
		{
			Fail(Index, "Expected parameter declarations");
		}
		for (auto Child = Get(Index).FirstChild; Child >= 0; Child = Get(Child).NextSibling)
		{
			Fields(Child, {"type", "default", "min", "max"});
			FContentParameterValue Value;
			Value.Id = Get(Child).Key;
			// 宣言名にも通常IDと同じ上限と文字規則を適用する。
			if (Value.Id.IsEmpty() || Value.Id.Size() > m_Limits.MaxIdBytes)
			{
				Fail(Child, "Parameter ID length limit");
			}
			for (Toolbox::size_t Byte = 0; Byte < Value.Id.Size(); ++Byte)
			{
				const char C = Value.Id.Data()[Byte];
				if (!((C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || C == '_' || (Byte > 0 && ((C >= '0' && C <= '9') || C == '-'))))
				{
					Fail(Child, "Invalid parameter ID");
				}
			}
			const auto Type = String(Required(Child, "type"));
			const auto Default = Required(Child, "default");
			if (Get(Default).Kind == Toolbox::EJsonKind::Object)
			{
				Fail(Default, "Parameter default must be a literal");
			}
			if (Type == "number")
			{
				Value.Kind = EContentParameterKind::Number;
				Value.Number = Number(Default);
			}
			else if (Type == "bool")
			{
				Value.Kind = EContentParameterKind::Boolean;
				Value.Boolean = Boolean(Default);
			}
			else if (Type == "vector2")
			{
				Value.Kind = EContentParameterKind::Vector2;
				Value.Vector2 = Vector2(Default);
			}
			else if (Type == "vector3")
			{
				Value.Kind = EContentParameterKind::Vector3;
				Value.Vector3 = Vector3(Default);
			}
			else if (Type == "color")
			{
				Value.Kind = EContentParameterKind::Color;
				Value.Color = Color(Default);
			}
			else if (Type == "asset")
			{
				Value.Kind = EContentParameterKind::Asset;
				Value.Asset = Id(Default);
			}
			else
			{
				Fail(Child, "Unknown parameter type");
			}
			const auto Min = Find(Child, "min");
			const auto Max = Find(Child, "max");
			if ((Min >= 0 || Max >= 0) && Value.Kind != EContentParameterKind::Number)
			{
				Fail(Child, "Bounds require a number parameter");
			}
			if ((Min >= 0 && Get(Min).Kind != Toolbox::EJsonKind::Number) || (Max >= 0 && Get(Max).Kind != Toolbox::EJsonKind::Number))
			{
				Fail(Child, "Parameter bounds must be literals");
			}
			if ((Min >= 0 && Value.Number < Number(Min)) || (Max >= 0 && Value.Number > Number(Max)) || (Min >= 0 && Max >= 0 && Number(Min) > Number(Max)))
			{
				Fail(Child, "Parameter default outside declared range");
			}
			bool Overridden = false;
			for (const auto& Override : Overrides)
			{
				if (Override.Id == Value.Id)
				{
					if (Overridden || Override.Kind != Value.Kind)
					{
						Fail(Child, "Duplicate or wrong-type override");
					}
					Value = Override;
					Overridden = true;
				}
			}
			if (!Toolbox::IsFinite(Value.Number) || !Value.Vector2.IsValid() || !Value.Vector3.IsValid())
			{
				Fail(Child, "Non-finite parameter override");
			}

			if ((Min >= 0 || Max >= 0) && Value.Kind != EContentParameterKind::Number)
			{
				Fail(Child, "Bounds require a number parameter");
			}
			if ((Min >= 0 && Value.Number < Number(Min)) || (Max >= 0 && Value.Number > Number(Max)) || (Min >= 0 && Max >= 0 && Number(Min) > Number(Max)))
			{
				Fail(Child, "Parameter outside declared range");
			}
			if (Value.Kind == EContentParameterKind::Asset)
			{
				if (Value.Asset.IsEmpty() || Value.Asset.Size() > m_Limits.MaxIdBytes)
				{
					Fail(Child, "Asset override ID length limit");
				}
				for (Toolbox::size_t I = 0; I < Value.Asset.Size(); ++I)
				{
					const char C = Value.Asset.Data()[I];
					if (!((C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || C == '_' || (I > 0 && ((C >= '0' && C <= '9') || C == '-'))))
					{
						Fail(Child, "Invalid asset override ID");
					}
				}
			}
			m_Parameters.PushBack(Toolbox::Move(Value));
		}
	}
	for (const auto& Override : Overrides)
	{
		bool Found = false;
		for (const auto& Value : m_Parameters)
		{
			Found = Found || Value.Id == Override.Id;
		}
		if (!Found)
		{
			Fail(0, "Unknown parameter override");
		}
	}
}
const Toolbox::TVector<FContentParameterValue>& FSchemaReader::GetParameters() const noexcept
{
	return m_Parameters;
}
} // namespace Dxf::ContentPrivate
