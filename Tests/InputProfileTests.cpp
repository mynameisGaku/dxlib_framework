// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/InputProfileCodec.h"
#include "Dxf/UserSettingsCodec.h"
#include "Dxf/InputRebindCapture.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/SaveStore.h"
#include "Dxf/SaveError.h"
#include "Toolbox/JsonWriter.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 入力を一度ずつ既存Trackerへ渡し、Map更新回数は試験内で明示する。
FInputBindingDescription Key(EKey Value)
{
	// 検証後に返す独立した候補値。
	FInputBindingDescription Result;
	Result.Key = Value;
	return Result;
}
FInputBindingDescription Mouse(EMouseButton Value)
{
	// 検証後に返す独立した候補値。
	FInputBindingDescription Result;
	Result.Device = EInputBindingDevice::Mouse;
	Result.MouseButton = Value;
	return Result;
}
FInputBindingDescription Pad(uint32 Slot, uint32 Button)
{
	// 検証後に返す独立した候補値。
	FInputBindingDescription Result;
	Result.Device = EInputBindingDevice::Gamepad;
	Result.PadSlot = Slot;
	Result.PadButton = Button;
	return Result;
}
FInputProfilePolicy Policy()
{
	// 検証後に返す独立した候補値。
	FInputProfilePolicy Result;
	Result.AllowedActions = {"Jump", "Interact"};
	Result.ReservedBindings = {Key(EKey::Escape), Key(EKey::F1)};
	return Result;
}
FInputProfile Profile()
{
	// 検証後に返す独立した候補値。
	FInputProfile Result;
	Result.Actions = {{"Jump", {Key(EKey::K), Pad(3, 15)}}, {"Interact", {Mouse(EMouseButton::Right)}}};
	return Result;
}
template <typename Function>
void Reject(Function Run)
{
	// 注入した処理が例外で失敗したか。
	bool Failed = false;
	try
	{
		Run();
	}
	catch (...)
	{
		Failed = true;
	}
	REQUIRE(Failed);
}
void PressKey(FRawInput& Raw, EKey Value, bool Down)
{
	Raw.Keys[static_cast<size_t>(Value)] = Down;
}
} // namespace
TEST("Profile explicit names round trip all supported keyboard mouse and pad bindings")
{
	// このゲームが許可するActionと予約入力。
	const FInputProfilePolicy Rules = Policy();
	// この境界で検査する入力値。
	FInputProfile Value = Profile();
	for (size_t Index = 0; Index < static_cast<size_t>(EKey::Count); ++Index)
	{
		if (Index == static_cast<size_t>(EKey::Escape) || Index == static_cast<size_t>(EKey::F1))
		{
			continue;
		}
		Value.Actions[0].Bindings = {Key(static_cast<EKey>(Index))};
		// 検証後に返す独立した候補値。
		const auto Result = FInputProfileCodec::Decode(FInputProfileCodec::Encode(Value, Rules), Rules);
		REQUIRE(Result.Actions[1].Bindings[0] == Value.Actions[0].Bindings[0]);
	}
	for (uint32 Slot = 0; Slot < 4; ++Slot)
	{
		for (uint32 Button = 0; Button < 16; ++Button)
		{
			Value.Actions[0].Bindings = {Pad(Slot, Button)};
			// 検証後に返す独立した候補値。
			const auto Result = FInputProfileCodec::Decode(FInputProfileCodec::Encode(Value, Rules), Rules);
			REQUIRE(Result.Actions[1].Bindings[0] == Value.Actions[0].Bindings[0]);
		}
	}
	for (size_t Button = 0; Button < static_cast<size_t>(EMouseButton::Count); ++Button)
	{
		Value.Actions[0].Bindings.Clear();
		Value.Actions[1].Bindings = {Mouse(static_cast<EMouseButton>(Button))};
		// 検証後に返す独立した候補値。
		const auto Result = FInputProfileCodec::Decode(FInputProfileCodec::Encode(Value, Rules), Rules);
		REQUIRE(Result.Actions[0].Bindings[0] == Value.Actions[1].Bindings[0]);
	}
}
TEST("Profile writer canonicalizes order without modifying shared definitions")
{
	// このゲームが許可するActionと予約入力。
	const auto Rules = Policy();
	// 順序変更から保護する共有元の割当。
	const auto Original = Profile();
	// 独立した配列順変更の対照。
	auto Reordered = Original;
	Swap(Reordered.Actions[0], Reordered.Actions[1]);
	Swap(Reordered.Actions[1].Bindings[0], Reordered.Actions[1].Bindings[1]);
	// 照合する長さ付き保存表現。
	const FString Bytes = FInputProfileCodec::Encode(Original, Rules);
	REQUIRE(Bytes == FInputProfileCodec::Encode(Reordered, Rules));
	REQUIRE(Bytes == "{\"actions\":[{\"bindings\":[{\"button\":\"Right\",\"device\":\"Mouse\"}],\"id\":\"Interact\"},{\"bindings\":[{\"button\":\"Button16\",\"device\":\"Gamepad\",\"slot\":3},{\"device\":\"Keyboard\",\"key\":\"K\"}],\"id\":\"Jump\"}],\"profileVersion\":1}");
	REQUIRE(Original.Actions[0].Action == "Jump");
	REQUIRE(Original.Actions[0].Bindings[0].Key == EKey::K);
}
TEST("Profile rejects unknown incomplete duplicated conflicting reserved and invalid inputs")
{
	// このゲームが許可するActionと予約入力。
	const auto Rules = Policy();
	for (uint32 Mutation = 0; Mutation < 10; ++Mutation)
	{
		// 元の設定から独立した不正入力の候補。
		auto Bad = Profile();
		switch (Mutation)
		{
		case 0:
			Bad.Actions[0].Action = "Unknown";
			break;
		case 1:
			Bad.Actions.PopBack();
			break;
		case 2:
			Bad.Actions[1].Action = "Jump";
			break;
		case 3:
			Bad.Actions[1].Bindings = {Key(EKey::K)};
			break;
		case 4:
			Bad.Actions[0].Bindings = {Key(EKey::Escape)};
			break;
		case 5:
			Bad.Actions[0].Bindings = {Key(EKey::Count)};
			break;
		case 6:
			Bad.Actions[0].Bindings = {Pad(4, 0)};
			break;
		case 7:
			Bad.Actions[0].Bindings = {Pad(0, 16)};
			break;
		case 8:
			Bad.Actions[0].Bindings = {Key(EKey::A), Key(EKey::A)};
			break;
		case 9:
			Bad.Actions[0].Action = FString("Jump\0bad", 8);
			break;
		}
		Reject([&]
		       {
			       FInputProfileCodec::Validate(Bad, Rules);
		       });
	}
	// 空の割当を明示した有効プロファイル。
	auto Unassigned = Profile();
	Unassigned.Actions[0].Bindings.Clear();
	FInputProfileCodec::Validate(Unassigned, Rules);
}
TEST("Profile malformed handcrafted JSON rejects future unknown names duplicate keys and nonintegers")
{
	// このゲームが許可するActionと予約入力。
	FInputProfilePolicy Rules;
	Rules.AllowedActions = {"Jump"};
	// このActionの正準順序の入力表現。
	const char* Inputs[] = {
	    "{\"profileVersion\":2,\"actions\":[]}",
	    "{\"profileVersion\":1.0,\"actions\":[]}",
	    "{\"profileVersion\":1,\"actions\":[],\"extra\":0}",
	    "{\"profileVersion\":1,\"actions\":[{\"id\":\"Jump\",\"bindings\":[{\"device\":\"Keyboard\",\"key\":\"Spacebar\"}]}]}",
	    "{\"profileVersion\":1,\"actions\":[{\"id\":\"Jump\",\"bindings\":[{\"device\":\"Gamepad\",\"button\":\"Button1\",\"slot\":0.0}]}]}",
	    "{\"profileVersion\":1,\"actions\":[{\"id\":\"Jump\",\"bindings\":[{\"device\":\"Gamepad\",\"button\":\"Button0\",\"slot\":0}]}]}",
	    "{\"profileVersion\":1,\"actions\":[{\"id\":\"Jump\",\"bindings\":[{\"device\":\"Mouse\",\"button\":\"Left\",\"key\":\"A\"}]}]}",
	    "{\"profileVersion\":1,\"profileVersi\\u006fn\":1,\"actions\":[]}",
	    "{\"profileVersion\":1,\"actions\":[]} true"};
	for (const char* Bytes : Inputs)
	{
		Reject([&]
		       {
			       (void)FInputProfileCodec::Decode(Bytes, Rules);
		       });
	}
}
TEST("Profile lowered finite bounds reject before applying any input")
{
	// このゲームが許可するActionと予約入力。
	auto Rules = Policy();
	// この境界で検査する入力値。
	const auto Value = Profile();
	Rules.MaxBindings = 2;
	Reject([&]
	       {
		       FInputProfileCodec::Validate(Value, Rules);
	       });
	Rules = Policy();
	Rules.MaxBindingsPerAction = 1;
	Reject([&]
	       {
		       FInputProfileCodec::Validate(Value, Rules);
	       });
	Rules = Policy();
	Rules.MaxActions = 1;
	Reject([&]
	       {
		       (void)FInputProfileCodec::Decode(FInputProfileCodec::Encode(Value, Policy()), Rules);
	       });
	Rules = Policy();
	Rules.MaxBytes = 20;
	Reject([&]
	       {
		       (void)FInputProfileCodec::Encode(Value, Rules);
	       });
	Rules.MaxBytes = 0;
	Reject([&]
	       {
		       FInputProfileCodec::Validate(Value, Rules);
	       });
}
TEST("Profile invalid late binding preserves old map and action history")
{
	// 既存Action更新を行う本体Map。
	FInputMap Map;
	REQUIRE(Map.Bind("Jump", EKey::Space));
	// 既存Snapshot履歴を一度ずつ更新するTracker。
	FInputStateTracker Tracker;
	// 試験が明示する固定入力値。
	FRawInput Raw;
	PressKey(Raw, EKey::Space, true);
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.WasPressed("Jump"));
	// 元の設定から独立した不正入力の候補。
	auto Bad = Profile();
	Bad.Actions[1].Bindings = {Pad(0, 16)};
	REQUIRE(!Map.ReplaceBindings(Bad.Actions, Tracker.GetSnapshot()));
	REQUIRE(Map.WasPressed("Jump"));
	Reject([&]
	       {
		       FInputProfileCodec::Apply(Bad, Policy(), Map, Tracker.GetSnapshot());
	       });
	REQUIRE(Map.WasPressed("Jump"));
}
TEST("Profile apply changes real actions and suppresses held keyboard mouse and all pad slots")
{
	for (uint32 Slot = 0; Slot < 4; ++Slot)
	{
		// 既存Action更新を行う本体Map。
		FInputMap Map;
		// 既存Snapshot履歴を一度ずつ更新するTracker。
		FInputStateTracker Tracker;
		// 試験が明示する固定入力値。
		FRawInput Raw;
		PressKey(Raw, EKey::K, true);
		Raw.MouseButtons[1] = true;
		Raw.Pads[Slot].bConnected = true;
		Raw.Pads[Slot].Buttons[15] = true;
		Tracker.Advance(Raw);
		// この境界で検査する入力値。
		auto Value = Profile();
		Value.Actions[0].Bindings[1] = Pad(Slot, 15);
		FInputProfileCodec::Apply(Value, Policy(), Map, Tracker.GetSnapshot());
		Map.Update(Tracker.GetSnapshot());
		REQUIRE(!Map.IsDown("Jump") && !Map.WasPressed("Interact"));
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		REQUIRE(!Map.IsDown("Jump"));
		PressKey(Raw, EKey::K, false);
		Raw.MouseButtons[1] = false;
		Raw.Pads[Slot].Buttons[15] = false;
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		Raw.Pads[Slot].Buttons[15] = true;
		Raw.MouseButtons[1] = true;
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		REQUIRE(Map.WasPressed("Jump") && Map.WasPressed("Interact"));
		// 同じSnapshotを追加PollしてもMapを更新せず、保持の観察だけを行う。
		REQUIRE(Map.WasPressed("Jump"));
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		REQUIRE(Map.IsDown("Jump") && !Map.WasPressed("Jump"));
		Raw.Pads[Slot].Buttons[15] = false;
		Raw.MouseButtons[1] = false;
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		PressKey(Raw, EKey::Space, true);
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		REQUIRE(!Map.IsDown("Jump"));
		PressKey(Raw, EKey::K, true);
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		REQUIRE(Map.WasPressed("Jump"));
	}
}
TEST("Profile focus loss pad reconnect and modal dismissal require release")
{
	// 既存Action更新を行う本体Map。
	FInputMap Map;
	// 既存Snapshot履歴を一度ずつ更新するTracker。
	FInputStateTracker Tracker;
	// 試験が明示する固定入力値。
	FRawInput Raw;
	Raw.Pads[3].bConnected = true;
	Tracker.Advance(Raw);
	FInputProfileCodec::Apply(Profile(), Policy(), Map, Tracker.GetSnapshot());
	Raw.Pads[3].bConnected = false;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	Raw.Pads[3].bConnected = true;
	Raw.Pads[3].Buttons[15] = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(!Map.WasPressed("Jump"));
	Raw.Pads[3].Buttons[15] = false;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	PressKey(Raw, EKey::K, true);
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.WasPressed("Jump"));
	Map.SuppressUntilRelease(Tracker.GetSnapshot());
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(!Map.IsDown("Jump"));
	Raw.bFocused = false;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	Raw.bFocused = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(!Map.WasPressed("Jump"));
	PressKey(Raw, EKey::K, false);
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	PressKey(Raw, EKey::K, true);
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.WasPressed("Jump"));
}
TEST("Rebind start click is ignored and keyboard mouse pad capture occurs after release")
{
	for (uint32 Device = 0; Device < 3; ++Device)
	{
		// 既存Snapshot履歴を一度ずつ更新するTracker。
		FInputStateTracker Tracker;
		// 試験が明示する固定入力値。
		FRawInput Raw;
		Raw.MouseButtons[0] = true;
		Raw.Pads[2].bConnected = true;
		PressKey(Raw, EKey::A, true);
		Raw.Pads[2].Buttons[0] = true;
		Tracker.Advance(Raw);
		// 押下の解放を待つ再割当採取。
		FInputRebindCapture Capture;
		REQUIRE(Capture.Begin(static_cast<EInputBindingDevice>(Device), 2, Tracker.GetSnapshot()));
		Capture.Poll(Tracker.GetSnapshot());
		REQUIRE(Capture.GetState() == EInputRebindState::WaitingRelease);
		Raw.MouseButtons[0] = false;
		PressKey(Raw, EKey::A, false);
		Raw.Pads[2].Buttons[0] = false;
		Tracker.Advance(Raw);
		Capture.Poll(Tracker.GetSnapshot());
		REQUIRE(Capture.GetState() == EInputRebindState::WaitingPress);
		PressKey(Raw, EKey::K, true);
		Raw.MouseButtons[1] = true;
		Raw.Pads[2].Buttons[15] = true;
		Tracker.Advance(Raw);
		Capture.Poll(Tracker.GetSnapshot());
		// Ready後に返された単一の型付き割当。
		FInputBindingDescription Binding;
		REQUIRE(Capture.TryGetBinding(Binding));
		REQUIRE(Binding == (Device == 0 ? Key(EKey::K) : Device == 1 ? Mouse(EMouseButton::Right)
		                                                             : Pad(2, 15)));
		Capture.Cancel();
		REQUIRE(Capture.GetState() == EInputRebindState::Ready);
	}
}
TEST("Rebind focus loss disconnection cancellation and ambiguity are terminal")
{
	// 既存Snapshot履歴を一度ずつ更新するTracker。
	FInputStateTracker Tracker;
	// 試験が明示する固定入力値。
	FRawInput Raw;
	Raw.Pads[0].bConnected = true;
	Tracker.Advance(Raw);
	// 押下の解放を待つ再割当採取。
	FInputRebindCapture Capture;
	REQUIRE(Capture.Begin(EInputBindingDevice::Keyboard, 0, Tracker.GetSnapshot()));
	Capture.Poll(Tracker.GetSnapshot());
	PressKey(Raw, EKey::K, true);
	PressKey(Raw, EKey::J, true);
	Tracker.Advance(Raw);
	Capture.Poll(Tracker.GetSnapshot());
	REQUIRE(Capture.GetState() == EInputRebindState::Canceled);
	REQUIRE(Capture.Begin(EInputBindingDevice::Gamepad, 0, Tracker.GetSnapshot()));
	Raw.Pads[0].bConnected = false;
	Tracker.Advance(Raw);
	Capture.Poll(Tracker.GetSnapshot());
	REQUIRE(Capture.GetState() == EInputRebindState::Canceled);
	REQUIRE(Capture.Begin(EInputBindingDevice::Mouse, 0, Tracker.GetSnapshot()));
	Raw.bFocused = false;
	Tracker.Advance(Raw);
	Capture.Poll(Tracker.GetSnapshot());
	REQUIRE(Capture.GetState() == EInputRebindState::Canceled);
	REQUIRE(!Capture.Begin(EInputBindingDevice::Keyboard, 0, Tracker.GetSnapshot()));
}
TEST("Settings persist independently from progress and decoding invalid settings preserves caller")
{
	// 進行と独立して保存する設定値。
	FUserSettings Settings;
	Settings.Input = Profile();
	Settings.Volume = 0.4f;
	Settings.UiScale = 1.75f;
	// 検証したSettings種類の保存封筒。
	const auto Document = FUserSettingsCodec::Encode(Settings, Policy(), "profile_test");
	// 今回の試験だけが使用する新規の専用保存Root。
	const FPath Root = FPath(DXF_PROFILE_TEST_ROOT) / FPath(FString(reinterpret_cast<const char*>(u8"設定 専用-")) + FJsonWriter::Unsigned(MonotonicNanoseconds()));
	// 専用Rootの正常面を管理する本体保存窓口。
	FSaveStore Store(Root);
	REQUIRE(Store.Save("settings", Document) == 1);
	// 設定と別系列のゲーム進行。
	FSaveDocument Progress;
	Progress.GameId = "profile_test";
	Progress.ContentRevision = "bundle-v1";
	Progress.SceneKey = "course";
	Progress.CheckpointKey = "start";
	REQUIRE(Store.Save("manual", Progress) == 1);
	// 新しい読込から得た独立した設定値。
	const auto Loaded = FUserSettingsCodec::Decode(*Store.Load("settings").Document, Policy(), "profile_test");
	REQUIRE(Loaded.Volume == 0.4f && Loaded.UiScale == 1.75f);
	REQUIRE(Loaded.Input.Actions[1].Bindings[1] == Key(EKey::K));
	REQUIRE(Store.Load("manual").Document->Kind == ESaveDocumentKind::Progress);
	// 不正入力から保護する現在の有効設定。
	FUserSettings Current = Settings;
	// 元の設定から独立した不正入力の候補。
	auto Bad = Document;
	Bad.Payload = "{\"settingsVersion\":1,\"input\":{},\"volume\":1.1,\"uiScale\":2}";
	Reject([&]
	       {
		       Current = FUserSettingsCodec::Decode(Bad, Policy(), "profile_test");
	       });
	REQUIRE(Current.Volume == Settings.Volume && Current.UiScale == Settings.UiScale);
	Reject([&]
	       {
		       (void)FUserSettingsCodec::Decode(Document, Policy(), "different_game");
	       });
	Reject([&]
	       {
		       (void)FUserSettingsCodec::Decode(Progress, Policy(), "profile_test");
	       });
}
TEST("Settings reject invalid ranges unknown fields and future versions before caller adoption")
{
	// 進行と独立して保存する設定値。
	FUserSettings Settings;
	Settings.Input = Profile();
	// 後続の不正候補を作る正常な設定文書。
	const auto Valid = FUserSettingsCodec::Encode(Settings, Policy(), "settings_ranges");
	for (uint32 Mutation = 0; Mutation < 5; ++Mutation)
	{
		// 元の設定から独立した不正入力の候補。
		auto Bad = Settings;
		switch (Mutation)
		{
		case 0:
			Bad.Volume = -0.1f;
			break;
		case 1:
			Bad.Volume = 1.1f;
			break;
		case 2:
			Bad.UiScale = 0.24f;
			break;
		case 3:
			Bad.UiScale = 8.1f;
			break;
		case 4:
			Bad.SettingsVersion = 2;
			break;
		}
		Reject([&]
		       {
			       (void)FUserSettingsCodec::Encode(Bad, Policy(), "settings_ranges");
		       });
	}
	// 使用する装置の型付き入力、またはその検証済み保存表現。
	const FString Input = FInputProfileCodec::Encode(Settings.Input, Policy());
	// 上限判定に使う付加バイト数、または拒否する追加項目の試験表現。
	const char* Extra[] = {",\"volume\":-0.1,\"uiScale\":1}", ",\"volume\":1,\"uiScale\":8.1}", ",\"volume\":1,\"uiScale\":1,\"unknown\":0}"};
	for (const char* Suffix : Extra)
	{
		// 元の設定から独立した不正入力の候補。
		auto Bad = Valid;
		Bad.Payload = FString("{\"settingsVersion\":1,\"input\":") + Input + Suffix;
		Reject([&]
		       {
			       (void)FUserSettingsCodec::Decode(Bad, Policy(), "settings_ranges");
		       });
	}
	for (uint32 Level = 0; Level < 2; ++Level)
	{
		// 将来バージョンを明示した拒否対象。
		auto Future = Valid;
		if (Level == 0)
		{
			Future.GameVersion = 2;
		}
		else
		{
			Future.Payload = FString("{\"settingsVersion\":2,\"input\":") + Input + ",\"volume\":1,\"uiScale\":1}";
		}
		// 期待した診断で拒否されたか。
		bool Rejected = false;
		try
		{
			(void)FUserSettingsCodec::Decode(Future, Policy(), "settings_ranges");
		}
		catch (const FSaveError& Error)
		{
			REQUIRE(Error.GetCode() == ESaveError::UnsupportedVersion);
			Rejected = true;
		}
		REQUIRE(Rejected);
	}
}
