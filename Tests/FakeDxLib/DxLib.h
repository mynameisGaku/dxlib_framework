#pragma once
// HANDWRITTEN TEST DOUBLE: verifies call translation, NOT real SDK signatures, ABI or devices.
#include "Toolbox/Array.h"
#include "Toolbox/Algorithm.h"
#include "Toolbox/String.h"
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
/**
 * ネイティブAPI呼び出しの引数と状態の記録。
 */
struct FNativeTrace
{
	/**
	 * 次に発行する疑似資源番号。
	 */
	Toolbox::int32 NextHandle = 1, Ends = 0, CharCode = 0, DeletedGraphs = 0, SoundType = 0, LoadedSoundType = 0;
	/**
	 * 検証する再生音量。
	 */
	Toolbox::int32 Volume = 0, PadCount = 0, PadButtons = 0, PadX = 0, PadY = 0, LastPad = 0, MouseButtons = 0;
	/**
	 * 設定された透過度・表示回数・描画先番号。
	 */
	Toolbox::int32 Alpha = 255, Presentations = 0, Target = DX_SCREEN_BACK;
	/**
	 * 初期化失敗を発生させるか。
	 */
	bool bFailInit = false, bFailSize = false, bFailKeys = false, bFailDraw = false, bActive = true;
	/**
	 * 読み込みパスと描画文字列の記録。
	 */
	Toolbox::FString Path, Text;
	/**
	 * 描画時に設定されたRGBの明るさ。
	 */
	Toolbox::TArray<Toolbox::int32, 3> Brightness{255, 255, 255};
	/**
	 * 変形画像描画で受け取った四頂点の座標。
	 */
	Toolbox::TArray<Toolbox::f32, 8> Vertices{};
	/**
	 * キーごとの押下状態。
	 */
	Toolbox::TArray<char, 256> Keys{};
};
/**
 * 操作順序と引数の観測記録。
 */
inline FNativeTrace Trace;
/**
 * 文字コード設定のAPI呼び出しを再現する。
 */
inline Toolbox::int32 SetUseCharCodeFormat(Toolbox::int32 Value)
{
	Trace.CharCode = Value;
	return 0;
}
/**
 * ウィンドウ名設定を再現する。
 */
inline Toolbox::int32 SetMainWindowText(const char*)
{
	return 0;
}
/**
 * ウィンドウモード設定を再現する。
 */
inline Toolbox::int32 ChangeWindowMode(Toolbox::int32)
{
	return 0;
}
/**
 * 画面サイズ設定を再現する。
 */
inline Toolbox::int32 SetGraphMode(Toolbox::int32, Toolbox::int32, Toolbox::int32)
{
	return 0;
}
/**
 * 垂直同期設定を再現する。
 */
inline Toolbox::int32 SetWaitVSyncFlag(Toolbox::int32)
{
	return 0;
}
/**
 * 非アクティブ時の実行設定を再現する。
 */
inline Toolbox::int32 SetAlwaysRunFlag(Toolbox::int32)
{
	return 0;
}
/**
 * ネイティブ初期化の呼び出しを再現する。
 */
inline Toolbox::int32 DxLib_Init()
{
	return Trace.bFailInit ? -1 : 0;
}
/**
 * ネイティブ終了の呼び出しを再現する。
 */
inline Toolbox::int32 DxLib_End()
{
	++Trace.Ends;
	return 0;
}
/**
 * ネイティブイベント処理を再現する。
 */
inline Toolbox::int32 ProcessMessage()
{
	return 0;
}
/**
 * 検証用のウィンドウ稼働状態を返す。
 */
inline Toolbox::int32 GetWindowActiveFlag()
{
	return Trace.bActive ? TRUE : FALSE;
}
/**
 * 検証用の全キー状態を書き込む。
 */
inline Toolbox::int32 GetHitKeyStateAll(char* Keys)
{
	Toolbox::Copy(Trace.Keys.Begin(), Trace.Keys.End(), Keys);
	return Trace.bFailKeys ? -1 : 0;
}
/**
 * 検証用のマウス座標を書き込む。
 */
