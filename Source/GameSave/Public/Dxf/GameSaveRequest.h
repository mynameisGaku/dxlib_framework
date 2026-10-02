// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMESAVEREQUEST_H
#define DXF_GAMESAVEREQUEST_H
#include "Dxf/SaveStore.h"
#include "Dxf/GameSaveTicket.h"
#include "Dxf/TaskDispatcher.h"
#include "Dxf/SavePayloadMigration.h"
namespace Dxf
{
/**
 * GameInstanceなどが明示所有する要求窓口。Workerは文書とI/Oだけ、公開は通常の所有側Poll。
 */
class FGameSaveRequest
{
public:
	/**
	 * @param Store 不変Rootと上限。
	 * @param Dispatcher 任意の借用先。
	 * @param Parent 自分だけを退役する親Scope。
	 */
	explicit FGameSaveRequest(FSaveStore Store, FTaskDispatcher* Dispatcher = nullptr, FTaskScope Parent = {});
	/**
	 * 対象Scopeだけを待ち、未公開要求を終端へ確定する。
	 */
	~FGameSaveRequest();
	/**
	 * 所有・要求番号を複製しない。
	 */
	FGameSaveRequest(const FGameSaveRequest&) = delete;
	/**
	 * 所有・要求番号を複製しない。
	 */
	FGameSaveRequest& operator=(const FGameSaveRequest&) = delete;
	/**
	 * @param Slot 保存系列。
	 * @param Captured 安全な採取境界で完成した不変値。未公開保存中の次要求はBusy例外。
	 */
	FGameSaveTicket RequestSave(Toolbox::FString Slot, FSaveDocument Captured);
	/**
	 * @param Slot 読込系列。
	 * @param Expected 信頼するカタログの識別条件。先行読込はSupersededへ確定する。
	 * @param Migration 任意の純粋なゲーム変換。元保存は変更せず、ゲームとContent識別を先に照合する。
	 * @param PrepareValues 検証済み文書からCPU値だけを作る任意処理。Dispatcher利用時はWorkerで呼ぶ。Scene・Native資源を捕捉しない。
	 */
	FGameSaveTicket RequestLoad(Toolbox::FString Slot, FSaveDocument Expected, FSavePayloadMigration Migration = {}, Toolbox::TFunction<void(const FSaveDocument&)> PrepareValues = {});
	/**
	 * 公開前はtrue、公開済み・公開結果不明ならfalse。Scene移行を巻き戻す操作ではない。
	 */
	bool Cancel();
	/**
	 * 公開結果が不明な保存だけを明示再試行する。通常PollはI/O失敗を繰り返さない。
	 */
	bool RetryPublication();
	/**
	 * Task Commit外で呼ぶ。保存公開・読込値採用だけを行い、SceneNavigatorを再入しない。
	 */
	void Poll();
	/**
	 * 最後に互換性検査を通過した読込値。新要求の失敗でも保持する。未成功なら例外。
	 */
	const FSaveLoadResult& GetLoaded() const;
	/**
	 * 最後の読込値を採用した要求番号。Preparedを再開成功とは扱わない。
	 */
	Toolbox::uint64 GetAcceptedSequence() const;
	/**
	 * @param Sequence 通常更新から実際にNavigatorへ要求した読込番号。
	 */
	void MarkTransitionRequested(Toolbox::uint64 Sequence);
	/**
	 * @param Sequence 新Sceneの成功PostPhysicsと必須Readyを確認した番号。検査そのものは再開adapterが担当する。
	 */
	void MarkResumeCompleted(Toolbox::uint64 Sequence);
	/**
	 * @param Sequence 現在の読込番号。
	 * @param Reason 準備・初期化・必須Ready前の失敗原因。
	 * 旧読込値と旧Sceneを破棄せず、再開要求だけを終端へ確定する。
	 */
	void MarkResumeFailed(Toolbox::uint64 Sequence, const char* Reason);
	/**
	 * 自分のScopeだけを退役する。安全な所有側境界以外ではfalse。
	 */
	bool Retire() noexcept;

private:
	/**
	 * 要求窓口だけが所有する採用先。
	 */
	struct FImpl;
	/**
	 * Workerへ持ち出さない所有側実装。
	 */
	Toolbox::TUniquePtr<FImpl> m_pImpl;
};
} // namespace Dxf
#endif
