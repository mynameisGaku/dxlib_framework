// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/FileLock.h"
#include "Toolbox/FileError.h"
#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <errno.h>
#endif
namespace Toolbox
{
FFileLock::FFileLock(const FPath& Path)
{
#if defined(_WIN32)
	// 他のStore・プロセスと共有しないOS所有権。
	const HANDLE Handle = CreateFileW(ToWide(Path.ToUtf8()).CStr(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		// 次のAPI呼出しより前に原因を保存する。
		const DWORD Code = GetLastError();
		throw FFileError(Code == ERROR_SHARING_VIOLATION || Code == ERROR_LOCK_VIOLATION ? EFileOperation::Busy : EFileOperation::Create, Code);
	}
	m_Handle = static_cast<int64>(reinterpret_cast<intptr_t>(Handle));
#else
	// flockが適用されるファイル記述子。
	const int Descriptor = open(Path.ToUtf8().CStr(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
	if (Descriptor < 0)
	{
		throw FFileError(EFileOperation::Create, static_cast<uint32>(errno));
	}
	if (flock(Descriptor, LOCK_EX | LOCK_NB) != 0)
	{
		// closeより前に原因を保存する。
		const int Code = errno;
		close(Descriptor);
		throw FFileError(Code == EWOULDBLOCK ? EFileOperation::Busy : EFileOperation::Create, static_cast<uint32>(Code));
	}
	m_Handle = Descriptor;
#endif
}
FFileLock::~FFileLock() noexcept
{
#if defined(_WIN32)
	if (m_Handle != -1)
	{
		CloseHandle(reinterpret_cast<HANDLE>(static_cast<intptr_t>(m_Handle)));
	}
#else
	if (m_Handle >= 0)
	{
		close(static_cast<int>(m_Handle));
	}
#endif
}
} // namespace Toolbox
