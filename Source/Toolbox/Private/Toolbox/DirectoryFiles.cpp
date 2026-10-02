// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/DirectoryFiles.h"
#include "Toolbox/FileError.h"
#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#endif
namespace Toolbox
{
namespace
{
// 検査する末尾を勝手にglobとして解釈しない。
bool HasSuffix(FStringView Name, FStringView Suffix)
{
	return Name.Size() >= Suffix.Size() && FStringView(Name.Data() + Name.Size() - Suffix.Size(), Suffix.Size()) == Suffix;
}
} // namespace
TVector<FString> DirectoryFiles(const FPath& Directory, FStringView Suffix, size_t MaxCount)
{
	if (MaxCount == 0 || MaxCount > 1024 * 1024)
	{
		throw FException("Invalid directory entry limit");
	}
	// 返却する通常ファイルの名前。
	TVector<FString> Names;
#if defined(_WIN32)
	// 一回の列挙の情報。
	WIN32_FIND_DATAW Data{};
	// 全量の列挙を表すOS所有権。
	const HANDLE Handle = FindFirstFileW(ToWide((Directory / FPath("*")).ToUtf8()).CStr(), &Data);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		// 空領域とI/O失敗を区別する。
		const DWORD Error = GetLastError();
		if (Error == ERROR_FILE_NOT_FOUND)
		{
			return Names;
		}
		throw FFileError(EFileOperation::Read, Error);
	}
	try
	{
		do
		{
			if ((Data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0)
			{
				// UTF-8へ変換した一つの名前。
				const FString Name = FromWide(FWideString(Data.cFileName));
				if (HasSuffix(Name, Suffix))
				{
					if (Names.Size() == MaxCount)
					{
						throw FFileError(EFileOperation::SizeLimit, 0);
					}
					Names.PushBack(Name);
				}
			}
		} while (FindNextFileW(Handle, &Data));
		if (GetLastError() != ERROR_NO_MORE_FILES)
		{
			throw FFileError(EFileOperation::Read, GetLastError());
		}
	}
	catch (...)
	{
		FindClose(Handle);
		throw;
	}
	FindClose(Handle);
#else
	// サブツリーを辿らない列挙所有権。
	DIR* Handle = opendir(Directory.ToUtf8().CStr());
	if (Handle == nullptr)
	{
		throw FFileError(EFileOperation::Read, static_cast<uint32>(errno));
	}
	try
	{
		for (;;)
		{
			errno = 0;
			// readdirの寿命内だけ参照する名前。
			const dirent* Entry = readdir(Handle);
			if (Entry == nullptr)
			{
				if (errno != 0)
				{
					throw FFileError(EFileOperation::Read, static_cast<uint32>(errno));
				}
				break;
			}
			// linkを辿らず通常ファイルだけを返す。
			struct stat Info{};
			if (lstat((Directory / FPath(Entry->d_name)).ToUtf8().CStr(), &Info) != 0)
			{
				throw FFileError(EFileOperation::Read, static_cast<uint32>(errno));
			}
			if (S_ISREG(Info.st_mode) && HasSuffix(Entry->d_name, Suffix))
			{
				if (Names.Size() == MaxCount)
				{
					throw FFileError(EFileOperation::SizeLimit, 0);
				}
				Names.PushBack(FString(Entry->d_name));
			}
		}
	}
	catch (...)
	{
		closedir(Handle);
		throw;
	}
	closedir(Handle);
#endif
	return Names;
}
} // namespace Toolbox
