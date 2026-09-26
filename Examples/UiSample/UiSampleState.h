// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SAMPLE_STATE_H
#define DXF_UI_SAMPLE_STATE_H
#include "Dxf/UiProperty.h"
#include "Toolbox/String.h"
namespace Dxf::UiSample
{
/**
 * サンプル固有の操作要求。UIはNavigatorやPhysicsを直接所有しない。
 */
enum class EUiSampleAction : Toolbox::uint8
{
	None,
	Start2D,
	Start3D,
	Title,
	Quit,
	Pause,
	Settings,
	CloseSettings,
	PlaySound,
	ReloadStyle,
	ConfirmTitle,
	CancelConfirm
};
/**
 * Scene間で共有するゲームの表示用状態。フレームワーク共通の設定管理ではない。
 */
struct FUiSampleState
{
	TUiProperty<Toolbox::f64> Volume{0.5};
	TUiProperty<Toolbox::f64> Scale{1.0};
	TUiProperty<bool> Split{false};
	TUiProperty<Toolbox::FString> Status{"Ready"};
	TUiProperty<Toolbox::FString> Detail{"項目を選択してください"};
	TUiProperty<Toolbox::FString> Notice;
	EUiSampleAction Action = EUiSampleAction::None;
};
} // namespace Dxf::UiSample
#endif
