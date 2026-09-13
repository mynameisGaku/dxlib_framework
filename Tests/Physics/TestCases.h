// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_TEST_CASES_H
#define DXF_PHYSICS_TEST_CASES_H
#include "Toolbox/Utility.h"
namespace PhysicsTest
{
/**
 * 独立した回帰ケースの名前と実行入口。
 */
struct FCase
{
	/**
	 * 失敗時にも表示するケース名。
	 */
	const char* Name;
	/**
	 * 期待値に反すると例外を返す処理。
	 */
	void (*Run)();
};
/**
 * 検査した条件が偽の場合、式を例外にしてランナーへ返す。
 * @param bCondition 確認する条件。
 * @param Expression 診断用の式の表記。
 */
FORCEINLINE void Require_Internal(bool bCondition, const char* Expression)
{
	if (!bCondition)
	{
		throw Toolbox::FException(Expression);
	}
}
/**
 * 接触判定の回帰ケースを返す。
 * @param Count 返す配列の要素数。
 */
const FCase* GetCollisionCases(Toolbox::size_t& Count) noexcept;
/**
 * 連続衝突判定の回帰ケースを返す。
 * @param Count 返す配列の要素数。
 */
const FCase* GetSweepCases(Toolbox::size_t& Count) noexcept;
/**
 * 固定ステップ管理の回帰ケースを返す。
 * @param Count 返す配列の要素数。
 */
const FCase* GetFixedStepCases(Toolbox::size_t& Count) noexcept;
/**
 * 剛体の生成と自由運動の回帰ケースを返す。
 * @param Count 返す配列の要素数。
 */
const FCase* GetRigidBodyCases(Toolbox::size_t& Count) noexcept;
/**
 * 接触点・法線・分離距離の回帰ケースを返す。
 * @param Count 返す配列の要素数。
 */
const FCase* GetContactCases(Toolbox::size_t& Count) noexcept;
/**
 * 衝突応答ソルバーの回帰ケースを返す。
 * @param Count 返す配列の要素数。
 */
const FCase* GetSolverCases(Toolbox::size_t& Count) noexcept;
/**
 * 連続衝突を物理更新へ接続する回帰ケースを返す。
 * @param Count 返す配列の要素数。
 */
const FCase* GetContinuousCases(Toolbox::size_t& Count) noexcept;
/**
 * 安定化と休止の回帰ケースを返す。
 * @param Count 返す配列の要素数。
 */
const FCase* GetStabilityCases(Toolbox::size_t& Count) noexcept;
} // namespace PhysicsTest
#define PHYSICS_REQUIRE(...) PhysicsTest::Require_Internal(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__)
#endif
