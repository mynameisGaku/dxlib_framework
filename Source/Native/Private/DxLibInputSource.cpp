#include "Dxf/DxLibInputSource.h"
#include "NativeApi.h"
#include <algorithm>
namespace Dxf
{
TResult<FRawInput> FDxLibInputSource::Poll()
{
	FRawInput State;
	State.bFocused = DxLib::GetWindowActiveFlag() != FALSE;
	// Drain wheel input even while inactive, preventing a delayed burst after refocus.
	State.Wheel = DxLib::GetMouseWheelRotVol();
	if (!State.bFocused)
	{
		State.Wheel = 0;
		return TResult<FRawInput>::Success(State);
	}
	std::array<char, 256> Keys{};
	if (DxLib::GetHitKeyStateAll(Keys.data()) < 0 || DxLib::GetMousePoint(&State.MouseX, &State.MouseY) < 0)
	{
		return TResult<FRawInput>::Failure(EErrorCode::BackendFailure, "Input polling failed");
	}
	static constexpr std::array NativeKeys{
	KEY_INPUT_A, KEY_INPUT_B, KEY_INPUT_C, KEY_INPUT_D, KEY_INPUT_E, KEY_INPUT_F, KEY_INPUT_G, KEY_INPUT_H, KEY_INPUT_I, KEY_INPUT_J, KEY_INPUT_K, KEY_INPUT_L, KEY_INPUT_M, KEY_INPUT_N, KEY_INPUT_O, KEY_INPUT_P, KEY_INPUT_Q, KEY_INPUT_R, KEY_INPUT_S, KEY_INPUT_T, KEY_INPUT_U, KEY_INPUT_V, KEY_INPUT_W, KEY_INPUT_X, KEY_INPUT_Y, KEY_INPUT_Z, KEY_INPUT_0, KEY_INPUT_1, KEY_INPUT_2, KEY_INPUT_3, KEY_INPUT_4, KEY_INPUT_5, KEY_INPUT_6, KEY_INPUT_7, KEY_INPUT_8, KEY_INPUT_9, KEY_INPUT_SPACE, KEY_INPUT_ESCAPE, KEY_INPUT_RETURN, KEY_INPUT_TAB, KEY_INPUT_BACK, KEY_INPUT_LEFT, KEY_INPUT_RIGHT, KEY_INPUT_UP, KEY_INPUT_DOWN, KEY_INPUT_LSHIFT, KEY_INPUT_RSHIFT, KEY_INPUT_LCONTROL, KEY_INPUT_RCONTROL, KEY_INPUT_F1, KEY_INPUT_F2, KEY_INPUT_F3, KEY_INPUT_F4, KEY_INPUT_F5, KEY_INPUT_F6, KEY_INPUT_F7, KEY_INPUT_F8, KEY_INPUT_F9, KEY_INPUT_F10, KEY_INPUT_F11, KEY_INPUT_F12
	};
	static_assert(NativeKeys.size() == static_cast<std::size_t>(EKey::Count));
	for (std::size_t Index = 0; Index < NativeKeys.size(); ++Index)
	{
		State.Keys[Index] = Keys[static_cast<std::size_t>(NativeKeys[Index])] != 0;
	}
	const int Mouse = DxLib::GetMouseInput();
	if (Mouse < 0)
	{
		return TResult<FRawInput>::Failure(EErrorCode::BackendFailure, "Mouse polling failed");
	}
	State.MouseButtons = {(Mouse & MOUSE_INPUT_LEFT) != 0, (Mouse & MOUSE_INPUT_RIGHT) != 0, (Mouse & MOUSE_INPUT_MIDDLE) != 0};
	static constexpr std::array PadIds{DX_INPUT_PAD1, DX_INPUT_PAD2, DX_INPUT_PAD3, DX_INPUT_PAD4};
	static constexpr std::array Buttons{PAD_INPUT_1, PAD_INPUT_2, PAD_INPUT_3, PAD_INPUT_4, PAD_INPUT_5, PAD_INPUT_6, PAD_INPUT_7, PAD_INPUT_8, PAD_INPUT_9, PAD_INPUT_10, PAD_INPUT_11, PAD_INPUT_12, PAD_INPUT_13, PAD_INPUT_14, PAD_INPUT_15, PAD_INPUT_16};
	const int PadCount = std::clamp(DxLib::GetJoypadNum(), 0, static_cast<int>(State.Pads.size()));
	for (int Index = 0; Index < PadCount; ++Index)
	{
		const auto Position = static_cast<std::size_t>(Index);
		const int Bits = DxLib::GetJoypadInputState(PadIds[Position]);
		int X = 0, Y = 0;
		if (Bits < 0 || DxLib::GetJoypadAnalogInput(&X, &Y, PadIds[Position]) < 0)
		{
			continue;
			// A controller may disconnect between count and state queries.
		}
		auto& Pad = State.Pads[Position];
		Pad.bConnected = true;
		Pad.LeftX = static_cast<float>(std::clamp(X, -1000, 1000)) / 1000.0f;
		Pad.LeftY = static_cast<float>(std::clamp(Y, -1000, 1000)) / 1000.0f;
		for (std::size_t Button = 0; Button < Buttons.size(); ++Button)
		{
			Pad.Buttons[Button] = (Bits & Buttons[Button]) != 0;
		}
	}
	return TResult<FRawInput>::Success(State);
}
}
