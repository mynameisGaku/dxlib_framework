#include "Support/Test.h"
#include "Dxf/NativeBackends.h"

// 実SDKの取り込みによって後から追加されるWindowsマクロを再現する。
#define CreateFont CreateFontA
#define DrawText DrawTextA
#include "../Source/Native/Private/NativeApi.h"

#ifdef CreateFont
#error NativeApi must remove the Windows CreateFont macro
#endif
#ifdef DrawText
#error NativeApi must remove the Windows DrawText macro
#endif

// SDKの取り込み後もメンバー名を保ち、フォント作成と描画の呼び出しを確認する。
TEST("Native API preserves framework member names after Windows macros")
{
	DxLib::Trace = {};
	// 代替SDKに対するフォント作成を担当する接続部。
	Dxf::FDxLibFontBackend Fonts;
	// 作成したフォントのハンドルと成否。
	/**
	 * 検証で使用するフォント資源。
	 */
	auto Font = Fonts.CreateFont({});
	REQUIRE(Font);
	Fonts.DeleteFont(Font.Value());
	// 無効なフォントを検出する描画側の接続部。
	Dxf::FDxLibRenderBackend Renderer;
	REQUIRE(!Renderer.DrawText({}));
}
