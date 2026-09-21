// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_PROJECT_PATHS_H
#define TOOLBOX_PROJECT_PATHS_H
#include "Toolbox/Platform.h"
namespace Toolbox
{
/**
 * 開発パス設定ファイルの最大バイト数。これを超える内容は拒否する。
 */
static constexpr size_t MaxSettingsFileBytes = 4096;
/**
 * 開発パス設定ファイルを読み取る。存在しなければfalseを返す。
 * 共有違反・権限・種別・上限超過・読込失敗は例外で通知し、黙って空にしない。
 * @param Path 読み取る設定ファイルのパス。
 * @param Text 読み取ったUTF-8本文の格納先。失敗時は変更しない。
 * @return 読み取れたらtrue。存在しない場合だけfalse。
 */
bool TryReadSettingsFile(const FPath& Path, FString& Text);
/**
 * exe横の開発パス設定が保持する値。設定・結果の保持専用。
 */
struct FProjectPathSettings
{
	/**
	 * 設定形式の版。現在は1のみ有効。
	 */
	uint32 Version = 0;
	/**
	 * DevelopmentまたはPackagedの用途。
	 */
	FString Mode;
	/**
	 * exe配置先を基準にしたProjectRootへの相対パス。
	 */
	FString ProjectRootRelative;
};
/**
 * 小さなバージョン付き開発パス設定を読み取る。
 * @param Text 設定ファイルのUTF-8本文。
 * @param Destination 読み取った設定の格納先。
 * @return 形式・版・必須項目が正しければtrue。
 */
bool ParseProjectPathSettings(const FString& Text, FProjectPathSettings& Destination);
/**
 * exe配置先と開発パス設定からProjectRootを解決する。字句的で存在確認はしない。
 * 相対値はexe配置先を基準に結合し、完全修飾の絶対値はそのまま使う。
 * @param ExeDirectory 実行ファイルが配置されている完全修飾ディレクトリ。
 * @param SettingsText 開発パス設定ファイルのUTF-8本文。
 * @param Root 解決したProjectRootの格納先。
 * @return 設定が正しく絶対パスへ解決できればtrue。
 */
bool ResolveDevelopmentRoot(const FPath& ExeDirectory, const FString& SettingsText, FPath& Root);
/**
 * 論理パスをProjectRoot基準の絶対パスへ解決する。Root決定後は不変。
 * 解決は字句的でファイルへ触れず、Workerから参照できる。
 */
class FAssetPathResolver
{
public:
	/**
	 * 未設定の解決器を作る。
	 */
	FAssetPathResolver() = default;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FAssetPathResolver(const FAssetPathResolver&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	FAssetPathResolver& operator=(const FAssetPathResolver&) = delete;
	/**
	 * ProjectRootを一度だけ設定する。絶対パスを要求する。
	 * @param Root sln配置先の完全修飾ディレクトリ。
	 * @return 初回かつ有効なRootならtrue。
	 */
	bool SetRoot(const FPath& Root);
	/**
	 * Rootが設定済みかを返す。
	 */
	FORCEINLINE bool IsSet() const noexcept
	{
		return m_bSet;
	}
	/**
	 * 設定済みのProjectRootを返す。未設定なら空。
	 */
	FORCEINLINE const FPath& GetRoot() const noexcept
	{
		return m_Root;
	}
	/**
	 * 論理パスを解決する。相対パスはRootと結合し正規化する。
	 * 完全修飾の絶対パスはRootを前置せず正規化だけ行う。
	 * Rootは隔離境界ではなく解決基準であり、symlink先の検証はしない。
	 * @param LogicalPath 呼び出し側が指定したパス。
	 * @param Resolved 解決した絶対パスの格納先。
	 * @return 解決できればtrue。Root外・曖昧・不正入力はfalse。
	 */
	bool Resolve(const FString& LogicalPath, FPath& Resolved) const;

private:
	/**
	 * 設定済みのProjectRoot。
	 */
	FPath m_Root;
	/**
	 * Rootを設定済みか。
	 */
	bool m_bSet = false;
};
} // namespace Toolbox
#endif
