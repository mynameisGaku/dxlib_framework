// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_DEBUG_RECORDER_3D_H
#define DXF_PHYSICS_DEBUG_RECORDER_3D_H
#include "Dxf/DebugSnapshotHistory.h"
namespace Dxf
{
/**
 * 正常Step完了後の明示的な採取と観察履歴をまとめる。WorldやSceneを所有・参照しない。
 * 無効中はWorldへ触れず、全件採取や履歴への複製を行わない。変更は所有スレッドに限定する。
 */
class FPhysicsDebugRecorder3D
{
public:
	/**
	 * @param HistoryCapacity 履歴の保存上限、1〜240。
	 * @param HistoryInterval 通常時に履歴へ保存するStep間隔。1以上。
	 */
	explicit FPhysicsDebugRecorder3D(Toolbox::size_t HistoryCapacity = 120, Toolbox::uint64 HistoryInterval = 6);
	/**
	 * 観察を切り替える。無効化時は最新値を破棄し、保存済み履歴は保持する。
	 * @param bEnabled 採取を行うか。
	 */
	void SetEnabled(bool bEnabled) noexcept;
	/**
	 * 採取が有効か。
	 */
	FORCEINLINE bool IsEnabled() const noexcept
	{
		return m_bEnabled;
	}
	/**
	 * 正常Step完了後に呼ぶ。無効時はWorldへ触れずfalseを返す。
	 * 有効時は採取・変換して最新値を置き換え、条件を満たせば履歴へ保存してtrueを返す。
	 * 失敗時は最新値と履歴を変更しない。
	 * @param World 採取するWorld。変更しない。
	 * @param SimulationSeconds 呼出し側の時計で記録した経過秒数。
	 * @param bForceHistory 間隔に関係なく履歴へ保存する。同じStepは二重に保存しない。
	 */
	TResult<bool> Record(const FPhysicsWorld3D& World, Toolbox::f64 SimulationSeconds, bool bForceHistory);
	/**
	 * 有効化後に採取した最新値があるか。
	 */
	FORCEINLINE bool HasLive() const noexcept
	{
		return m_bHasLive;
	}
	/**
	 * 最新の採取値。HasLiveがfalseなら空のSnapshot。
	 */
	FORCEINLINE const FPhysicsDebugSnapshot3D& GetLive() const noexcept
	{
		return m_Live;
	}
	/**
	 * 観察履歴。値の取り出しは独立した複写になる。
	 */
	FORCEINLINE const FDebugSnapshotHistory& GetHistory() const noexcept
	{
		return m_History;
	}
	/**
	 * World作り直し時に最新値と履歴を失効させる。有効・無効の状態は維持する。
	 */
	void Clear() noexcept;

private:
	/**
	 * 保存済みの観察値。
	 */
	FDebugSnapshotHistory m_History;
	/**
	 * 最新の採取値。
	 */
	FPhysicsDebugSnapshot3D m_Live;
	/**
	 * 通常時に履歴へ保存するStep間隔。
	 */
	Toolbox::uint64 m_HistoryInterval;
	/**
	 * 最後に履歴へ保存したStep。
	 */
	Toolbox::uint64 m_HistoryStep = 0;
	/**
	 * 採取が有効か。
	 */
	bool m_bEnabled = true;
	/**
	 * 最新値があるか。
	 */
	bool m_bHasLive = false;
};
} // namespace Dxf
#endif
