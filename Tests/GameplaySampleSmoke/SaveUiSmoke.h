// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAVE_UI_SMOKE_H
#define DXF_GAMEPLAY_SAVE_UI_SMOKE_H
#include "SmokeSupport.h"
#include "SaveCourseGameInstance.h"
#include "../UiNativeSmoke/ForwardRenderer.h"
namespace Dxf::GameplaySmoke
{
/**
 * 実保存メニューの倍率と画素を照合し、同じ位置のクリックが設定に届くことを確認する。
 * @param App 通常更新と固定Snapshotを使う実Application。
 * @param Game 現在の独立した設定の所有者。
 * @param Render すべて実DxLibへ送る観察用の描画窓口。
 * @param Backends 実フォントの計測を行う窓口。
 * @param Output 今回専用の画像保存先。
 * @param CompletedSteps 対象Sceneの成功固定更新数を読む非所有の処理。
 */
void RunSaveUiSmoke(FSmokeApp& App, GameplaySample::ASaveCourseGameInstance& Game, UiSmoke::FForwardRenderer& Render, FDxLibBackends& Backends, const Toolbox::FPath& Output, Toolbox::TFunction<Toolbox::uint64()> CompletedSteps);
} // namespace Dxf::GameplaySmoke
#endif
