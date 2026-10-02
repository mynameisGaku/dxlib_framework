// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_DOCUMENT_2D_H
#define DXF_CHECKPOINT_DOCUMENT_2D_H
#include "Dxf/SaveDocument.h"
#include "Dxf/CheckpointInstance2D.h"
#include "Dxf/CheckpointJointState.h"
namespace Dxf
{
/**
 * 2Dゲームの論理チェックポイント。任意Worldの中断snapshotではない。
 */
struct FCheckpointDocument2D
{
	/**
	 * 保存形式・ゲーム・Scene・checkpointの識別。Payloadは既定の空Objectに保つ。
	 */
	FSaveDocument Identity;
	/**
	 * Scene準備に使った全定義バイトの指紋。資源内容はbundle revisionと区別する。
	 */
	Toolbox::FString DefinitionFingerprint;
	/**
	 * 存在と除去、生成順を含む全論理個体。
	 */
	Toolbox::TVector<FCheckpointInstance2D> Instances;
	/**
	 * 種類付きの接続・操作要求。実行時Joint IDは含めない。
	 */
	Toolbox::TVector<FCheckpointJointState> Joints;
	/**
	 * ゲームcodecが型と範囲を検証する進行Object。保存コアに鍵や得点の規則を入れない。
	 */
	Toolbox::FString GamePayload = "{}";
};
} // namespace Dxf
#endif
