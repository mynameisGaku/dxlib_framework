// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UserSettingsCodec.h"
#include "Dxf/InputProfileCodec.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/SaveStore.h"
namespace
{
// この利用者が許可する操作と予約キー。フレームワークへゲーム名を固定しない。
Dxf::FInputProfilePolicy Policy()
{
	// 検証後に返す独立した候補値。
	Dxf::FInputProfilePolicy Result;
	Result.AllowedActions = {"Jump", "Interact", "PadAction"};
	// メニューから脱出するため予約するキー。
	Dxf::FInputBindingDescription Escape;
	Escape.Key = Dxf::EKey::Escape;
	Result.ReservedBindings = {Escape};
	return Result;
}
// 変更した設定を独立プロセスで照合するための期待値。
Dxf::FUserSettings Expected()
{
	// 検証後に返す独立した候補値。
	Dxf::FUserSettings Result;
	// 繰り返し検査で再確保しないAction名、または変更するJump割当。
	Dxf::FInputBindingDescription Jump;
	Jump.Key = Dxf::EKey::K;
	// ゲームの明示したマウス割当。
	Dxf::FInputBindingDescription Interact;
	Interact.Device = Dxf::EInputBindingDevice::Mouse;
	Interact.MouseButton = Dxf::EMouseButton::Right;
	// ゲームの明示したPad割当。
	Dxf::FInputBindingDescription Pad;
	Pad.Device = Dxf::EInputBindingDevice::Gamepad;
	Pad.PadSlot = 3;
	Pad.PadButton = 15;
	Result.Input.Actions = {{"Jump", {Jump}}, {"Interact", {Interact}}, {"PadAction", {Pad}}};
	Result.Volume = 0.4f;
	Result.UiScale = 1.75f;
	return Result;
}
Toolbox::int32 Run(const Toolbox::FString& Mode, const Toolbox::FPath& Root)
{
	// 専用Rootの正常面を管理する本体保存窓口。
	Dxf::FSaveStore Store(Root);
	if (Mode == "write")
	{
		if (Store.Save("settings", Dxf::FUserSettingsCodec::Encode(Expected(), Policy(), "external_profile")) != 1)
		{
			throw Toolbox::FException("Profile consumer requires empty dedicated slot");
		}
	}
	else if (Mode == "read")
	{
		// Storeから完全性確認を通過した設定封筒。
		const auto Saved = Store.Load("settings");
		if (Saved.State != Dxf::ESaveLoadState::Loaded || !Saved.Document || Saved.Document->Generation != 1)
		{
			throw Toolbox::FException("Profile consumer did not find committed settings");
		}
		// この境界で検査する入力値。
		const auto Value = Dxf::FUserSettingsCodec::Decode(*Saved.Document, Policy(), "external_profile");
		if (Value.Volume != 0.4f || Value.UiScale != 1.75f || Dxf::FInputProfileCodec::Encode(Value.Input, Policy()) != Dxf::FInputProfileCodec::Encode(Expected().Input, Policy()))
		{
			throw Toolbox::FException("Restored settings differ from independent expected values");
		}
		// 再読込した割当を実InputMapへ一括適用し、Spaceでは動かず変更キーで操作する。
		Dxf::FInputMap Map;
		// 既存Snapshot履歴を一度ずつ更新するTracker。
		Dxf::FInputStateTracker Tracker;
		// 試験が明示する固定入力値。
		Dxf::FRawInput Raw;
		Raw.Pads[3].bConnected = true;
		Tracker.Advance(Raw);
		Dxf::FInputProfileCodec::Apply(Value.Input, Policy(), Map, Tracker.GetSnapshot());
		Raw.Keys[static_cast<Toolbox::size_t>(Dxf::EKey::Space)] = true;
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		if (Map.IsDown("Jump"))
		{
			throw Toolbox::FException("Old key still controls restored Action");
		}
		Raw.Keys[static_cast<Toolbox::size_t>(Dxf::EKey::K)] = true;
		Raw.MouseButtons[static_cast<Toolbox::size_t>(Dxf::EMouseButton::Right)] = true;
		Raw.Pads[3].Buttons[15] = true;
		Tracker.Advance(Raw);
		Map.Update(Tracker.GetSnapshot());
		if (!Map.WasPressed("Jump") || !Map.WasPressed("Interact") || !Map.WasPressed("PadAction"))
		{
			throw Toolbox::FException("Restored profile does not control real Actions");
		}
	}
	else
	{
		throw Toolbox::FException("Unknown profile consumer mode");
	}
	Toolbox::Out << "INPUT_PROFILE_CONSUMER_PASSED " << Mode << '\n';
	return 0;
}
} // namespace
#if defined(_WIN32)
// OSから受けたUnicodeの絶対保存Rootを一度だけ変換する入口。
int wmain(int Count, wchar_t** Arguments)
{
	try
	{
		if (Count != 3)
		{
			throw Toolbox::FException("Use write/read and absolute dedicated SaveRoot");
		}
		return Run(Toolbox::FromWide(Toolbox::FWideString(Arguments[1])), Toolbox::FPath(Arguments[2]));
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << '\n';
		return 1;
	}
}
#else
// POSIXの実行入口。保存値と割当だけを扱い、SceneやPhysicsを要求しない。
int main(int Count, char** Arguments)
{
	try
	{
		if (Count != 3)
		{
			throw Toolbox::FException("Use write/read and absolute dedicated SaveRoot");
		}
		return Run(Toolbox::FString(Arguments[1]), Toolbox::FPath(Arguments[2]));
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << '\n';
		return 1;
	}
}
#endif
