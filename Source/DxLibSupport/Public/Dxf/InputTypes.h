#pragma once
#include <array>
#include <cstddef>

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
}
