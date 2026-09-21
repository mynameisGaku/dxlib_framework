// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_DEBUG_SNAPSHOT_HISTORY_H
#define DXF_DEBUG_SNAPSHOT_HISTORY_H
#include "Dxf/PhysicsDebugSnapshot3D.h"
namespace Dxf
{
/**
 * 観察値だけを最大240件保存する履歴。物理のロールバックや再実行ではない。
 * 同一Worldの単調増加する固定更新だけ受け付け、変更は所有スレッドへ限定する。
 */
class FDebugSnapshotHistory
{
public:
	/**
	 * @param Capacity 保存上限、1〜240。形状は各Snapshot最大256件。
	 */
	explicit FDebugSnapshotHistory(Toolbox::size_t Capacity = 120);
	/**
	 * 履歴の管理状態は複製せず、値SnapshotのReadAgeで取り出す。
	 */
	FDebugSnapshotHistory(const FDebugSnapshotHistory&) = delete;
	FDebugSnapshotHistory& operator=(const FDebugSnapshotHistory&) = delete;
	/**
	 * 完成した履歴の所有権を例外なしで移す。移動元は破棄・代入のみ可能。
	 */
	FDebugSnapshotHistory(FDebugSnapshotHistory&&) noexcept = default;
	FDebugSnapshotHistory& operator=(FDebugSnapshotHistory&&) noexcept = default;
	/**
	 * @param Snapshot 失敗時は履歴を変更しない。更新に成功してから呼ぶ。
	 */
	TResult<void> Push(FPhysicsDebugSnapshot3D Snapshot);
	/**
	 * @param Age 0が最新。独立した複写を返し、後の上書きやClearから保護する。
	 */
	TResult<FPhysicsDebugSnapshot3D> ReadAge(Toolbox::size_t Age) const;
	/**
	 * Scene切替やWorld再生成時に観察対象と履歴を失効させる。
	 */
	void Clear() noexcept;
	/**
	 * 現在の保存件数。
	 */
	FORCEINLINE Toolbox::size_t GetCount() const noexcept
	{
		return m_Count;
	}
private:
	/**
	 * 固定容量のリング領域。
	 */
	Toolbox::TVector<FPhysicsDebugSnapshot3D> m_Frames;
	/**
	 * 次に上書きする位置。
	 */
	Toolbox::size_t m_Next = 0;
	/**
	 * 使用中の件数。
	 */
	Toolbox::size_t m_Count = 0;
};
}
#endif
