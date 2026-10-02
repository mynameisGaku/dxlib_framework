// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_CHECKPOINT_CONSUMER_H
#define DXF_PACKAGE_CHECKPOINT_CONSUMER_H
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Toolbox/Platform.h"
/**
 * 再配置した公開targetだけで両次元の論理チェックポイントを新Sceneへ変換する。
 * @param Assets 既存の所有側資源サービス。
 * @param Root 配布した定義の完全修飾Root。保存から任意パスを取得しない。
 * @param SaveRoot 今回用の独立した完全修飾保存領域。@param Audio 既存の所有側音声窓口。
 */
void RunCheckpointConsumer(Dxf::FAssetService& Assets, const Toolbox::FPath& Root, const Toolbox::FPath& SaveRoot, Dxf::FAudioPlayer& Audio);
#endif
