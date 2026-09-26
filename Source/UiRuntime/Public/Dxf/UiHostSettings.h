// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_HOST_SETTINGS_H
#define DXF_UI_HOST_SETTINGS_H
#include "Dxf/InputStateTracker.h"
#include "Dxf/RenderContext.h"
#include "Dxf/Scene.h"
#include "Dxf/UiNavigationBindings.h"
#include "Dxf/UiRoot.h"
#include "Dxf/UiWorldPanel.h"
#include "Toolbox/Array.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * ルートがキーボード・パッドの操作を受ける条件。
 */
enum class EUiNavigationPolicy : Toolbox::uint8
{
	/**
	 * 常に受ける（メニュー画面・ポーズ画面）。
	 */
	Always,
	/**
	 * フォーカスのある要素があるときだけ受ける（HUDのボタン等。ゲーム中の操作を奪わない）。
	 */
	WhenFocused,
	/**
	 * 受けない（ポインターだけ）。
	 */
	Never
};
/**
 * 表示先の種類。
 */
enum class EUiDisplayKind : Toolbox::uint8
{
	/**
	 * 全画面の重ね合わせ。
	 */
	Screen,
	/**
	 * 分割画面の一つの領域。
	 */
	Viewport,
	/**
	 * 2D世界の矩形のパネル。
	 */
	WorldPanel2D,
	/**
	 * 3D世界の平面のパネル（テクスチャへ描いて平面に貼る）。
	 */
	WorldPanel3D
};
/**
 * 表示先の設定。
 */
struct FUiDisplayOptions
{
	/**
	 * 倍率（ワールドのパネルでは描画先テクスチャの高さを基準にする）。
	 */
	FUiScaleSettings Scale;
	/**
	 * キーボード・パッドの操作を受ける条件。
	 */
	EUiNavigationPolicy Navigation = EUiNavigationPolicy::WhenFocused;
	/**
	 * 操作を受ける入力プレイヤー。
	 */
	Toolbox::int32 Player = 0;
	/**
	 * 2D命令のレイヤー（全画面のUIはViewportのUI・ワールドのパネルより大きい値にする）。
	 */
	Toolbox::int32 Layer = 1000;
	/**
	 * Modalの間、その入力プレイヤーのキー・パッドの入力をゲームへ届けない。
	 */
	bool bBlockGameWhileModal = true;
};
/**
 * 表示先の識別（0は無効）。
 */
using FUiDisplayId = Toolbox::uint64;
/**
 * 一フレームの仲介の結果（診断・試験用）。
 */
struct FUiRoutingResult
{
	/**
	 * ポインターを受けた表示先（0はゲームへ通した）。
	 */
	FUiDisplayId PointerDisplay = 0;
	/**
	 * UIが持っている（ゲームへ届けない）キーの数。
	 */
	Toolbox::size_t OwnedKeys = 0;
	/**
	 * UIが持っているマウスのボタンの数。
	 */
	Toolbox::size_t OwnedMouseButtons = 0;
	/**
	 * UIが持っているパッドのボタンの数。
	 */
	Toolbox::size_t OwnedPadButtons = 0;
	/**
	 * ホイールをUIが使ったか。
	 */
	bool bWheelConsumed = false;
	/**
	 * 入力を処理したルートの数（同じルートは一度だけ）。
	 */
	Toolbox::size_t ProcessedRoots = 0;
	/**
	 * 入力プレイヤーごとの、操作を受けた表示先（0はなし）。
	 */
	Toolbox::TArray<FUiDisplayId, 4> NavigationDisplays{};
};
/**
 * 仲介の設定。
 */
struct FUiSceneHostSettings
{
	/**
	 * 操作の割当。
	 */
	FUiNavigationBindings Bindings = FUiNavigationBindings::MakeDefault();
	/**
	 * 入力プレイヤーの機器（既定: 0=キーボード＋パッド0、1〜3=パッド1〜3）。
	 */
	Toolbox::TArray<FUiPlayerDevices, 4> Players{FUiPlayerDevices{true, 0}, FUiPlayerDevices{false, 1},
	                                             FUiPlayerDevices{false, 2}, FUiPlayerDevices{false, 3}};
	/**
	 * 描画の前に入力が来た場合の画面の大きさ（描画のたびに実際の描画先の大きさへ更新する）。
	 */
	Toolbox::int32 ScreenWidth = 1280;
	/**
	 * 同上の高さ。
	 */
	Toolbox::int32 ScreenHeight = 720;
};
} // namespace Dxf
// namespace Dxf
#endif
