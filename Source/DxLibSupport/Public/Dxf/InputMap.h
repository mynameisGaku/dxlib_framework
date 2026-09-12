#pragma once
#include "Dxf/InputSnapshot.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace Dxf
{
/** Aggregates bindings into actions. Call Update once for each new input snapshot. */
class FInputMap
{
public:
	bool Bind(std::string Action, EKey Key)
	{
		return Key < EKey::Count && Bind_Internal(std::move(Action), {EDevice::Keyboard, static_cast<std::size_t>(Key), 0});
	}
	bool BindMouse(std::string Action, EMouseButton Button)
	{
		return Button < EMouseButton::Count && Bind_Internal(std::move(Action), {EDevice::Mouse, static_cast<std::size_t>(Button), 0});
	}
	bool BindPad(std::string Action, std::size_t Pad, std::size_t Button)
	{
		return Pad < 4 && Button < 16 && Bind_Internal(std::move(Action), {EDevice::Gamepad, Button, Pad});
	}
	bool Unbind(const std::string& Action)
	{
		return m_Actions.erase(Action) != 0;
	}
	void Clear() noexcept
	{
		m_Actions.clear();
	}
	void Update(const FInputSnapshot& Input)
	{
		for (auto& [Name, Action] : m_Actions)
		{
			(void)Name;
			Action.bPrevious = Action.bCurrent;
			Action.bCurrent = false;
			for (const auto& Binding : Action.Bindings)
			{
				Action.bCurrent = Action.bCurrent || IsBindingDown_Internal(Input, Binding);
			}
		}
	}
	bool IsDown(const std::string& Action) const
	{
		const auto* State = Find_Internal(Action);
		return State && State->bCurrent;
	}
	bool WasPressed(const std::string& Action) const
	{
		const auto* State = Find_Internal(Action);
		return State && State->bCurrent && !State->bPrevious;
	}
	bool WasReleased(const std::string& Action) const
	{
		const auto* State = Find_Internal(Action);
		return State && !State->bCurrent && State->bPrevious;
	}
	float GetAxis(const std::string& NegativeAction, const std::string& PositiveAction) const
	{
		return static_cast<float>(IsDown(PositiveAction)) - static_cast<float>(IsDown(NegativeAction));
	}
private:
	enum class EDevice
	{
		Keyboard,
		Mouse,
		Gamepad
	};
	struct FBinding
	{
		EDevice Device;
		std::size_t Button;
		std::size_t Pad;
		bool operator==(const FBinding&) const = default;
	};
	struct FActionState
	{
		std::vector<FBinding> Bindings;
		bool bCurrent = false;
		bool bPrevious = false;
	};
	bool Bind_Internal(std::string Action, FBinding Binding)
	{
		if (Action.empty())
		{
			return false;
		}
		auto& Bindings = m_Actions[std::move(Action)].Bindings;
		if (std::find(Bindings.begin(), Bindings.end(), Binding) == Bindings.end())
		{
			Bindings.push_back(Binding);
		}
		return true;
	}
	static bool IsBindingDown_Internal(const FInputSnapshot& Input, const FBinding& Binding) noexcept
	{
		switch (Binding.Device)
		{
		case EDevice::Keyboard:
			return Input.IsDown(static_cast<EKey>(Binding.Button));
		case EDevice::Mouse:
			return Input.IsMouseDown(static_cast<EMouseButton>(Binding.Button));
		case EDevice::Gamepad:
			return Input.IsPadDown(Binding.Pad, Binding.Button);
		}
		return false;
	}
	const FActionState* Find_Internal(const std::string& Name) const
	{
		const auto It = m_Actions.find(Name);
		return It == m_Actions.end() ? nullptr : &It->second;
	}
	std::unordered_map<std::string, FActionState> m_Actions;
};
}
