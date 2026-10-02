// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_SAVE_COURSE_GAME_INSTANCE_H
#define DXF_SAMPLE_SAVE_COURSE_GAME_INSTANCE_H
#include "SaveCourseInput.h"
#include "SaveCourseSlot.h"
#include "Toolbox/Array.h"
#include "Dxf/GameInstance.h"
#include "Dxf/GameSaveTicket.h"
#include "Dxf/SceneContentSource.h"
namespace Dxf::GameplaySample
{
/**
 * Scene切替を越えて保存要求と独立設定を所有するSampleの長期所有者。
 * WorldやSceneの所有は既存Applicationへ任せる。
 */
class ASaveCourseGameInstance final : public DGameInstance
{
public:
	/**
	 * @param SaveRoot 起動時に確定する完全修飾の専用保存領域。
	 * @param Content 信頼するProjectRootの読込窓口。許可値を一度だけ読み込む。
	 * @param bAsync 既存Dispatcherを借りるか。同期経路も同じ公開前照合を使う。
	 */
	ASaveCourseGameInstance(Toolbox::FPath SaveRoot, FSceneContentSource Content, bool bAsync = true);
	/**
	 * 自分の要求を退役し、Sceneや兄弟Scopeの終了を再入しない。
	 */
	~ASaveCourseGameInstance() override;
	/**
	 * @param Dimension 2または3。次の通常更新でNew Gameを準備する。
	 */
	void RequestNewGame(Toolbox::uint32 Dimension);
	/**
	 * @param Dimension 従来のC++コースへ通常Scene切替を要求する。
	 */
	void RequestLegacyCourse(Toolbox::uint32 Dimension);
	/**
	 * @param Slot 短い識別子。通常は次の成功境界で採取し、Pause中は最後の正常値を使う。
	 */
	void RequestSave(Toolbox::FString Slot);
	/**
	 * @param Dimension 信頼するScene次元。保存値と違う場合は旧Sceneを保持する。
	 * @param Slot 読み込む保存系列。
	 */
	void RequestLoad(Toolbox::uint32 Dimension, Toolbox::FString Slot);
	/**
	 * autosaveからの再開を要求する。保存完了と再開完了を区別する。
	 * @param Dimension 再開する2Dまたは3D。
	 */
	void RequestRetry(Toolbox::uint32 Dimension);
	/**
	 * 次の所有側更新で公開前取消しを要求する。公開後を取消し成功と表示しない。
	 */
	void RequestCancel() noexcept;
	/**
	 * @param Settings 検証済み候補。次の所有側更新で元入力と共に全体適用する。
	 */
	void RequestSettings(FUserSettings Settings);
	/**
	 * 現在の設定だけを別スロットへ保存する。進行ロードで設定を巻き戻さない。
	 */
	void RequestSettingsSave() noexcept;
	/**
	 * 公開結果が未確定の同じ保存だけを、次の通常更新で再確認する。
	 * @param bSettings trueは独立設定、falseはゲーム進行の保存。
	 */
	void RequestPublicationRetry(bool bSettings) noexcept;
	/**
	 * 現在適用した独立設定。
	 */
	const FUserSettings& GetSettings() const noexcept;
	/**
	 * 保存面の確定を確認していない設定変更が残るか。
	 */
	bool IsSettingsDirty() const noexcept;
	/**
	 * 成功固定更新の採取を待っているか。Preparedや保存成功とは別。
	 */
	bool IsCapturing() const noexcept;
	/**
	 * 最新の保存・ロード要求の状態。新Sceneの必須ReadyまでCompletedにしない。
	 */
	FGameSaveTicket GetSaveTicket() const;
	/**
	 * 最新の独立設定保存の状態。
	 */
	FGameSaveTicket GetSettingsTicket() const;
	/**
	 * 採取待ちや要求前の失敗も含む診断。通常Tickで毎回確保しない。
	 */
	const Toolbox::FString& GetDiagnostic() const noexcept;
	/**
	 * 初期化と要求の終端で更新した4系列。通常UI表示ではI/Oしない。
	 */
	const Toolbox::TArray<FSaveCourseSlot, 4>& GetSlots() const noexcept;
	/**
	 * 次の所有側更新でスロット一覧を読み直す。毎Frameは要求しない。
	 */
	void RequestSlotRefresh() noexcept;
	/**
	 * スロット一覧を実際に更新した番号。定常表示ではI/Oしない。
	 */
	Toolbox::uint64 GetSlotRevision() const noexcept;
	/**
	 * 現在適用した設定の番号。UIは変更時だけ表示値を作り直す。
	 */
	Toolbox::uint64 GetSettingsRevision() const noexcept;

protected:
	/**
	 * @param Context 設定読込と資源準備に借りる所有側窓口。初期化中は音を鳴らさない。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * @param Context Task Commit外の通常更新。Sceneと資源をWorkerへ捕捉しない。
	 */
	void OnTick(const FTickContext& Context) override;
	/**
	 * 自分の未完了要求だけを終了し、Native資源は所有側で解放する。
	 */
	void OnDeinitialize() noexcept override;

private:
	/**
	 * 信頼する値・Scope付き要求・独立設定の所有側実装。
	 */
	struct FImpl;
	/**
	 * GameInstanceだけが所有する状態。別の常設Managerを作らない。
	 */
	Toolbox::TUniquePtr<FImpl> m_pImpl;
};
} // namespace Dxf::GameplaySample
#endif
