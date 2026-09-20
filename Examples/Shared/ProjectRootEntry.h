// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PROJECT_ROOT_ENTRY_H
#define DXF_PROJECT_ROOT_ENTRY_H
#include "Toolbox/ProjectPaths.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
namespace Dxf
{
// exe横の開発パス設定を読み取りProjectRootを返す。設定がなければexe配置先を返す。
// 設定の解析失敗や存在しないRootの指定は例外で通知し、配布構成へ黙って切り替えない。
// @param SettingsFileName exeと同じ場所に置く開発パス設定のファイル名。
inline Toolbox::FPath ResolveEntryProjectRoot(const wchar_t* SettingsFileName)
{
	// 実行ファイルが配置されているディレクトリ。
	const Toolbox::FPath ExeDirectory = Toolbox::ExecutableDirectory();
	// 読み取る開発パス設定のパス。
	const Toolbox::FPath SettingsPath = ExeDirectory / SettingsFileName;
	// 設定ファイルを開くためのハンドル。存在しなければ配布構成として扱う。
	const HANDLE File = CreateFileW(Toolbox::ToWide(SettingsPath.ToUtf8()).CStr(), GENERIC_READ, FILE_SHARE_READ,
	                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (File == INVALID_HANDLE_VALUE)
	{
		return ExeDirectory;
	}
	// 読み取った設定本文のバイト列。
	Toolbox::TVector<char> Bytes(4096);
	// 実際に読み取ったバイト数。
	DWORD Read = 0;
	// 読み取りに成功したか。
	const BOOL bRead = ReadFile(File, Bytes.Data(), static_cast<DWORD>(Bytes.Size() - 1), &Read, nullptr);
	CloseHandle(File);
	if (!bRead)
	{
		throw Toolbox::FException("Cannot read the development path settings");
	}
	// 読み取った設定本文。
	const Toolbox::FString Text(Bytes.Data(), Read);
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
