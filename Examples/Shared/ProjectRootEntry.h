// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PROJECT_ROOT_ENTRY_H
#define DXF_PROJECT_ROOT_ENTRY_H
#include "Toolbox/ProjectPaths.h"
namespace Dxf
{
// exe横の開発パス設定を読み取りProjectRootを返す。設定がなければexe配置先を返す。
// 設定の解析失敗・読込失敗や存在しないRootの指定は例外で通知し、
// 配布構成へ黙って切り替えない。
// @param SettingsFileName exeと同じ場所に置く開発パス設定のファイル名。
inline Toolbox::FPath ResolveEntryProjectRoot(const char* SettingsFileName)
{
	// 実行ファイルが配置されているディレクトリ。
	const Toolbox::FPath ExeDirectory = Toolbox::ExecutableDirectory();
	// 読み取る開発パス設定のパス。
	const Toolbox::FPath SettingsPath = ExeDirectory / SettingsFileName;
	// 読み取った設定本文。
	Toolbox::FString Text;
	if (!Toolbox::TryReadSettingsFile(SettingsPath, Text))
	{
		return ExeDirectory;
	}
	// 解決したProjectRoot。
	Toolbox::FPath Root;
	if (!Toolbox::ResolveDevelopmentRoot(ExeDirectory, Text, Root))
	{
		throw Toolbox::FException("Broken development path settings");
	}
	if (!Toolbox::IsDirectory(Root))
	{
		throw Toolbox::FException("Project root from development path settings is missing");
	}
	return Root;
}
} // namespace Dxf
#endif
