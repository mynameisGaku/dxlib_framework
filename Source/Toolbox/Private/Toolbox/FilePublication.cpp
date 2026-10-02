// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/FilePublication.h"
#include "Toolbox/FileError.h"
#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <ShlObj.h>
#else
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#endif
namespace Toolbox
{
void PublishFile(const FPath& Temporary, const FPath& Destination)
{
#if defined(_WIN32)
	if (!MoveFileExW(ToWide(Temporary.ToUtf8()).CStr(), ToWide(Destination.ToUtf8()).CStr(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		throw FFileError(EFileOperation::Publish, GetLastError());
	}
#else
	if (rename(Temporary.ToUtf8().CStr(), Destination.ToUtf8().CStr()) != 0)
	{
		throw FFileError(EFileOperation::Publish, static_cast<uint32>(errno));
	}
#endif
}
bool RemoveOwnedFile(const FPath& Path) noexcept
{
	try
	{
#if defined(_WIN32)
		return DeleteFileW(ToWide(Path.ToUtf8()).CStr()) != 0;
#else
		return unlink(Path.ToUtf8().CStr()) == 0;
#endif
	}
	catch (...)
	{
		return false;
	}
}
FPath UserDataDirectory()
{
#if defined(_WIN32)
	// Known Folderが返した領域。成功・失敗のどちらでも解放する。
	PWSTR Text = nullptr;
	// 呼出し直後に保存する取得結果。
	const HRESULT Result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &Text);
	try
	{
		if (FAILED(Result) || Text == nullptr)
		{
			throw FFileError(EFileOperation::Root, static_cast<uint32>(Result));
		}
		// OSの文字列が解放される前にコピーする値。
		FPath Path(Text);
		CoTaskMemFree(Text);
		Text = nullptr;
		return Path;
	}
	catch (...)
	{
		CoTaskMemFree(Text);
		throw;
	}
#else
	// POSIXの明示されたユーザー領域。未設定なら失敗を返す。
	const char* Root = getenv("XDG_DATA_HOME");
	if (Root != nullptr && Root[0] == '/')
	{
		return FPath(Root);
	}
	// HOMEは読取専用に扱い、変更しない。
	const char* UserHome = getenv("HOME");
	if (UserHome == nullptr || UserHome[0] != '/')
	{
		throw FFileError(EFileOperation::Root, 0);
	}
	return FPath(UserHome) / FPath(".local/share");
#endif
}
} // namespace Toolbox
