// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_USER_SETTINGS_H
#define DXF_USER_SETTINGS_H
#include "Dxf/InputProfile.h"
namespace Dxf
{
/**
 * ゲーム進行と独立して永続化する操作・表示・音の値。適用と保存成功は別。
 */
struct FUserSettings
{
	/**
	 * 設定payloadのバージョン。現在は1のみ。
	 */
	Toolbox::uint32 SettingsVersion = 1;
	/**
	 * 完全な入力割当。
	 */
	FInputProfile Input;
	/**
	 * 元の再生音量へ掛ける倍率。有限な0〜1。
	 */
	Toolbox::f32 Volume = 1.0f;
	/**
	 * UIの描画とhit testへ同時に渡す倍率。有限な0.25〜8。
	 */
	Toolbox::f32 UiScale = 1.0f;
};
} // namespace Dxf
#endif
