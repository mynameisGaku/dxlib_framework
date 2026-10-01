// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SCENE_CONTENT_REQUEST_H
#define DXF_SCENE_CONTENT_REQUEST_H
#include "Dxf/SceneContentSource.h"
#include "Dxf/SceneContentTicket.h"
#include "Dxf/PreparedScene2D.h"
#include "Dxf/PreparedScene3D.h"
#include "Dxf/TaskDispatcher.h"
#include "Toolbox/UniquePtr.h"
namespace Dxf
{
/**
 * 呼出し側が所有するScene準備要求。WorkerはCPU定義だけを作り、Pollが資源を取り込む。
 * 全操作と破棄は構築した所有スレッドのTask/Commit外で行う。
 * Dispatcherを使う場合はこの要求より長く生存させる。
 */
class FSceneContentRequest
{
public:
	/**
	 * @param Source 固定Rootと有限上限を持つCPU読込入口。
	 * @param Dispatcher 借用する既存Dispatcher。nullなら同期準備。
	 * @param Parent 要求Scopeの親。Scene遷移を所有する長期所有者から借用する。
	 */
	explicit FSceneContentRequest(FSceneContentSource Source, FTaskDispatcher* Dispatcher = nullptr, FTaskScope Parent = {});
	/**
	 * 自分のScopeだけを退役し、所有側で準備資源を解放する。
	 */
	~FSceneContentRequest();
	/**
	 * 所有側の採用先を複製しない。
	 */
	FSceneContentRequest(const FSceneContentRequest&) = delete;
	/**
	 * 所有側の採用先を複製しない。
	 */
	FSceneContentRequest& operator=(const FSceneContentRequest&) = delete;
	/**
	 * @param Path Root相対の2D Scene。前の未完了要求を置換する。
	 */
	FSceneContentTicket LoadScene2D(Toolbox::FString Path);
	/**
	 * @param Path Root相対の3D Scene。前の未完了要求を置換する。
	 */
	FSceneContentTicket LoadScene3D(Toolbox::FString Path);
	/**
	 * 未完了要求を終端へ確定する。実行中CPU処理を強制終了しない。
	 */
	void Cancel();
	/**
	 * 通常の所有側更新から呼ぶ。DispatcherのCommitはCPU結果の公開だけを行う。
	 * @param Assets 既存資源所有経路。Modelなどの同期Loaderはここでブロックし得る。
	 */
	void Poll(FAssetService& Assets);
	/**
	 * 最後に成功した2D準備値。新しい失敗・取消しでも維持し、未成功なら例外。
	 */
	const FPreparedScene2D& GetPrepared2D() const;
	/**
	 * 最後に成功した3D準備値。新しい失敗・取消しでも維持し、未成功なら例外。
	 */
	const FPreparedScene3D& GetPrepared3D() const;
	/**
	 * 最後に採用した要求番号。TicketのReadyを確認してからScene変更を要求する。
	 */
	Toolbox::uint64 GetAcceptedSequence() const;
	/**
	 * 所有側で自分のCPU準備を待つ。兄弟Scopeは待たず、再入中ならfalse。
	 */
	bool Retire() noexcept;

private:
	/**
	 * CPU共有状態とNative所有値を別々に持つ内部実装。
	 */
	struct FImpl;
	/**
	 * Requestが唯一所有する採用先。
	 */
	Toolbox::TUniquePtr<FImpl> m_pImpl;
};
} // namespace Dxf
#endif
