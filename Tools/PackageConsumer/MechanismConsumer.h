// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_MECHANISM_CONSUMER_H
#define DXF_PACKAGE_MECHANISM_CONSUMER_H
#include "FMechanismConsumerResult.h"
#include "Dxf/Scene.h"
#include "Dxf/RenderView3D.h"
namespace Dxf
{
class FAssetService;
class FAudioPlayer;
} // namespace Dxf
/**
 * 再配置した公開Componentだけで仕掛けを構成する。
 * @param Result Scene終了後も保持する結果。
 */
Toolbox::TUniquePtr<Dxf::DScene> MakeMechanismConsumer2D(FMechanismConsumerResult& Result);
/**
 * 非Identity姿勢の3D装置を構成する。
 * @param Result Scene終了後も保持する結果。
 */
Toolbox::TUniquePtr<Dxf::DScene> MakeMechanismConsumer3D(FMechanismConsumerResult& Result);
/**
 * 描画と画素検査が共有する値のカメラ。
 */
Dxf::FRenderView3D GetMechanismConsumerView();
/**
 * 両次元の実Scene更新・接続・寿命を確認する。成功は0。
 * @param Assets 外部利用者の資源窓口。
 * @param Audio 外部利用者の音声窓口。
 */
Toolbox::int32 RunMechanismConsumer(Dxf::FAssetService& Assets, Dxf::FAudioPlayer& Audio);
#endif
