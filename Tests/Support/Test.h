#pragma once
#include "Toolbox/Function.h"
#include "Toolbox/Utility.h"
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Test
{
/**
 * 登録済みテストの表示名と実行入口。
 */
struct FCase
{
	/**
	 * 失敗や成功の表示に使うテスト名。
	 */
	const char* Name;
	/**
	 * 引数を取らないテスト本体。
	 */
	void (*Run)();
};
/**
 * 翻訳単位をまたいで登録先を共有する。
 */
FORCEINLINE Toolbox::TVector<FCase>& Cases_Internal()
{
	// 初回参照時に作られる全テストの登録先。
	static Toolbox::TVector<FCase> Cases;
	return Cases;
}
/**
 * 静的初期化時にテスト本体を登録する補助型。
 */
class FRegistration
{
public:
	/**
	 * 名前と実行入口をテスト一覧へ追加する。
	 * @param Name 結果に表示する名前。
	 * @param Run 実行するテスト本体。
	 */
	FRegistration(const char* Name, void (*Run)())
	{
		Cases_Internal().PushBack({Name, Run});
	}
};
/**
 * 条件が偽なら式と行番号を付けてテストを失敗させる。
 * @param bValue 検証する条件の評価結果。
 * @param Text 条件式のソース表記。
 * @param Line 条件式がある行番号。
 */
FORCEINLINE void Require_Internal(bool bValue, const char* Text, Toolbox::int32 Line)
{
	if (!bValue)
	{
		throw Toolbox::FException(Toolbox::FString(Text) + " at line " + Toolbox::ToString(Line));
	}
}
} // namespace Test
#define DXF_JOIN_IMPL(A, B) A##B
#define DXF_JOIN(A, B) DXF_JOIN_IMPL(A, B)
#define TEST(Name)                                                                                                     \
	static void DXF_JOIN(Test_Internal_, __LINE__)();                                                                  \
	static Test::FRegistration DXF_JOIN(Registration_, __LINE__)(Name, &DXF_JOIN(Test_Internal_, __LINE__));           \
	static void DXF_JOIN(Test_Internal_, __LINE__)()
#define REQUIRE(...) Test::Require_Internal(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __LINE__)
