// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_RULES_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_RULES_H
#include "Toolbox/Utility.h"
namespace Dxf::GameplaySample
{
/**
 * 接触・Triggerのサンプルのゲームの規則と状態（2D／3D共通）。何を取得し、いつ扉を開けるかはゲーム側が決める（Physicsは知らない）。
 * 通知（物理Stepの後）と固定更新から呼ばれ、描画は読むだけにする。
 */
class FInteractionRules
{
public:
	/**
	 * 取得物を一つ取得した。
	 */
	void Collect() noexcept;
	/**
	 * 取得した数。
	 */
	FORCEINLINE Toolbox::int32 GetCollected() const noexcept
	{
		return m_Collected;
	}
	/**
	 * 圧力板に入っている当事者（Body）の数を更新する。1以上の間は扉を開け、0になってから保持秒数の後に閉じる。
	 * @param Occupants 入っているBodyの数。
	 */
	void SetPlateOccupants(Toolbox::int32 Occupants) noexcept;
	/**
	 * 圧力板に入っている当事者の数。
	 */
	FORCEINLINE Toolbox::int32 GetPlateOccupants() const noexcept
	{
		return m_PlateOccupants;
	}
	/**
	 * 固定更新の時間を進める（扉の保持時間）。
	 * @param Seconds 固定更新の秒数。
	 */
	void Advance(Toolbox::f64 Seconds) noexcept;
	/**
	 * 扉を開けておくか。
	 */
	FORCEINLINE bool IsDoorOpen() const noexcept
	{
		return m_PlateOccupants > 0 || m_DoorHold > 0;
	}
	/**
	 * プレイヤーと箱の接触の遷移を記録する（Begin／End）。
	 * @param bBegin Beginならtrue、Endならfalse。
	 */
	void CrateContact(bool bBegin) noexcept;
	/**
	 * 同じ箱との接触が続いた固定更新を数える。
	 */
	void CrateStay() noexcept;
	/**
	 * プレイヤーと箱のStayの数。
	 */
	FORCEINLINE Toolbox::int32 GetCrateStays() const noexcept
	{
		return m_CrateStays;
	}
	/**
	 * プレイヤーが箱に触れているか。
	 */
	FORCEINLINE bool IsTouchingCrate() const noexcept
	{
		return m_bTouchingCrate;
	}
	/**
	 * プレイヤーと箱のBegin・Endの数。
	 */
	FORCEINLINE Toolbox::int32 GetCrateBegins() const noexcept
	{
		return m_CrateBegins;
	}
	FORCEINLINE Toolbox::int32 GetCrateEnds() const noexcept
	{
		return m_CrateEnds;
	}
	/**
	 * チェックポイントを通った（番号が大きい方だけ記録する）。
	 * @param Index チェックポイントの番号。
	 */
	void ReachCheckpoint(Toolbox::int32 Index) noexcept;
	/**
	 * 最後に記録したチェックポイントの番号。
	 */
	FORCEINLINE Toolbox::int32 GetCheckpoint() const noexcept
	{
		return m_Checkpoint;
	}
	/**
	 * 危険領域に入り、チェックポイントへ戻った回数を数える。
	 */
	void Respawned() noexcept;
	/**
	 * チェックポイントへ戻った回数。
	 */
	FORCEINLINE Toolbox::int32 GetRespawns() const noexcept
	{
		return m_Respawns;
	}

private:
	/**
	 * 取得した数。
	 */
	Toolbox::int32 m_Collected = 0;
	/**
	 * 圧力板に入っている当事者の数。
	 */
	Toolbox::int32 m_PlateOccupants = 0;
	/**
	 * 圧力板が空になった後、扉を開けておく残りの秒数。
	 */
	Toolbox::f64 m_DoorHold = 0;
	/**
	 * プレイヤーが箱に触れているか。
	 */
	bool m_bTouchingCrate = false;
	/**
	 * プレイヤーと箱のBegin・Endの数。
	 */
	Toolbox::int32 m_CrateBegins = 0;
	/**
	 * 継続中の接触を確認した固定更新の数。
	 */
	Toolbox::int32 m_CrateStays = 0;
	Toolbox::int32 m_CrateEnds = 0;
	/**
	 * 最後に記録したチェックポイントの番号。
	 */
	Toolbox::int32 m_Checkpoint = 0;
	/**
	 * チェックポイントへ戻った回数。
	 */
	Toolbox::int32 m_Respawns = 0;
};
} // namespace Dxf::GameplaySample
#endif
