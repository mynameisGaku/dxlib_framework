// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_RESTORE_H
#define DXF_CHECKPOINT_RESTORE_H
#include "Dxf/CheckpointCodec.h"
#include "Dxf/CheckpointCatalog2D.h"
#include "Dxf/CheckpointCatalog3D.h"
#include "Dxf/CheckpointResumePlan2D.h"
#include "Dxf/CheckpointResumePlan3D.h"
#include "Dxf/SceneContentLimits.h"
namespace Dxf
{
/**
 * 論理キーと許可カタログから新SceneのCPU計画を作る。旧WorldやNavigatorを操作しない。
 */
class FCheckpointRestore
{
public:
	/**
	 * @param Document 構造検証に加え、ゲーム側で進行を検証した2D値。
	 * @param Catalog 信頼する定義と明示したCPUレシピ。不一致・不在・種類違いは例外。
	 * @param Limits 保存文書の確保前上限。
	 * @param ContentLimits 展開後の全部品・Joint・資源へ適用する上限。
	 */
	static FCheckpointResumePlan2D Build(const FCheckpointDocument2D& Document, const FCheckpointCatalog2D& Catalog, FCheckpointLimits Limits = {}, FSceneContentLimits ContentLimits = {});
	/**
	 * @param Document 論理進行と剛体配置。実体ID・Solver状態を含まない。
	 * @param Catalog 信頼する3D定義。保存の配列位置ではなく個体・exportキーを解決する。
	 * @param Limits 保存文書の上限。
	 * @param ContentLimits 3D初期構成の既存検証上限。
	 */
	static FCheckpointResumePlan3D Build(const FCheckpointDocument3D& Document, const FCheckpointCatalog3D& Catalog, FCheckpointLimits Limits = {}, FSceneContentLimits ContentLimits = {});
};
} // namespace Dxf
#endif
