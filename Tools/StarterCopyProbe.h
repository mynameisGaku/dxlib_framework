// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_STARTER_COPY_PROBE_H
#define DXF_STARTER_COPY_PROBE_H
#include "Dxf/Application.h"
/**
 * コピー先Starterの検証だけに使う固定入力を返す。
 */
Dxf::IInputSource& StarterCopyInput();
/**
 * 実Starterの設定と初期Sceneを使い、失敗からの再試行とゲームの一周を検証する。
 * @param App コピー先のWindowsMainが構築したApplication。
 * @param Root 既存の.dxfpaths入口で決定したコピー先ルート。
 * @param Scene WindowsMainが選んだ最初のScene。
 */
Dxf::TResult<void> RunStarterCopyProbe(Dxf::FApplication& App, const Toolbox::FString& Root,
                                       Toolbox::TUniquePtr<Dxf::DScene> Scene);
#endif
