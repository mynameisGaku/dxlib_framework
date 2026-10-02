// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_CHECKPOINT_CONSUMER_H
#define DXF_PACKAGE_CHECKPOINT_CONSUMER_H
#include "Dxf/AssetService.h"
#include "Toolbox/Platform.h"
/**
 * 再配置した公開targetだけで両次元の論理チェックポイントを新Sceneへ変換する。
 * @param Assets 既存の所有側資源サービス。
 * @param Root 配布した定義の完全修飾Root。保存から任意パスを取得しない。
 */
void RunCheckpointConsumer(Dxf::FAssetService& Assets, const Toolbox::FPath& Root);
#endif