inline Toolbox::int32 GetMousePoint(Toolbox::int32* X, Toolbox::int32* Y)
{
	*X = 0;
	*Y = 0;
	return 0;
}
/**
 * 検証用のマウスボタン状態を返す。
 */
inline Toolbox::int32 GetMouseInput()
{
	return Trace.MouseButtons;
}
/**
 * 検証用のホイール移動量を返す。
 */
inline Toolbox::int32 GetMouseWheelRotVol()
{
	return 0;
}
/**
 * 検証用のゲームパッド接続数を返す。
 */
inline Toolbox::int32 GetJoypadNum()
{
	return Trace.PadCount;
}
/**
 * 検証用のゲームパッドボタン状態を返す。
 */
inline Toolbox::int32 GetJoypadInputState(Toolbox::int32 Pad)
{
	Trace.LastPad = Pad;
	return Trace.PadButtons;
}
/**
 * 検証用のアナログスティック値を書き込む。
 */
inline Toolbox::int32 GetJoypadAnalogInput(Toolbox::int32* X, Toolbox::int32* Y, Toolbox::int32 Pad)
{
	Trace.LastPad = Pad;
	*X = Trace.PadX;
	*Y = Trace.PadY;
	return 0;
}
/**
 * 画像読み込みAPIの呼び出しを再現する。
 */
inline Toolbox::int32 LoadGraph(const char* Path, Toolbox::int32)
{
	Trace.Path = Path;
	return Trace.NextHandle++;
}
/**
 * 検証用画像のサイズを返す。
 */
inline Toolbox::int32 GetGraphSize(Toolbox::int32, Toolbox::int32* X, Toolbox::int32* Y)
{
	*X = 64;
	*Y = 64;
	return Trace.bFailSize ? -1 : 0;
}
/**
 * 画像解放APIの呼び出しを再現する。
 */
inline Toolbox::int32 DeleteGraph(Toolbox::int32)
{
	++Trace.DeletedGraphs;
	return 0;
}
/**
 * 描画先生成APIの呼び出しを再現する。
 */
inline Toolbox::int32 MakeScreen(Toolbox::int32, Toolbox::int32, Toolbox::int32)
{
	return Trace.NextHandle++;
}
/**
 * 音声の読み込み方式設定を再現する。
 */
inline Toolbox::int32 SetCreateSoundDataType(Toolbox::int32 Type)
{
	Trace.SoundType = Type;
	return 0;
}
/**
 * 音声読み込みAPIの呼び出しを再現する。
 */
inline Toolbox::int32 LoadSoundMem(const char*)
{
	Trace.LoadedSoundType = Trace.SoundType;
	return Trace.NextHandle++;
}
/**
 * 音声複製APIの呼び出しを再現する。
 */
inline Toolbox::int32 DuplicateSoundMem(Toolbox::int32)
{
	return Trace.NextHandle++;
}
/**
 * 音声再生APIの呼び出しを再現する。
 */
inline Toolbox::int32 PlaySoundMem(Toolbox::int32, Toolbox::int32, Toolbox::int32)
{
	return 0;
}
/**
 * 音声停止APIの呼び出しを再現する。
 */
inline Toolbox::int32 StopSoundMem(Toolbox::int32)
{
	return 0;
}
/**
 * 音声解放APIの呼び出しを再現する。
 */
inline Toolbox::int32 DeleteSoundMem(Toolbox::int32)
{
	return 0;
}
/**
 * ネイティブ音量設定の引数を記録する。
 */
inline Toolbox::int32 ChangeVolumeSoundMem(Toolbox::int32 Volume, Toolbox::int32)
{
	Trace.Volume = Volume;
	return 0;
}
/**
 * 検証用の音声再生状態を返す。
 */
inline Toolbox::int32 CheckSoundMem(Toolbox::int32)
{
	return 0;
}
/**
 * フォント生成APIの呼び出しを再現する。
 */
