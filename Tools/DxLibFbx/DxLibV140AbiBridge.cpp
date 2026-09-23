// SPDX-License-Identifier: NOASSERTION
// 公式VC SDKの DxUseCLib_vs2015_*.lib（VS2015 v140でビルド）と、現在のMSVCでビルドしたDxLib本体を
// 同じ実行ファイルへリンクするための名前修飾の橋渡し。DxLibのソースは変更しない。
//
// DxMovie.h は namespace DxLib の中で、未宣言の struct SETUP_GRAPHHANDLE_GPARAM を関数引数の中で
// 初めて参照する。v140はこの名前を大域名前空間として扱い、公式ライブラリの修飾名は
// ?...@@YA...PEAUSETUP_GRAPHHANDLE_GPARAM@@... になる。現在の規格準拠のMSVCは DxLib::SETUP_GRAPHHANDLE_GPARAM
// として扱うため、同じ関数でも修飾名が異なる（...GPARAM@1@...）。構造体の定義とレイアウトは同一である。
//
// ここでは公式 DxUseCLib が参照する3関数だけを、旧来の引数型で受けて本物の実装へ転送する。
// 処理の代替や空実装ではない。対象はリンク時の未解決シンボルから特定し、Validationへ記録する。
#define DX_MAKE
#include "DxGraphics.h"

// v140が大域名前空間に置いた同名の構造体。定義は持たず、ポインタの型としてだけ使う。
struct SETUP_GRAPHHANDLE_GPARAM;

namespace DxLib
{
namespace
{
// 大域名の不完全型へのポインタを、同じ定義を持つDxLib内の型へ戻す。
SETUP_GRAPHHANDLE_GPARAM* ToDxLib_Internal(::SETUP_GRAPHHANDLE_GPARAM* GParam)
{
	return reinterpret_cast<SETUP_GRAPHHANDLE_GPARAM*>(GParam);
}
} // namespace

// 公式 DxUseCLib（v140）の参照先: ?Graphics_Image_MakeGraph_UseGParam@DxLib@@YAHPEAUSETUP_GRAPHHANDLE_GPARAM@@HHHHHHH@Z
int Graphics_Image_MakeGraph_UseGParam(::SETUP_GRAPHHANDLE_GPARAM* GParam, int SizeX, int SizeY, int NotUse3DFlag,
                                       int UsePaletteFlag, int PaletteBitDepth, int ASyncLoadFlag, int ASyncThread)
{
	return Graphics_Image_MakeGraph_UseGParam(ToDxLib_Internal(GParam), SizeX, SizeY, NotUse3DFlag, UsePaletteFlag,
	                                          PaletteBitDepth, ASyncLoadFlag, ASyncThread);
}

// 公式 DxUseCLib（v140）の参照先: ?Graphics_Image_InitSetupGraphHandleGParam@DxLib@@YAXPEAUSETUP_GRAPHHANDLE_GPARAM@@@Z
void Graphics_Image_InitSetupGraphHandleGParam(::SETUP_GRAPHHANDLE_GPARAM* GParam)
{
	Graphics_Image_InitSetupGraphHandleGParam(ToDxLib_Internal(GParam));
}

// 公式 DxUseCLib（v140）の参照先: ?Graphics_Image_InitSetupGraphHandleGParam_Normal_NonDrawValid@DxLib@@YAXPEAUSETUP_GRAPHHANDLE_GPARAM@@HHH@Z
void Graphics_Image_InitSetupGraphHandleGParam_Normal_NonDrawValid(::SETUP_GRAPHHANDLE_GPARAM* GParam, int BitDepth,
                                                                   int AlphaChannel, int AlphaTest)
{
	Graphics_Image_InitSetupGraphHandleGParam_Normal_NonDrawValid(ToDxLib_Internal(GParam), BitDepth, AlphaChannel,
	                                                              AlphaTest);
}
} // namespace DxLib
