#pragma once
#include "Dxf/InputSnapshot.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

namespace Dxf
{
class FInputMap
{
public:
	void Bind(std::string Action, EKey Key)
	{
		m_Actions[std::move(Action)].Keys.push_back(Key);
	}
	void Update(const FInputSnapshot& Input)
	{
		for (auto& [Name, Action] : m_Actions)
		{
			(void)Name;
			Action.bPrevious = Action.bCurrent;
			Action.bCurrent = false;
			for (EKey Key : Action.Keys)
			{
				Action.bCurrent = Action.bCurrent || Input.IsDown(Key);
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
private:
	struct FActionState
	{
		std::vector<EKey> Keys;
		bool bCurrent = false;
		bool bPrevious = false;
	};
	const FActionState* Find_Internal(const std::string& Name) const
	{
		const auto It = m_Actions.find(Name);
		return It == m_Actions.end() ? nullptr : &It->second;
	}
	std::unordered_map<std::string, FActionState> m_Actions;
};
}
