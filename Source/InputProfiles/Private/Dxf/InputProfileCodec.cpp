// SPDX-License-Identifier: NOASSERTION
#include "Dxf/InputProfileCodec.h"
#include "Dxf/InputProfileSchema.h"
namespace Dxf
{
using namespace Toolbox;
using namespace InputProfileSchema;
namespace
{
// 保存名と型付きキーを明示的に対応させる。enumの数値は保存しない。
struct FKeyName
{
	// 型付きのキーボード入力。
	EKey Key;
	// 保存へ使う明示した入力名。
	const char* Name;
};
// 型付きキーと保存名の対応表。
const FKeyName KeyNames[] = {
    {EKey::A, "A"},
    {EKey::B, "B"},
    {EKey::C, "C"},
    {EKey::D, "D"},
    {EKey::E, "E"},
    {EKey::F, "F"},
    {EKey::G, "G"},
    {EKey::H, "H"},
    {EKey::I, "I"},
    {EKey::J, "J"},
    {EKey::K, "K"},
    {EKey::L, "L"},
    {EKey::M, "M"},
    {EKey::N, "N"},
    {EKey::O, "O"},
    {EKey::P, "P"},
    {EKey::Q, "Q"},
    {EKey::R, "R"},
    {EKey::S, "S"},
    {EKey::T, "T"},
    {EKey::U, "U"},
    {EKey::V, "V"},
    {EKey::W, "W"},
    {EKey::X, "X"},
    {EKey::Y, "Y"},
    {EKey::Z, "Z"},
    {EKey::Num0, "Num0"},
    {EKey::Num1, "Num1"},
    {EKey::Num2, "Num2"},
    {EKey::Num3, "Num3"},
    {EKey::Num4, "Num4"},
    {EKey::Num5, "Num5"},
    {EKey::Num6, "Num6"},
    {EKey::Num7, "Num7"},
    {EKey::Num8, "Num8"},
    {EKey::Num9, "Num9"},
    {EKey::Space, "Space"},
    {EKey::Escape, "Escape"},
    {EKey::Enter, "Enter"},
    {EKey::Tab, "Tab"},
    {EKey::Backspace, "Backspace"},
    {EKey::Left, "Left"},
    {EKey::Right, "Right"},
    {EKey::Up, "Up"},
    {EKey::Down, "Down"},
    {EKey::LeftShift, "LeftShift"},
    {EKey::RightShift, "RightShift"},
    {EKey::LeftControl, "LeftControl"},
    {EKey::RightControl, "RightControl"},
    {EKey::F1, "F1"},
    {EKey::F2, "F2"},
    {EKey::F3, "F3"},
    {EKey::F4, "F4"},
    {EKey::F5, "F5"},
    {EKey::F6, "F6"},
    {EKey::F7, "F7"},
    {EKey::F8, "F8"},
    {EKey::F9, "F9"},
    {EKey::F10, "F10"},
    {EKey::F11, "F11"},
    {EKey::F12, "F12"}};
// Snapshotのボタン番号との対応を固定した名称。WindowsのABI値ではない。
const char* PadNames[] = {"Button1", "Button2", "Button3", "Button4", "Button5", "Button6", "Button7", "Button8", "Button9", "Button10", "Button11", "Button12", "Button13", "Button14", "Button15", "Button16"};
void ActionName(FStringView Name)
{
	Require(!Name.IsEmpty() && Name.Size() <= 128, "Invalid input Action name length");
	for (size_t Index = 0; Index < Name.Size(); ++Index)
	{
		const char Value = Name[Index];
		// 識別子の先頭として使える文字か。
		const bool Letter = (Value >= 'A' && Value <= 'Z') || (Value >= 'a' && Value <= 'z') || Value == '_';
		Require(Letter || (Index > 0 && ((Value >= '0' && Value <= '9') || Value == '-')), "Invalid input Action name");
	}
}
const char* KeyName(EKey Key)
{
	for (const auto& Entry : KeyNames)
	{
		if (Entry.Key == Key)
		{
			return Entry.Name;
		}
	}
	throw FSaveError(ESaveError::InvalidDocument, "Unknown keyboard key");
}
EKey ParseKey(FStringView Name)
{
	for (const auto& Entry : KeyNames)
	{
		if (Name == Entry.Name)
		{
			return Entry.Key;
		}
	}
	throw FSaveError(ESaveError::InvalidDocument, "Unknown keyboard key name");
}
const char* MouseName(EMouseButton Button)
{
	switch (Button)
	{
	case EMouseButton::Left:
		return "Left";
	case EMouseButton::Right:
		return "Right";
	case EMouseButton::Middle:
		return "Middle";
	default:
		throw FSaveError(ESaveError::InvalidDocument, "Unknown mouse button");
	}
}
EMouseButton ParseMouse(FStringView Name)
{
	if (Name == "Left")
	{
		return EMouseButton::Left;
	}
	if (Name == "Right")
	{
		return EMouseButton::Right;
	}
	if (Name == "Middle")
	{
		return EMouseButton::Middle;
	}
	throw FSaveError(ESaveError::InvalidDocument, "Unknown mouse button name");
}
void Binding(const FInputBindingDescription& Input)
{
	switch (Input.Device)
	{
	case EInputBindingDevice::Keyboard:
		(void)KeyName(Input.Key);
		break;
	case EInputBindingDevice::Mouse:
		(void)MouseName(Input.MouseButton);
		break;
	case EInputBindingDevice::Gamepad:
		Require(Input.PadSlot < 4 && Input.PadButton < 16, "Invalid gamepad binding");
		break;
	default:
		Require(false, "Unknown binding device");
	}
}
FString EncodeBinding(const FInputBindingDescription& Input)
{
	switch (Input.Device)
	{
	case EInputBindingDevice::Keyboard:
		return FString("{\"device\":\"Keyboard\",\"key\":") + FJsonWriter::Quote(KeyName(Input.Key)) + "}";
	case EInputBindingDevice::Mouse:
		return FString("{\"button\":") + FJsonWriter::Quote(MouseName(Input.MouseButton)) + ",\"device\":\"Mouse\"}";
	case EInputBindingDevice::Gamepad:
		return FString("{\"button\":") + FJsonWriter::Quote(PadNames[Input.PadButton]) + ",\"device\":\"Gamepad\",\"slot\":" + FJsonWriter::Unsigned(Input.PadSlot) + "}";
	default:
		throw FSaveError(ESaveError::InvalidDocument, "Unknown binding device");
	}
}
FInputBindingDescription DecodeBinding(const FJsonDocument& Json, int32 Index)
{
	// 検証後に返す独立した候補値。
	FInputBindingDescription Result;
	// 保存に明記された入力装置。
	const FString& Device = Text(Json, Index, "device");
	if (Device == "Keyboard")
	{
		Fields(Json, Index, "device|key");
		Result.Device = EInputBindingDevice::Keyboard;
		Result.Key = ParseKey(Text(Json, Index, "key"));
	}
	else if (Device == "Mouse")
	{
		Fields(Json, Index, "device|button");
		Result.Device = EInputBindingDevice::Mouse;
		Result.MouseButton = ParseMouse(Text(Json, Index, "button"));
	}
	else if (Device == "Gamepad")
	{
		Fields(Json, Index, "device|button|slot");
		Result.Device = EInputBindingDevice::Gamepad;
		// 浮動小数点を経由しないPad番号。
		const uint64 Slot = Integer(Json, Index, "slot");
		Require(Slot < 4, "Invalid gamepad slot");
		Result.PadSlot = static_cast<uint32>(Slot);
		// 保存された安定したボタン名。
		const FString& Button = Text(Json, Index, "button");
		// 許可集合へ正しく対応したか。
		bool Found = false;
		for (uint32 Number = 0; Number < 16; ++Number)
		{
			if (Button == PadNames[Number])
			{
				Found = true;
				Result.PadButton = Number;
			}
		}
		Require(Found, "Unknown gamepad button name");
	}
	else
	{
		Require(false, "Unknown binding device name");
	}
	return Result;
}
void Add(FString& Bytes, FStringView Text, size_t Limit)
{
	Require(Bytes.Size() <= Limit && Text.Size() <= Limit - Bytes.Size(), "Input profile output byte limit");
	Bytes += FString(Text.Data(), Text.Size());
}
} // namespace
void FInputProfileCodec::Validate(const FInputProfile& Profile, const FInputProfilePolicy& Policy)
{
	Bounds(Policy);
	Version(Profile.ProfileVersion);
	Require(Policy.AllowedActions.Size() <= Policy.MaxActions && Policy.ReservedBindings.Size() <= Policy.MaxBindings, "Input policy entry limit");
	for (size_t Index = 0; Index < Policy.AllowedActions.Size(); ++Index)
	{
		ActionName(Policy.AllowedActions[Index]);
		for (size_t Prior = 0; Prior < Index; ++Prior)
		{
			Require(Policy.AllowedActions[Prior] != Policy.AllowedActions[Index], "Duplicate permitted Action");
		}
	}
	for (const auto& Reserved : Policy.ReservedBindings)
	{
		Binding(Reserved);
	}
	Require(Profile.Actions.Size() == Policy.AllowedActions.Size(), "Incomplete input profile");
	// 確保前に検査する割当総数。
	size_t Total = 0;
	for (size_t Index = 0; Index < Profile.Actions.Size(); ++Index)
	{
		// 処理中のActionと割当。
		const auto& Action = Profile.Actions[Index];
		ActionName(Action.Action);
		Require(Find(Policy.AllowedActions.Begin(), Policy.AllowedActions.End(), Action.Action) != Policy.AllowedActions.End(), "Unknown input Action");
		Require(Action.Bindings.Size() <= Policy.MaxBindingsPerAction && Action.Bindings.Size() <= Policy.MaxBindings - Total, "Input binding count limit");
		Total += Action.Bindings.Size();
		for (size_t Prior = 0; Prior < Index; ++Prior)
		{
			Require(Profile.Actions[Prior].Action != Action.Action, "Duplicate input Action");
		}
		for (size_t InputIndex = 0; InputIndex < Action.Bindings.Size(); ++InputIndex)
		{
			// 使用する装置の型付き入力、またはその検証済み保存表現。
			const auto& Input = Action.Bindings[InputIndex];
			Binding(Input);
			for (const auto& Reserved : Policy.ReservedBindings)
			{
				Require(!(Input == Reserved), "Reserved UI input");
			}
			for (size_t Prior = 0; Prior < InputIndex; ++Prior)
			{
				Require(!(Input == Action.Bindings[Prior]), "Duplicate input binding");
			}
			for (size_t PriorAction = 0; PriorAction < Index; ++PriorAction)
			{
				for (const auto& Other : Profile.Actions[PriorAction].Bindings)
				{
					Require(!(Input == Other), "Input binding conflicts between Actions");
				}
			}
		}
	}
}
FString FInputProfileCodec::Encode(const FInputProfile& Profile, const FInputProfilePolicy& Policy)
{
	Validate(Profile, Policy);
	// コピーを並べ替え、共有プロファイルの配列順を変更しない。
	TVector<const FInputActionBindings*> Ordered;
	for (const auto& Action : Profile.Actions)
	{
		Ordered.PushBack(&Action);
	}
	Sort(Ordered.Begin(), Ordered.End(), [](const auto* Left, const auto* Right)
	     {
		     return Left->Action < Right->Action;
	     });
	// 検証後に返す独立した候補値。
	FString Result;
	Add(Result, "{\"actions\":[", Policy.MaxBytes);
	// Action間の区切りを一度だけ出力する判定。
	bool FirstAction = true;
	for (const auto* Action : Ordered)
	{
		if (!FirstAction)
		{
			Add(Result, ",", Policy.MaxBytes);
		}
		FirstAction = false;
		Add(Result, "{\"bindings\":[", Policy.MaxBytes);
		// このActionの正準順序の入力表現。
		TVector<FString> Inputs;
		for (const auto& Input : Action->Bindings)
		{
			Inputs.PushBack(EncodeBinding(Input));
		}
		Sort(Inputs.Begin(), Inputs.End());
		// 入力間の区切りを一度だけ出力する判定。
		bool FirstInput = true;
		for (const auto& Input : Inputs)
		{
			if (!FirstInput)
			{
				Add(Result, ",", Policy.MaxBytes);
			}
			FirstInput = false;
			Add(Result, Input, Policy.MaxBytes);
		}
		Add(Result, "],\"id\":", Policy.MaxBytes);
		Add(Result, FJsonWriter::Quote(Action->Action), Policy.MaxBytes);
		Add(Result, "}", Policy.MaxBytes);
	}
	Add(Result, "],\"profileVersion\":1}", Policy.MaxBytes);
	return Result;
}
FInputProfile FInputProfileCodec::Decode(FStringView Bytes, const FInputProfilePolicy& Policy)
{
	// 全入力の文法を検証した不変のJSON値集合。
	const FJsonDocument Json = FJsonDocument::Parse(Bytes, Limits(Policy));
	Version(Integer(Json, 0, "profileVersion"));
	Fields(Json, 0, "actions|profileVersion");
	// 必須Action配列の定義位置。
	const int32 Actions = Field(Json, 0, "actions", EJsonKind::Array);
	// 検証後に返す独立した候補値。
	FInputProfile Result;
	// 確保前に検査する割当総数。
	size_t Total = 0;
	for (int32 Index = Json.Get(Actions).FirstChild; Index >= 0; Index = Json.Get(Index).NextSibling)
	{
		Require(Result.Actions.Size() < Policy.MaxActions, "Input Action count limit");
		Fields(Json, Index, "id|bindings");
		// 処理中のActionと割当。
		FInputActionBindings Action;
		Action.Action = Text(Json, Index, "id");
		// 対象Actionの必須割当配列。
		const int32 Bindings = Field(Json, Index, "bindings", EJsonKind::Array);
		for (int32 Input = Json.Get(Bindings).FirstChild; Input >= 0; Input = Json.Get(Input).NextSibling)
		{
			Require(Action.Bindings.Size() < Policy.MaxBindingsPerAction && Total < Policy.MaxBindings, "Input binding count limit");
			Action.Bindings.PushBack(DecodeBinding(Json, Input));
			++Total;
		}
		Result.Actions.PushBack(Move(Action));
	}
	Validate(Result, Policy);
	return Result;
}
void FInputProfileCodec::Apply(const FInputProfile& Profile, const FInputProfilePolicy& Policy, FInputMap& Map, const FInputSnapshot& Current)
{
	Validate(Profile, Policy);
	Require(Map.ReplaceBindings(Profile.Actions, Current), "Invalid input profile replacement");
}
} // namespace Dxf
