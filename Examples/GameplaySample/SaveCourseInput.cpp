// SPDX-License-Identifier: NOASSERTION
#include "SaveCourseInput.h"
namespace Dxf::GameplaySample
{
FInputProfilePolicy FSaveCourseInput::Policy()
{
	FInputProfilePolicy Result;
	Result.AllowedActions = {"Left", "Right", "Up", "Down", "Jump", "Interact", "Spawn"};
	for (const auto Key : {EKey::Escape, EKey::F1, EKey::F2})
	{
		FInputBindingDescription Binding;
		Binding.Key = Key;
		Result.ReservedBindings.PushBack(Binding);
	}
	return Result;
}
FUserSettings FSaveCourseInput::Defaults()
{
	FUserSettings Result;
	const auto Policy = FSaveCourseInput::Policy();
	// 許可名の順番はこのC++値だけで使い、保存は名前で行う。
	const EKey Keys[] = {EKey::A, EKey::D, EKey::W, EKey::S, EKey::Space, EKey::E, EKey::G};
	for (Toolbox::size_t Index = 0; Index < Policy.AllowedActions.Size(); ++Index)
	{
		FInputActionBindings Action;
		Action.Action = Policy.AllowedActions[Index];
		FInputBindingDescription Binding;
		Binding.Key = Keys[Index];
		Action.Bindings.PushBack(Binding);
		Result.Input.Actions.PushBack(Toolbox::Move(Action));
	}
	return Result;
}
} // namespace Dxf::GameplaySample
