// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiNavigationBindings.h"
namespace Dxf
{
// 既定の割当。
FUiNavigationBindings FUiNavigationBindings::MakeDefault()
{
	FUiNavigationBindings Bindings;
	auto Key = [&](EUiNavigationCommand Command, EKey Value, EUiShiftCondition Shift = EUiShiftCondition::Any)
	{
		Bindings.Keys[static_cast<Toolbox::size_t>(Command)].PushBack({Value, Shift});
	};
	auto Pad = [&](EUiNavigationCommand Command, Toolbox::size_t Button)
	{
		Bindings.PadButtons[static_cast<Toolbox::size_t>(Command)].PushBack(Button);
	};
	Key(EUiNavigationCommand::Next, EKey::Tab, EUiShiftCondition::NoShift);
	Key(EUiNavigationCommand::Previous, EKey::Tab, EUiShiftCondition::Shift);
	Key(EUiNavigationCommand::Up, EKey::Up);
	Key(EUiNavigationCommand::Down, EKey::Down);
	Key(EUiNavigationCommand::Left, EKey::Left);
	Key(EUiNavigationCommand::Right, EKey::Right);
	Key(EUiNavigationCommand::Confirm, EKey::Enter);
	Key(EUiNavigationCommand::Confirm, EKey::Space);
	Key(EUiNavigationCommand::Cancel, EKey::Escape);
	Key(EUiNavigationCommand::Cancel, EKey::Backspace);
	Pad(EUiNavigationCommand::Confirm, 0);
	Pad(EUiNavigationCommand::Cancel, 1);
	Pad(EUiNavigationCommand::Previous, 4);
	Pad(EUiNavigationCommand::Next, 5);
	return Bindings;
}
} // namespace Dxf
