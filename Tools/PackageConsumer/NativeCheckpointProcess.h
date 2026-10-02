// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONSUMER_NATIVE_CHECKPOINT_PROCESS_H
#define DXF_CONSUMER_NATIVE_CHECKPOINT_PROCESS_H
#include "Dxf/Application.h"
/**
 * 本体Applicationの更新と実DxLibの画素で、外部保存と再開を照合する。
 * @param App Native資源とSceneを所有するApplication。
 * @param Root 信頼する配布定義とAssetsのProjectRoot。
 * @param SaveRoot 新規の専用保存先。
 * @param Mode writeまたはread。
 * @param Dimension 2または3。
 * @param Step 一回だけ進める通常更新。
 */
void RunNativeCheckpointProcess(Dxf::FApplication& App, const Toolbox::FPath& Root, const Toolbox::FPath& SaveRoot, Toolbox::FStringView Mode, Toolbox::uint32 Dimension, const Toolbox::TFunction<void()>& Step);
#endif
