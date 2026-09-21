// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_EXAMPLE_TRANSPARENCY_DEMO_H
#define DXF_EXAMPLE_TRANSPARENCY_DEMO_H
#include "Dxf/Render3DContext.h"
namespace Dxf::RenderDebug
{
/**
 * 手前を先に要求する三枚の半透明パネルを、同じビュー区間へ追加する。
 * @param Render このフレームの3D描画窓口。ビューやWorldを変更しない。
 */
TResult<void> SubmitTransparencyDemo(FRender3DContext& Render);
}
#endif
