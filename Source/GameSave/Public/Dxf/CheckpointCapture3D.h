// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_CAPTURE_3D_H
#define DXF_CHECKPOINT_CAPTURE_3D_H
#include "Dxf/CheckpointCatalog3D.h"
#include "Dxf/CheckpointDocument3D.h"
#include "Dxf/CheckpointLimits.h"
#include "Dxf/ContentScene3D.h"
namespace Dxf
{
namespace SavePrivate
{
class FCheckpointCaptureRuntime3D;
}
/**
 * Sceneの論理個体履歴と世代付き非所有参照。WorldやObjectを所有せず、所有側だけで使う。
 */
class FCheckpointCapture3D
{
public:
	/**
	 * @param Catalog 信頼する初期Sceneと許可Prefab。初期個体履歴を一度だけ作る。
	 * @param Limits 保存の有限上限。
	 */
	explicit FCheckpointCapture3D(const FCheckpointCatalog3D& Catalog, FCheckpointLimits Limits = {});
	/**
	 * @param Checkpoint 検証済み再開の論理履歴。新Worldの実行時参照は含めない。
	 * @param Limits 保存の有限上限。
	 */
	explicit FCheckpointCapture3D(FCheckpointDocument3D Checkpoint, FCheckpointLimits Limits = {});
	/**
	 * 非所有参照とCPU値だけを解放する。Sceneの終了処理を呼ばない。
	 */
	~FCheckpointCapture3D();
	/**
	 * 複製・移動による別Sceneへの暗黙追従を禁止する。
	 */
	FCheckpointCapture3D(const FCheckpointCapture3D&) = delete;
	/**
	 * 複製・移動による別Sceneへの暗黙追従を禁止する。
	 */
	FCheckpointCapture3D& operator=(const FCheckpointCapture3D&) = delete;
	/**
	 * @param Scene 初期化済みの対応Scene。一度だけ個体を解決し、以後同名の新世代へ追従しない。
	 */
	void Bind(DContentScene3D& Scene);
	/**
	 * @param Scene Bindした同一Scene。
	 * @param Instance 許可カタログで検証した動的個体。初期配置と公開値を記録する。
	 * @param Prepared 既存AssetServiceで準備済みの対応Prefab。
	 * 受付前に履歴の必要容量を確保する。失敗時は履歴を変更しない。
	 */
	TResult<TObjectHandle<DPrefabInstance3D>> Spawn(DContentScene3D& Scene, FCheckpointInstance3D Instance, FPreparedPrefab3D Prepared);
	/**
	 * @param Scene Bindした同一Scene。
	 * @param Key 明示破棄する個体の論理キー。既存Collectionの境界解放を使う。
	 * 失効した古いhandleから同名の新個体を破棄しない。追加確保しない。
	 */
	bool Destroy(DContentScene3D& Scene, Toolbox::FStringView Key) noexcept;
	/**
	 * 次の成功固定更新境界で一度だけ採取する。未Ready・保留設定は次の境界を待つ。
	 * 退役後はfalse。既存文書を毎Frameencodeしない。
	 */
	bool RequestCapture() noexcept;
	/**
	 * @param CheckpointKey 同じゲーム・Sceneで明示した安全な再開地点。不正なら旧要求を保持する。
	 * 最新の要求地点を次の成功境界へまとめる。Pause中の旧採取値は変更しない。
	 */
	bool RequestCapture(Toolbox::FString CheckpointKey);
	/**
	 * 未採取要求が残るか。最後の正常文書があっても保留要求とは区別する。
	 */
	bool HasCaptureRequest() const noexcept;
	/**
	 * @param Scene BindしたScene。成功World.Stepと全PostPhysics後のフック内だけ採取可能。
	 * @param GamePayload 同じ境界のゲームcodecが検証した進行Object。
	 * 要求なし・未Ready・保留変更ではfalse。採取失敗は旧正常値を保持して例外を伝える。
	 */
	bool CaptureIfRequested(DContentScene3D& Scene, Toolbox::FStringView GamePayload);
	/**
	 * 最後に採取できた不変CPU値。Pause中の保存はこの値を使い、未処理変更を混ぜない。
	 */
	const Toolbox::TOptional<FCheckpointDocument3D>& GetLastCapture() const noexcept;
	/**
	 * 最後の採取が属する成功固定更新の通し番号。未採取は0。
	 */
	Toolbox::uint64 GetCapturedStep() const noexcept;
	/**
	 * Scene終了前に非所有参照と未採取要求を終了する。以後再Bindしない。
	 */
	void Retire() noexcept;

private:
	/**
	 * 個体履歴、世代付きhandle、最後の正常採取値。
	 */
	Toolbox::TUniquePtr<SavePrivate::FCheckpointCaptureRuntime3D> m_pRuntime;
};
} // namespace Dxf
#endif
