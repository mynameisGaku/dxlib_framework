#pragma once
// 個別Native翻訳テスト専用。製品のinclude pathには追加しない。
#include "Toolbox/Utility.h"
#define TRUE 1
#define FALSE 0
#define DX_SCREEN_BACK -2
#define DX_BLENDMODE_NOBLEND 0
#define DX_BLENDMODE_ALPHA 1
#define DX_BLENDMODE_PMA_ALPHA 17
#define DX_DRAWMODE_BILINEAR 1
namespace DxLib
{
inline Toolbox::int32 TestPresentCount = 0;
inline Toolbox::int32 TestBoxFill = -1;
inline Toolbox::int32 SetDrawScreen(Toolbox::int32) { return 0; }
// 描画範囲の設定・復帰を故障注入で観察する。
inline Toolbox::int32 TestAreaCalls = 0;
inline Toolbox::int32 TestAreaFailAt = -1;
inline Toolbox::int32 TestAreaLeft = 0;
inline Toolbox::int32 TestAreaRight = 0;
inline Toolbox::int32 SetDrawArea(Toolbox::int32 Left, Toolbox::int32, Toolbox::int32 Right, Toolbox::int32)
{
	if (++TestAreaCalls == TestAreaFailAt)
	{
		return -1;
	}
	TestAreaLeft = Left;
	TestAreaRight = Right;
	return 0;
}
inline Toolbox::int32 TestBlendAlpha = 255;
inline Toolbox::int32 SetDrawBlendMode(Toolbox::int32, Toolbox::int32 Alpha)
{
	TestBlendAlpha = Alpha;
	return 0;
}
inline Toolbox::int32 SetDrawBright(Toolbox::int32, Toolbox::int32, Toolbox::int32) { return 0; }
inline Toolbox::int32 SetDrawMode(Toolbox::int32) { return 0; }
inline Toolbox::int32 SetUseZBufferFlag(Toolbox::int32) { return 0; }
inline Toolbox::int32 SetWriteZBufferFlag(Toolbox::int32) { return 0; }
inline Toolbox::int32 SetUseVertexShader(Toolbox::int32) { return 0; }
inline Toolbox::int32 SetUsePixelShader(Toolbox::int32) { return 0; }
inline Toolbox::int32 SetBackgroundColor(Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::int32 = 0)
{
	return 0;
}
inline Toolbox::int32 ClearDrawScreen() { return 0; }
inline Toolbox::uint32 GetColor(Toolbox::int32 R, Toolbox::int32 G, Toolbox::int32 B)
{ return (static_cast<Toolbox::uint32>(R) << 16) | (static_cast<Toolbox::uint32>(G) << 8) | static_cast<Toolbox::uint32>(B); }
inline Toolbox::int32 DrawModiGraphF(Toolbox::f32, Toolbox::f32, Toolbox::f32, Toolbox::f32, Toolbox::f32, Toolbox::f32, Toolbox::f32, Toolbox::f32, Toolbox::int32, Toolbox::int32) { return 0; }
inline Toolbox::int32 DrawStringFToHandle(Toolbox::f32, Toolbox::f32, const char*, Toolbox::uint32, Toolbox::int32) { return 0; }
inline Toolbox::int32 DrawBox(Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::uint32, Toolbox::int32 Fill) { TestBoxFill = Fill; return 0; }
inline Toolbox::int32 GetGraphSize(Toolbox::int32, Toolbox::int32* W, Toolbox::int32* H)
{
	*W = 640;
	*H = 480;
	return 0;
}
inline Toolbox::int32 ScreenFlip() { ++TestPresentCount; return 0; }
}
#include "RenderViewsApi.h"
#include "ModelApi.h"
