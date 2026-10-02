// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/FileWriter.h"
#include "Toolbox/FileError.h"
#include "FileWriteLoop.h"
#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif
namespace Toolbox
{
FFileWriter::FFileWriter(const FPath& Path)
{
#if defined(_WIN32)
	// 既存面を変更できないCREATE_NEWのハンドル。
	const HANDLE Handle = CreateFileW(ToWide(Path.ToUtf8()).CStr(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		throw FFileError(EFileOperation::Create, GetLastError());
	}
	m_Handle = static_cast<int64>(reinterpret_cast<intptr_t>(Handle));
#else
	// 上書きを許さない新規記述子。
	const int Descriptor = open(Path.ToUtf8().CStr(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
	if (Descriptor < 0)
	{
		throw FFileError(EFileOperation::Create, static_cast<uint32>(errno));
	}
	m_Handle = Descriptor;
#endif
}
FFileWriter::~FFileWriter() noexcept
{
	if (m_Handle != -1)
	{
#if defined(_WIN32)
		CloseHandle(reinterpret_cast<HANDLE>(static_cast<intptr_t>(m_Handle)));
#else
		close(static_cast<int>(m_Handle));
#endif
	}
}
void FFileWriter::Write(FStringView Bytes)
{
	if (m_Handle == -1)
	{
		throw FFileError(EFileOperation::Write, 0);
	}
	const auto WriteChunk = [this](const char* Data, uint32 Chunk)
	{
#if defined(_WIN32)
		// APIが返した実際の書き込み長。
		DWORD Written = 0;
		if (!WriteFile(reinterpret_cast<HANDLE>(static_cast<intptr_t>(m_Handle)), Data, Chunk, &Written, nullptr))
		{
			throw FFileError(EFileOperation::Write, GetLastError());
		}
		return static_cast<uint32>(Written);
#else
		while (true)
		{
			// OSから受け取る符号付きの書き込み結果。
			const ssize_t Written = write(static_cast<int>(m_Handle), Data, Chunk);
			if (Written >= 0)
			{
				return static_cast<uint32>(Written);
			}
			if (errno != EINTR)
			{
				throw FFileError(EFileOperation::Write, static_cast<uint32>(errno));
			}
		}
#endif
	};
	FilePrivate::WriteAll(Bytes, WriteChunk);
}
void FFileWriter::Flush()
{
	if (m_Handle == -1)
	{
		throw FFileError(EFileOperation::Flush, 0);
	}
#if defined(_WIN32)
	if (!FlushFileBuffers(reinterpret_cast<HANDLE>(static_cast<intptr_t>(m_Handle))))
	{
		throw FFileError(EFileOperation::Flush, GetLastError());
	}
#else
	if (fsync(static_cast<int>(m_Handle)) != 0)
	{
		throw FFileError(EFileOperation::Flush, static_cast<uint32>(errno));
	}
#endif
}
void FFileWriter::Close()
{
	if (m_Handle == -1)
	{
		return;
	}
	// close結果が不明でも、所有権を二回解放しない。
	const int64 Handle = Exchange(m_Handle, int64(-1));
#if defined(_WIN32)
	if (!CloseHandle(reinterpret_cast<HANDLE>(static_cast<intptr_t>(Handle))))
	{
		throw FFileError(EFileOperation::Close, GetLastError());
	}
#else
	if (close(static_cast<int>(Handle)) != 0)
	{
		throw FFileError(EFileOperation::Close, static_cast<uint32>(errno));
	}
#endif
}
} // namespace Toolbox
