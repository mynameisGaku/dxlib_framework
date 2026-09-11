#pragma once
#include "Dxf/Result.h"
#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
namespace Dxf
{
enum class EKey : std::size_t
{
	A,
	B,
	C,
	D,
	E,
	F,
	G,
	H,
	I,
	J,
	K,
	L,
	M,
	N,
	O,
	P,
	Q,
	R,
	S,
	T,
	U,
	V,
	W,
	X,
	Y,
	Z,
	Num0,
	Num1,
	Num2,
	Num3,
	Num4,
	Num5,
	Num6,
	Num7,
	Num8,
	Num9,
	Space,
	Escape,
	Enter,
	Tab,
	Backspace,
	Left,
	Right,
	Up,
	Down,
	LeftShift,
	RightShift,
	LeftControl,
	RightControl,
	F1,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	F11,
	F12,
	Count
};
enum class EMouseButton : std::size_t
{
	Left,
	Right,
	Middle,
	Count
};
struct FGamepadState
{
	bool bConnected = false;
	std::array<bool, 16> Buttons{};
	float LeftX = 0.0f;
	float LeftY = 0.0f;
};
struct FRawInput
{
	std::array<bool, static_cast<std::size_t>(EKey::Count)> Keys{};
	std::array<bool, static_cast<std::size_t>(EMouseButton::Count)> MouseButtons{};
	std::array<FGamepadState, 4> Pads{};
	int MouseX = 0;
	int MouseY = 0;
	int Wheel = 0;
	bool bFocused = true;
};
class IInputSource
{
public:
	virtual ~IInputSource() = default;
	virtual TResult<FRawInput> Poll() = 0;
};
/** Immutable through its public API; reads never consume an edge. */
class FInputSnapshot
{
public:
	bool Down(EKey Key) const noexcept
	{
		return Read_Internal(m_Current.Keys, static_cast<std::size_t>(Key));
	}
	bool Pressed(EKey Key) const noexcept
	{
		return Down(Key) && !Read_Internal(m_Previous.Keys, static_cast<std::size_t>(Key));
	}
	bool Released(EKey Key) const noexcept
	{
		return !Down(Key) && Read_Internal(m_Previous.Keys, static_cast<std::size_t>(Key));
	}
	bool MouseDown(EMouseButton Button) const noexcept
	{
		return Read_Internal(m_Current.MouseButtons, static_cast<std::size_t>(Button));
	}
	bool MousePressed(EMouseButton Button) const noexcept
	{
		return MouseDown(Button) && !Read_Internal(m_Previous.MouseButtons, static_cast<std::size_t>(Button));
	}
	bool MouseReleased(EMouseButton Button) const noexcept
	{
		return !MouseDown(Button) && Read_Internal(m_Previous.MouseButtons, static_cast<std::size_t>(Button));
	}
	bool PadDown(std::size_t Pad, std::size_t Button) const noexcept
	{
		return PadRead_Internal(m_Current, Pad, Button);
	}
	bool PadPressed(std::size_t Pad, std::size_t Button) const noexcept
	{
		return PadDown(Pad, Button) && !PadRead_Internal(m_Previous, Pad, Button);
	}
	bool PadReleased(std::size_t Pad, std::size_t Button) const noexcept
	{
		return !PadDown(Pad, Button) && PadRead_Internal(m_Previous, Pad, Button);
	}
	const FRawInput& GetRaw() const noexcept
	{
		return m_Current;
	}
private:
	friend class FInputStateTracker;
	template <std::size_t N> static bool Read_Internal(const std::array<bool, N>& Values, std::size_t Index) noexcept
	{
		return Index < N && Values[Index];
	}
	static bool PadRead_Internal(const FRawInput& State, std::size_t Pad, std::size_t Button) noexcept
	{
		return Pad < State.Pads.size() && State.Pads[Pad].bConnected && Read_Internal(State.Pads[Pad].Buttons, Button);
	}
	FRawInput m_Current;
	FRawInput m_Previous;
};
class FInputStateTracker
{
public:
	void Advance(FRawInput Raw) noexcept
	{
		if (!Raw.bFocused)
		{
			Raw.Keys.fill(false);
			Raw.MouseButtons.fill(false);
			Raw.Pads = {};
			Raw.Wheel = 0;
		}
		for (auto& Pad : Raw.Pads)
		{
			if (!Pad.bConnected)
			{
				Pad = {};
			}
		}
		m_Snapshot.m_Previous = m_Snapshot.m_Current;
		m_Snapshot.m_Current = Raw;
	}
	const FInputSnapshot& GetSnapshot() const noexcept
	{
		return m_Snapshot;
	}
private:
	FInputSnapshot m_Snapshot;
};
class FInputSystem
{
public:
	explicit FInputSystem(IInputSource& Source) : m_pSource(&Source)
	{
	}
	TResult<void> Update()
	{
		auto Raw = m_pSource->Poll();
		if (!Raw)
		{
			return TResult<void>::Failure(Raw.Error());
		}
		m_Tracker.Advance(Raw.Value());
		return {};
	}
	const FInputSnapshot& GetSnapshot() const noexcept
	{
		return m_Tracker.GetSnapshot();
	}
private:
	IInputSource* m_pSource;
	FInputStateTracker m_Tracker;
};
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
				Action.bCurrent = Action.bCurrent || Input.Down(Key);
			}
		}
	}
	bool Down(const std::string& Action) const
	{
		const auto* State = Find_Internal(Action);
		return State && State->bCurrent;
	}
	bool Pressed(const std::string& Action) const
	{
		const auto* State = Find_Internal(Action);
		return State && State->bCurrent && !State->bPrevious;
	}
	bool Released(const std::string& Action) const
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