inline Toolbox::int32 CreateFontToHandle(const char*, Toolbox::int32, Toolbox::int32, Toolbox::int32)
{
	return Trace.NextHandle++;
}
/**
 * フォント解放APIの呼び出しを再現する。
 */
inline Toolbox::int32 DeleteFontToHandle(Toolbox::int32)
{
	return 0;
}
/**
 * 描画先設定の引数を記録する。
 */
inline Toolbox::int32 SetDrawScreen(Toolbox::int32 Handle)
{
	Trace.Target = Handle;
	return 0;
}
/**
 * 描画範囲設定を再現する。
 */
inline Toolbox::int32 SetDrawArea(Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::int32)
{
	return 0;
}
/**
 * 透過合成設定の引数を記録する。
 */
inline Toolbox::int32 SetDrawBlendMode(Toolbox::int32, Toolbox::int32 Alpha)
{
	Trace.Alpha = Alpha;
	return 0;
}
/**
 * 描画色設定の引数を記録する。
 */
inline Toolbox::int32 SetDrawBright(Toolbox::int32 R, Toolbox::int32 G, Toolbox::int32 B)
{
	Trace.Brightness = {R, G, B};
	return 0;
}
/**
 * 画像補間方式の設定を再現する。
 */
inline Toolbox::int32 SetDrawMode(Toolbox::int32)
{
	return 0;
}
/**
 * 深度判定設定を再現する。
 */
inline Toolbox::int32 SetUseZBufferFlag(Toolbox::int32)
{
	return 0;
}
/**
 * 深度書き込み設定を再現する。
 */
inline Toolbox::int32 SetWriteZBufferFlag(Toolbox::int32)
{
	return 0;
}
/**
 * 頂点シェーダー設定を再現する。
 */
inline Toolbox::int32 SetUseVertexShader(Toolbox::int32)
{
	return 0;
}
/**
 * ピクセルシェーダー設定を再現する。
 */
inline Toolbox::int32 SetUsePixelShader(Toolbox::int32)
{
	return 0;
}
/**
 * 背景色設定を再現する。
 */
inline Toolbox::int32 SetBackgroundColor(Toolbox::int32, Toolbox::int32, Toolbox::int32)
{
	return 0;
}
/**
 * 画面消去APIの呼び出しを再現する。
 */
inline Toolbox::int32 ClearDrawScreen()
{
	return 0;
}
/**
 * RGB成分をネイティブ色値へまとめる。
 */
inline Toolbox::uint32 GetColor(Toolbox::int32 R, Toolbox::int32 G, Toolbox::int32 B)
{
	return (static_cast<Toolbox::uint32>(R) << 16) | (static_cast<Toolbox::uint32>(G) << 8) |
	       static_cast<Toolbox::uint32>(B);
}
/**
 * 変形画像描画の座標と資源番号を記録する。
 */
inline Toolbox::int32 DrawModiGraphF(Toolbox::f32 X1, Toolbox::f32 Y1, Toolbox::f32 X2, Toolbox::f32 Y2,
                                     Toolbox::f32 X3, Toolbox::f32 Y3, Toolbox::f32 X4, Toolbox::f32 Y4, Toolbox::int32,
                                     Toolbox::int32)
{
	Trace.Vertices = {X1, Y1, X2, Y2, X3, Y3, X4, Y4};
	return Trace.bFailDraw ? -1 : 0;
}
/**
 * フォント付き文字描画の呼び出しを再現する。
 */
inline Toolbox::int32 DrawStringFToHandle(Toolbox::f32, Toolbox::f32, const char* Text, Toolbox::uint32, Toolbox::int32)
{
	Trace.Text = Text;
	return Trace.bFailDraw ? -1 : 0;
}
/**
 * 矩形描画APIの呼び出しを再現する。
 */
inline Toolbox::int32 DrawBox(Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::uint32,
                              Toolbox::int32)
{
	return Trace.bFailDraw ? -1 : 0;
}
/**
 * 画面表示APIの呼び出しを再現する。
 */
inline Toolbox::int32 ScreenFlip()
{
	++Trace.Presentations;
	return 0;
}
} // namespace DxLib
