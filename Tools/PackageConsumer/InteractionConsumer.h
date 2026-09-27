// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_INTERACTION_CONSUMER_H
#define DXF_PACKAGE_INTERACTION_CONSUMER_H
#include "FInteractionConsumerResult.h"
#include "Dxf/Scene.h"
#include "Toolbox/UniquePtr.h"

namespace Dxf
{
class FAssetService;
class FAudioPlayer;
} // namespace Dxf

/**
 * 移動床と通知中の破棄を検証する2Dシーンを、配布された公開APIだけで生成する。
 * @param Result シーン破棄まで呼び出し側が保持する結果の保存先。
 */
Toolbox::TUniquePtr<Dxf::DScene> MakeInteractionConsumer2D(FInteractionConsumerResult& Result);
/**
 * 2Dと同じ手順を実行する3Dシーンを生成する。
 * @param Result シーン破棄まで呼び出し側が保持する結果の保存先。
 */
Toolbox::TUniquePtr<Dxf::DScene> MakeInteractionConsumer3D(FInteractionConsumerResult& Result);
/**
 * 実デバイスを使わず2D／3Dのシーンを固定更新し、成功なら0を返す。
 * @param Assets Scene初期化の資源窓口。画像・音声ファイルは読み込まない。
 * @param Audio Sceneの寿命を管理するNavigatorが使う音声窓口。
 */
Toolbox::int32 RunInteractionConsumer(Dxf::FAssetService& Assets, Dxf::FAudioPlayer& Audio);
#endif
