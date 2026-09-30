// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_JOINT_CONSUMER_H
#define DXF_PACKAGE_JOINT_CONSUMER_H
#include "FJointConsumerResult.h"
#include "Dxf/Scene.h"
#include "Dxf/RenderView3D.h"
#include "Toolbox/UniquePtr.h"
namespace Dxf
{
class FAssetService;
class FAudioPlayer;
} // namespace Dxf
/**
 * 公開Componentだけで吊り下げと接続先の再生成を通す2Dシーン。
 * @param Result Scene終了まで保持する結果。
 */
Toolbox::TUniquePtr<Dxf::DScene> MakeJointConsumer2D(FJointConsumerResult& Result);
/**
 * 同じ操作を3Dで通すシーン。
 * @param Result Scene終了まで保持する結果。
 */
Toolbox::TUniquePtr<Dxf::DScene> MakeJointConsumer3D(FJointConsumerResult& Result);
/**
 * Native描画と画素検査で共有するカメラの値。
 */
Dxf::FRenderView3D GetJointConsumerView();
/**
 * 実Sceneの固定更新と終了を両次元で確かめ、成功時0を返す。
 * @param Assets ファイルを読まない初期化窓口。
 * @param Audio Navigatorの音声窓口。
 */
Toolbox::int32 RunJointConsumer(Dxf::FAssetService& Assets, Dxf::FAudioPlayer& Audio);
#endif
