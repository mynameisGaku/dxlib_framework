#pragma once
// HANDWRITTEN TEST DOUBLE: verifies call translation, NOT real SDK signatures, ABI or devices.
#include <array>
#include <algorithm>
#include <string>
#define DX_CHARCODEFORMAT_UTF8 1000
#define DX_SOUNDDATATYPE_MEMNOPRESS 1001
#define DX_SOUNDDATATYPE_FILE 1002
#define DX_PLAYTYPE_LOOP 1003
#define DX_PLAYTYPE_BACK 1004
#define DX_FONTTYPE_ANTIALIASING_4X4 1005
#define DX_FONTTYPE_NORMAL 1006
#define DX_DRAWMODE_BILINEAR 1007
#define DX_BLENDMODE_NOBLEND 1008
#define DX_BLENDMODE_ALPHA 1009
#define KEY_INPUT_A 0
#define KEY_INPUT_B 1
#define KEY_INPUT_C 2
#define KEY_INPUT_D 3
#define KEY_INPUT_E 4
#define KEY_INPUT_F 5
#define KEY_INPUT_G 6
#define KEY_INPUT_H 7
#define KEY_INPUT_I 8
#define KEY_INPUT_J 9
#define KEY_INPUT_K 10
#define KEY_INPUT_L 11
#define KEY_INPUT_M 12
#define KEY_INPUT_N 13
#define KEY_INPUT_O 14
#define KEY_INPUT_P 15
#define KEY_INPUT_Q 16
#define KEY_INPUT_R 17
#define KEY_INPUT_S 18
#define KEY_INPUT_T 19
#define KEY_INPUT_U 20
#define KEY_INPUT_V 21
#define KEY_INPUT_W 22
#define KEY_INPUT_X 23
#define KEY_INPUT_Y 24
#define KEY_INPUT_Z 25
#define KEY_INPUT_0 26
#define KEY_INPUT_1 27
#define KEY_INPUT_2 28
#define KEY_INPUT_3 29
#define KEY_INPUT_4 30
#define KEY_INPUT_5 31
#define KEY_INPUT_6 32
#define KEY_INPUT_7 33
#define KEY_INPUT_8 34
#define KEY_INPUT_9 35
#define KEY_INPUT_SPACE 36
#define KEY_INPUT_ESCAPE 37
#define KEY_INPUT_RETURN 38
#define KEY_INPUT_TAB 39
#define KEY_INPUT_BACK 40
#define KEY_INPUT_LEFT 41
#define KEY_INPUT_RIGHT 42
#define KEY_INPUT_UP 43
#define KEY_INPUT_DOWN 44
#define KEY_INPUT_LSHIFT 45
#define KEY_INPUT_RSHIFT 46
#define KEY_INPUT_LCONTROL 47
#define KEY_INPUT_RCONTROL 48
#define KEY_INPUT_F1 49
#define KEY_INPUT_F2 50
#define KEY_INPUT_F3 51
#define KEY_INPUT_F4 52
#define KEY_INPUT_F5 53
#define KEY_INPUT_F6 54
#define KEY_INPUT_F7 55
#define KEY_INPUT_F8 56
#define KEY_INPUT_F9 57
#define KEY_INPUT_F10 58
#define KEY_INPUT_F11 59
#define KEY_INPUT_F12 60
#define PAD_INPUT_1 1
#define PAD_INPUT_2 2
#define PAD_INPUT_3 4
#define PAD_INPUT_4 8
#define PAD_INPUT_5 16
#define PAD_INPUT_6 32
#define PAD_INPUT_7 64
#define PAD_INPUT_8 128
#define PAD_INPUT_9 256
#define PAD_INPUT_10 512
#define PAD_INPUT_11 1024
#define PAD_INPUT_12 2048
#define PAD_INPUT_13 4096
#define PAD_INPUT_14 8192
#define PAD_INPUT_15 16384
#define PAD_INPUT_16 32768
#define DX_INPUT_PAD1 256
#define DX_INPUT_PAD2 512
#define DX_INPUT_PAD3 768
#define DX_INPUT_PAD4 1024
#define TRUE 1
#define FALSE 0
#define DX_SCREEN_BACK -2
#define MOUSE_INPUT_LEFT 1
#define MOUSE_INPUT_RIGHT 2
#define MOUSE_INPUT_MIDDLE 4
namespace DxLib
{
struct FNativeTrace
{
	int NextHandle = 1, Ends = 0, CharCode = 0, DeletedGraphs = 0, SoundType = 0, LoadedSoundType = 0;
	int Volume = 0, PadCount = 0, PadButtons = 0, PadX = 0, PadY = 0, LastPad = 0, MouseButtons = 0;
	int Alpha = 255, Presentations = 0, Target = DX_SCREEN_BACK;
	bool bFailInit = false, bFailSize = false, bFailKeys = false, bFailDraw = false, bActive = true;
	std::string Path, Text;
	std::array<int, 3> Brightness{255, 255, 255};
	std::array<float, 8> Vertices{};
	std::array<char, 256> Keys{};
};
inline FNativeTrace Trace;
inline int SetUseCharCodeFormat(int Value)
{
	Trace.CharCode = Value;
	return 0;
}
inline int SetMainWindowText(const char*)
{
	return 0;
}
inline int ChangeWindowMode(int)
{
	return 0;
}
inline int SetGraphMode(int, int, int)
{
	return 0;
}
inline int SetWaitVSyncFlag(int)
{
	return 0;
}
inline int SetAlwaysRunFlag(int)
{
	return 0;
}
inline int DxLib_Init()
{
	return Trace.bFailInit ? -1 : 0;
}
inline int DxLib_End()
{
	++Trace.Ends;
	return 0;
}
inline int ProcessMessage()
{
	return 0;
}
inline int GetWindowActiveFlag()
{
	return Trace.bActive ? TRUE : FALSE;
}
inline int GetHitKeyStateAll(char* Keys)
{
	std::copy(Trace.Keys.begin(), Trace.Keys.end(), Keys);
	return Trace.bFailKeys ? -1 : 0;
}
inline int GetMousePoint(int* X, int* Y)
{
	*X = 0;
	*Y = 0;
	return 0;
}
inline int GetMouseInput()
{
	return Trace.MouseButtons;
}
inline int GetMouseWheelRotVol()
{
	return 0;
}
inline int GetJoypadNum()
{
	return Trace.PadCount;
}
inline int GetJoypadInputState(int Pad)
{
	Trace.LastPad = Pad;
	return Trace.PadButtons;
}
inline int GetJoypadAnalogInput(int* X, int* Y, int Pad)
{
	Trace.LastPad = Pad;
	*X = Trace.PadX;
	*Y = Trace.PadY;
	return 0;
}
inline int LoadGraph(const char* Path, int)
{
	Trace.Path = Path;
	return Trace.NextHandle++;
}
inline int GetGraphSize(int, int* X, int* Y)
{
	*X = 64;
	*Y = 64;
	return Trace.bFailSize ? -1 : 0;
}
inline int DeleteGraph(int)
{
	++Trace.DeletedGraphs;
	return 0;
}
inline int MakeScreen(int, int, int)
{
	return Trace.NextHandle++;
}
inline int SetCreateSoundDataType(int Type)
{
	Trace.SoundType = Type;
	return 0;
}
inline int LoadSoundMem(const char*)
{
	Trace.LoadedSoundType = Trace.SoundType;
	return Trace.NextHandle++;
}
inline int DuplicateSoundMem(int)
{
	return Trace.NextHandle++;
}
inline int PlaySoundMem(int, int, int)
{
	return 0;
}
inline int StopSoundMem(int)
{
	return 0;
}
inline int DeleteSoundMem(int)
{
	return 0;
}
inline int ChangeVolumeSoundMem(int Volume, int)
{
	Trace.Volume = Volume;
	return 0;
}
inline int CheckSoundMem(int)
{
	return 0;
}
inline int CreateFontToHandle(const char*, int, int, int)
{
	return Trace.NextHandle++;
}
inline int DeleteFontToHandle(int)
{
	return 0;
}
inline int SetDrawScreen(int Handle)
{
	Trace.Target = Handle;
	return 0;
}
inline int SetDrawArea(int, int, int, int)
{
	return 0;
}
inline int SetDrawBlendMode(int, int Alpha)
{
	Trace.Alpha = Alpha;
	return 0;
}
inline int SetDrawBright(int R, int G, int B)
{
	Trace.Brightness = {R, G, B};
	return 0;
}
inline int SetDrawMode(int)
{
	return 0;
}
inline int SetUseZBufferFlag(int)
{
	return 0;
}
inline int SetWriteZBufferFlag(int)
{
	return 0;
}
inline int SetUseVertexShader(int)
{
	return 0;
}
inline int SetUsePixelShader(int)
{
	return 0;
}
inline int SetBackgroundColor(int, int, int)
{
	return 0;
}
inline int ClearDrawScreen()
{
	return 0;
}
inline unsigned int GetColor(int R, int G, int B)
{
	return (static_cast<unsigned int>(R) << 16) | (static_cast<unsigned int>(G) << 8) | static_cast<unsigned int>(B);
}
inline int DrawModiGraphF(float X1, float Y1, float X2, float Y2, float X3, float Y3, float X4, float Y4, int, int)
{
	Trace.Vertices = {X1, Y1, X2, Y2, X3, Y3, X4, Y4};
	return Trace.bFailDraw ? -1 : 0;
}
inline int DrawStringFToHandle(float, float, const char* Text, unsigned int, int)
{
	Trace.Text = Text;
	return Trace.bFailDraw ? -1 : 0;
}
inline int DrawBox(int, int, int, int, unsigned int, int)
{
	return Trace.bFailDraw ? -1 : 0;
}
inline int ScreenFlip()
{
	++Trace.Presentations;
	return 0;
}
}
