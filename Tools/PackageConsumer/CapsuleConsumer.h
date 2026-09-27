// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_CAPSULE_CONSUMER_H
#define DXF_PACKAGE_CAPSULE_CONSUMER_H
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"

/**
 * dxf::framework（Gameplay）の公開Componentで、2D／3Dのカプセルのキャラクター・高さの変更・Dynamicの箱の押し合いを
 * 実SceneNavigatorの固定更新で検証する。成功は0、失敗は検査ごとの番号（501〜）。
 * @param Assets 資源管理。
 * @param Audio 音声。
 */
Toolbox::int32 RunCapsuleConsumer(Dxf::FAssetService& Assets, Dxf::FAudioPlayer& Audio);
#endif
