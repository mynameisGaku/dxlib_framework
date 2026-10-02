// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONSUMER_CHECKPOINT_PROCESS_H
#define DXF_CONSUMER_CHECKPOINT_PROCESS_H
#include "Dxf/SceneNavigator.h"
#include "Toolbox/Platform.h"
#include "Toolbox/Function.h"
/**
 * 別プロセスで進行と設定を照合する、利用者側の小さいゲーム処理。
 * @param Assets 現在の所有側資源窓口。
 * @param Scenes 既存Scene切替窓口。
 * @param Step 通常の更新を一回進める利用者処理。
 * @param Observe 実描画を照合する任意の所有側処理。
 * @param Root 信頼する定義のProjectRoot。
 * @param SaveRoot 専用の絶対保存Root。
 * @param Mode writeまたはread。
 * @param Dimension 2または3。
 * @param NativeAssets 画像またはモデルを含む信頼する配布定義を選ぶか。
 */
void RunCheckpointProcess(Dxf::FAssetService& Assets, Dxf::FSceneNavigator& Scenes, const Toolbox::TFunction<void()>& Step, const Toolbox::TFunction<void(Dxf::DScene&, const char*)>& Observe, const Toolbox::FPath& Root, const Toolbox::FPath& SaveRoot, Toolbox::FStringView Mode, Toolbox::uint32 Dimension, bool NativeAssets = false);
#endif
