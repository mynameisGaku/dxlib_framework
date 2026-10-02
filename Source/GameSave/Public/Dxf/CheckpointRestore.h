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
	 * 信頼する初期構成からNew Game用の論理文書を作る。Worldや資源を登録しない。
	 * @param Catalog 初期個体と全公開Jointを解決する2D許可カタログ。
	 * @param GamePayload ゲームcodecで検証した進行Object。構造不正・容量超過は例外。
	 * @param Limits 文書と個体・Jointの確保前上限。
	 * @param ContentLimits 展開後の初期構成へ適用する上限。
	 */
	static FCheckpointDocument2D MakeInitial(const FCheckpointCatalog2D& Catalog, Toolbox::FStringView GamePayload = "{}", FCheckpointLimits Limits = {}, FSceneContentLimits ContentLimits = {});
	/**
	 * 3Dも同じ論理規則で初期文書を作る。配置やLocal Frameを再変換しない。
	 * @param Catalog 信頼する3D初期構成と許可Prefab。
	 * @param GamePayload ゲームの初期進行Object。ゲーム固有の意味は呼出し側で検証する。
	 * @param Limits 保存文書の有限上限。
	 * @param ContentLimits 3D初期構成の展開上限。
	 */
	static FCheckpointDocument3D MakeInitial(const FCheckpointCatalog3D& Catalog, Toolbox::FStringView GamePayload = "{}", FCheckpointLimits Limits = {}, FSceneContentLimits ContentLimits = {});
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
