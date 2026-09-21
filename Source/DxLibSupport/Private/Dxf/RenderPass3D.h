// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_RENDER_PASS_3D_H
#define DXF_PRIVATE_RENDER_PASS_3D_H
#include "Dxf/RenderGeometry3D.h"
namespace Dxf::Detail
{
/**
 * 同じビュー設定・Flush区間の命令を不透明・透明・重ね表示へ計画する。
 * 透明面は三頂点の中心、透明線は中点をカメラ前方へ射影して降順に並べる。
 * 同値キーは入力順。交差する面の画素単位順序やOITは保証しない。
 * 入力を変更せず、計画・確保をすべてNative実行より先に完了させる。
 * @param Prepared 変換済みの全命令。
 * @param Begin 対象区間の先頭。
 * @param End 対象区間の末尾の次。
 * @param View この区間のカメラ。
 */
TResult<Toolbox::TVector<FPreparedGeometry3D>> BuildRenderPasses3D_Internal(
	const Toolbox::TVector<FPreparedGeometry3D>& Prepared,
	Toolbox::size_t Begin, Toolbox::size_t End, const FRenderView3D& View);
}
#endif
