// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_CONTENT_CONSUMER_H
#define DXF_PACKAGE_CONTENT_CONSUMER_H
#include "Dxf/AssetService.h"
#include "Dxf/Application.h"
#include "Dxf/ContentScene2D.h"
#include "Dxf/ContentScene3D.h"
/**
 * 再配置先の定義だけから四種類のJointを生成し、実Worldと既存Collectionで終了する。
 * @param Assets 所有側の既存資源サービス。
 * @param Root 今回コピーした定義の絶対Root。
 */
void RunContentConsumer(Dxf::FAssetService& Assets, const Toolbox::FPath& Root);
/**
 * 再配置先の通常Applicationで同じ定義の両次元を描画し、一括読戻しする。
 * @param App 現在動作している所有側Application。
 * @param Root 定義と正規資源の再配置先。
 * @param Step 既存試験の固定時刻と入力を一回進める操作。
 */
void RunContentNativeConsumer(Dxf::FApplication& App, const Toolbox::FPath& Root, const Toolbox::TFunction<void()>& Step);
#endif
