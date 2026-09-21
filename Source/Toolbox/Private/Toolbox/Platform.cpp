// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Platform.h"
#include <errno.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#undef CreateDirectory
#undef CopyFile
#else
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#endif
namespace Toolbox
{
namespace
{
// 開いた読み取りハンドルを閉じる所有権。
struct FReadGuard
{
#if defined(_WIN32)
	// 所有するOS固有ハンドル。
	HANDLE Handle;
	// 無効なハンドルで初期化する。
	FReadGuard() : Handle(INVALID_HANDLE_VALUE)
	{
	}
#else
	// 所有するOS固有ハンドル。
	int Descriptor;
	// 無効な記述子で初期化する。
	FReadGuard() : Descriptor(-1)
	{
	}
#endif
	// 所有するハンドルを閉じる。
	~FReadGuard()
	{
#if defined(_WIN32)
		if (Handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(Handle);
		}
#else
		if (Descriptor >= 0)
		{
			close(Descriptor);
		}
#endif
	}
	// 所有権の複製を禁止する。
	FReadGuard(const FReadGuard&) = delete;
	FReadGuard& operator=(const FReadGuard&) = delete;
};
} // namespace
// 壁時計に影響されない単調増加時刻をナノ秒で返す。
uint64 MonotonicNanoseconds()
{
#if defined(_WIN32)
	// 高精度時計が返した現在のカウンター。
	LARGE_INTEGER Counter{};
	// 高精度時計が一秒間に進むカウント数。
	LARGE_INTEGER Frequency{};
	if (!QueryPerformanceCounter(&Counter) || !QueryPerformanceFrequency(&Frequency))
	{
		throw FException("Monotonic clock unavailable");
	}
	return static_cast<uint64>(Counter.QuadPart / Frequency.QuadPart) * 1000000000ULL +
	       static_cast<uint64>((Counter.QuadPart % Frequency.QuadPart) * 1000000000ULL / Frequency.QuadPart);
#else
	// 秒とナノ秒に分かれた単調増加時刻。
	timespec Value{};
	if (clock_gettime(CLOCK_MONOTONIC, &Value) != 0)
	{
		throw FException("Monotonic clock unavailable");
	}
	return static_cast<uint64>(Value.tv_sec) * 1000000000ULL + static_cast<uint64>(Value.tv_nsec);
#endif
}
// UTF-8をワイド文字列へ変換する。不正な入力は例外で通知する。
// @param Text 読み取る文字列。
FWideString ToWide(const FString& Text)
{
#if defined(_WIN32)
	if (Text.IsEmpty())
	{
		return {};
	}
	if (Text.Size() > static_cast<size_t>(INT_MAX))
	{
		throw FException("Path too long");
	}
	// 変換または読み取りに必要な文字・バイト数。
	const int32 Count =
	    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.Data(), static_cast<int32>(Text.Size()), nullptr, 0);
	if (!Count)
	{
		throw FException("Invalid UTF-8");
	}
	// 処理結果を組み立てる一時領域。
	FWideString Result;
	Result.Resize(static_cast<size_t>(Count));
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.Data(), static_cast<int32>(Text.Size()), Result.Data(),
	                         Count))
	{
		throw FException("UTF-8 conversion failed");
	}
	return Result;
#else
	// 処理結果を組み立てる一時領域。
	FWideString Result;
	// 現在の要素位置を進めて範囲を走査する。
	for (size_t I = 0; I < Text.Size();)
	{
		// 変換中のUnicodeコードポイント。
		uint32 Code = static_cast<unsigned char>(Text[I++]);
		// コードポイントを完成させるために必要な後続バイト数。
		uint32 Remaining = 0;
		// このUTF-8バイト長で許される最小コードポイント。
		uint32 Minimum = 0;
		if (Code >= 0xF0 && Code <= 0xF4)
		{
			Code &= 7;
			Remaining = 3;
			Minimum = 0x10000;
		}
		else if (Code >= 0xE0 && Code <= 0xEF)
		{
			Code &= 15;
			Remaining = 2;
			Minimum = 0x800;
		}
		else if (Code >= 0xC2 && Code <= 0xDF)
		{
			Code &= 31;
			Remaining = 1;
			Minimum = 0x80;
		}
		else if (Code >= 0x80)
		{
			throw FException("Invalid UTF-8");
		}
		while (Remaining--)
		{
			if (I == Text.Size() || (static_cast<unsigned char>(Text[I]) & 0xC0) != 0x80)
			{
				throw FException("Invalid UTF-8");
			}
			Code = (Code << 6) | (static_cast<unsigned char>(Text[I++]) & 63);
		}
		if (Code < Minimum || Code > 0x10FFFF || (Code >= 0xD800 && Code <= 0xDFFF))
		{
			throw FException("Invalid UTF-8 code point");
		}
		Result.PushBack(static_cast<wchar_t>(Code));
	}
	return Result;
#endif
}
// ワイド文字列をUTF-8へ変換する。不正な入力は例外で通知する。
// @param Text 読み取る文字列。
FString FromWide(const FWideString& Text)
{
#if defined(_WIN32)
	if (Text.IsEmpty())
	{
		return {};
	}
	if (Text.Size() > static_cast<size_t>(INT_MAX))
	{
		throw FException("Path too long");
	}
	// 変換または読み取りに必要な文字・バイト数。
	const int32 Count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Text.Data(), static_cast<int32>(Text.Size()),
	                                        nullptr, 0, nullptr, nullptr);
	if (!Count)
	{
		throw FException("Invalid UTF-16");
	}
	// 処理結果を組み立てる一時領域。
	FString Result;
	Result.Resize(static_cast<size_t>(Count));
	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Text.Data(), static_cast<int32>(Text.Size()), Result.Data(),
	                         Count, nullptr, nullptr))
	{
		throw FException("UTF-16 conversion failed");
	}
	return Result;
#else
	// 処理結果を組み立てる一時領域。
	FString Result;
	// ワイド文字のコードポイントを順に取り出す。
	for (wchar_t Value : Text)
	{
		// 変換中のUnicodeコードポイント。
		const uint32 Code = static_cast<uint32>(Value);
		if (Code < 0x80)
		{
			Result.PushBack(static_cast<char>(Code));
		}
		else if (Code < 0x800)
		{
			Result.PushBack(static_cast<char>(0xC0 | (Code >> 6)));
			Result.PushBack(static_cast<char>(0x80 | (Code & 63)));
		}
		else if (Code < 0x10000 && !(Code >= 0xD800 && Code <= 0xDFFF))
		{
			Result.PushBack(static_cast<char>(0xE0 | (Code >> 12)));
			Result.PushBack(static_cast<char>(0x80 | ((Code >> 6) & 63)));
			Result.PushBack(static_cast<char>(0x80 | (Code & 63)));
		}
		else if (Code >= 0x10000 && Code <= 0x10FFFF)
		{
			Result.PushBack(static_cast<char>(0xF0 | (Code >> 18)));
			Result.PushBack(static_cast<char>(0x80 | ((Code >> 12) & 63)));
			Result.PushBack(static_cast<char>(0x80 | ((Code >> 6) & 63)));
			Result.PushBack(static_cast<char>(0x80 | (Code & 63)));
		}
		else
		{
			throw FException("Invalid wide character");
		}
	}
	return Result;
#endif
}
// 指定した値を使って初期状態を構築する。
// @param Text 読み取る文字列。
FPath::FPath(const wchar_t* Text) : m_Text(FromWide(FWideString(Text)))
{
}
// 指定した値を使って初期状態を構築する。
// @param Text 読み取る文字列。
FPath::FPath(const FWideString& Text) : m_Text(FromWide(Text))
{
}
// パスの区切りや相対成分を整理する。ファイルへはアクセスしない。
FPath FPath::Normalize() const
{
	if (m_Text.IsEmpty())
	{
		return {};
	}
	// 変換または正規化の対象文字列。
	FString Text = m_Text;
#if defined(_WIN32)
	// 現在の要素位置を進めて範囲を走査する。
	for (size_t I = 0; I < Text.Size(); ++I)
	{
		if (Text[I] == '\\')
		{
			Text[I] = '/';
		}
	}
#endif
	// ドライブ名やルートを含むパス先頭部分。
	FString Prefix;
	// ルート名の次から走査する開始位置。
	size_t Start = 0;
#if defined(_WIN32)
	if (Text.Size() >= 2 && Text[1] == ':')
	{
		Prefix = Text.Substr(0, 2);
		Start = 2;
	}
	else if (Text.Size() > 2 && Text[0] == '/' && Text[1] == '/' && Text[2] != '/')
	{
		Start = 2;
		while (Start < Text.Size() && Text[Start] != '/')
		{
			++Start;
		}
		Prefix = Text.Substr(0, Start);
	}
#endif
	// パスにルートディレクトリが含まれるかどうか。
	const bool Absolute = Start < Text.Size() && Text[Start] == '/';
	if (Absolute)
	{
		Prefix += "/";
	}
	// 正規化後に残すパス構成要素。
	TVector<FString> Parts;
	// 末尾のディレクトリ区切りを維持するかどうか。
	bool TrailingSeparator = Text.Back() == '/';
	// 現在の要素位置を進めて範囲を走査する。
	for (size_t I = Start; I < Text.Size();)
	{
		if (Text[I] == '/')
		{
			++I;
			continue;
		}
		// パス構成要素の開始位置。
		const size_t Begin = I;
		while (I < Text.Size() && Text[I] != '/')
		{
			++I;
		}
		// 現在処理している一つのパス構成要素。
		FString Part = Text.Substr(Begin, I - Begin);
		if (Part == ".")
		{
			if (I == Text.Size())
			{
				TrailingSeparator = true;
			}
			continue;
		}
		if (Part == ".." && !Parts.IsEmpty() && !(Parts.Back() == ".."))
		{
			Parts.PopBack();
			if (I == Text.Size())
			{
				TrailingSeparator = true;
			}
		}
		else if (!(Absolute && Part == ".."))
		{
			Parts.PushBack(Move(Part));
		}
	}
	// 処理結果を組み立てる一時領域。
	FString Result = Prefix;
	// 現在の要素位置を進めて範囲を走査する。
	for (size_t I = 0; I < Parts.Size(); ++I)
	{
		if (I)
		{
			Result += "/";
		}
		Result += Parts[I];
	}
	if (Result.IsEmpty())
	{
		return FPath(".");
	}
	if (TrailingSeparator && !Parts.IsEmpty() && !(Parts.Back() == "..") && Result.Back() != '/')
	{
		Result += "/";
	}
	return FPath(Move(Result));
}
// 最後のパス要素を除いた親ディレクトリを返す。
FPath FPath::Parent() const
{
	// 変換または正規化の対象文字列。
	FString Text = Normalize().ToUtf8();
	// 現在の要素位置を進めて範囲を走査する。
	for (size_t I = Text.Size(); I > 0; --I)
	{
		if (Text[I - 1] == '/')
		{
			return FPath(Text.Substr(0, I == 1 ? 1 : I - 1));
		}
	}
	return {};
}
// 二つのパスを結合する。右側が絶対パスなら右側を返す。
// @param Left 結合元のパス。
// @param Right 追加するパス。
FPath operator/(const FPath& Left, const FPath& Right)
{
	if (Left.m_Text.IsEmpty())
	{
		return Right;
	}
	// 結合する右側のパス。絶対パスなら左側を置き換える。
	const FString& R = Right.m_Text;
	if (!R.IsEmpty() && (R[0] == '/' || R[0] == '\\' || (R.Size() > 1 && R[1] == ':')))
	{
		return Right;
	}
	return FPath(Left.m_Text + "/" + R);
}
// OSから現在の作業ディレクトリを取得する。
FPath CurrentDirectory()
{
#if defined(_WIN32)
	// 変換または読み取りに必要な文字・バイト数。
	const DWORD Count = GetCurrentDirectoryW(0, nullptr);
	if (!Count)
	{
		throw FException("Cannot read current directory");
	}
	// OS呼び出しや変換に使う一時領域。
	TVector<wchar_t> Buffer(Count);
	// OSが実際に書き込んだ文字数。
	const DWORD Written = GetCurrentDirectoryW(Count, Buffer.Data());
	if (!Written || Written >= Count)
	{
		throw FException("Cannot read current directory");
	}
	return FPath(FWideString(Buffer.Data(), Written));
#else
	// OS呼び出しや変換に使う一時領域。
	TVector<char> Buffer(256);
	while (!getcwd(Buffer.Data(), Buffer.Size()))
	{
		if (errno != ERANGE)
		{
			throw FException("Cannot read current directory");
		}
		Buffer.Resize(Buffer.Size() * 2);
	}
	return FPath(Buffer.Data());
#endif
}
// 実行ファイルが配置されているディレクトリを取得する。
FPath ExecutableDirectory()
{
#if defined(_WIN32)
	// 実行ファイル名を受け取る作業領域。
	TVector<wchar_t> Buffer(512);
	for (;;)
	{
		// 有効な要素数。
		const DWORD Size = GetModuleFileNameW(nullptr, Buffer.Data(), static_cast<DWORD>(Buffer.Size()));
		if (Size == 0)
		{
			throw FException("GetModuleFileNameW failed");
		}
		if (Size < Buffer.Size())
		{
			return FPath(FWideString(Buffer.Data(), Size)).Parent();
		}
		if (Buffer.Size() >= 32768)
		{
			throw FException("Executable path is too long");
		}
		Buffer.Resize(Buffer.Size() * 2);
	}
#else
	// 実行ファイルの配置を読む符号付きの読み取り結果。
	char Self[32] = "/proc/self/exe";
	// OS呼び出しや変換に使う一時領域。
	TVector<char> Buffer(256);
	for (;;)
	{
		// 解決したバイト数。
		const ssize_t Count = readlink(Self, Buffer.Data(), Buffer.Size());
		if (Count < 0)
		{
			throw FException("Cannot read executable directory");
		}
		if (static_cast<size_t>(Count) < Buffer.Size())
		{
			return FPath(FString(Buffer.Data(), static_cast<size_t>(Count))).Parent();
		}
		Buffer.Resize(Buffer.Size() * 2);
	}
#endif
}
// OSの一時ディレクトリを取得する。
FPath TemporaryDirectory(){
#if defined(_WIN32)
	// OS呼び出しや変換に使う一時領域。
	TVector<wchar_t> Buffer(32768);
	// OSが実際に書き込んだ文字数。
	DWORD Written = GetTempPathW(static_cast<DWORD>(Buffer.Size()), Buffer.Data());
	if (!Written || Written >= Buffer.Size())
	{
		throw FException("Cannot read temporary directory");
	}
	return FPath(FWideString(Buffer.Data(), Written));
#else
	// OSから得たディレクトリのパス。
	const char* Path = getenv("TMPDIR");
	return FPath(Path && *Path ? Path : "/tmp");
#endif
}
// 新しいディレクトリを作る。既存ならfalse、他の失敗は例外を返す。
// @param Path 操作対象のパス。
bool CreateDirectory(const FPath& Path)
{
#if defined(_WIN32)
	if (CreateDirectoryW(ToWide(Path.ToUtf8()).CStr(), nullptr))
	{
		return true;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		return false;
	}
#else
	if (mkdir(Path.ToUtf8().CStr(), 0700) == 0)
	{
		return true;
	}
	if (errno == EEXIST)
	{
		return false;
	}
#endif
	throw FException("Cannot create directory");
}
// 指定パスが通常のファイルを指しているか調べる。
// @param Path 操作対象のパス。
bool IsRegularFile(const FPath& Path)
{
#if defined(_WIN32)
	// 対象がディレクトリや再解析ポイントかを示す属性。
	const DWORD Attributes = GetFileAttributesW(ToWide(Path.ToUtf8()).CStr());
	return Attributes != INVALID_FILE_ATTRIBUTES && !(Attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
	// ファイル種別を含むPOSIXの属性情報。
	struct stat Info{};
	return stat(Path.ToUtf8().CStr(), &Info) == 0 && S_ISREG(Info.st_mode);
#endif
}
// 指定パスがディレクトリを指しているか調べる。
// @param Path 操作対象のパス。
bool IsDirectory(const FPath& Path)
{
#if defined(_WIN32)
	// 対象がディレクトリかどうかを示す属性。
	const DWORD Attributes = GetFileAttributesW(ToWide(Path.ToUtf8()).CStr());
	return Attributes != INVALID_FILE_ATTRIBUTES && (Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	// ファイル種別を含むPOSIXの属性情報。
	struct stat Info{};
	return stat(Path.ToUtf8().CStr(), &Info) == 0 && S_ISDIR(Info.st_mode);
#endif
}
// ファイル全体を読み取る。存在しなければfalseを返す。
// @param Path 読み取るファイルのパス。
// @param Out 読み取ったバイト列の格納先。失敗時は変更しない。
// @param MaxBytes 受け付ける最大バイト数。
bool ReadFileBytes(const FPath& Path, TVector<uint8>& Out, size_t MaxBytes)
{
#if defined(_WIN32)
	// 所有権と共に閉じる読み取りハンドル。
	FReadGuard Guard;
	Guard.Handle =
	    CreateFileW(ToWide(Path.ToUtf8()).CStr(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
	                FILE_ATTRIBUTE_NORMAL, nullptr);
	if (Guard.Handle == INVALID_HANDLE_VALUE)
	{
		// 開けなかった理由の番号。
		const DWORD Error = GetLastError();
		if (Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND)
		{
			return false;
		}
		throw FException("Cannot open file for reading");
	}
	// ファイル全体のバイト数。
	LARGE_INTEGER Size{};
	if (!GetFileSizeEx(Guard.Handle, &Size) || Size.QuadPart < 0 ||
	    static_cast<uint64>(Size.QuadPart) > MaxBytes)
	{
		throw FException("File is too large to read");
	}
	// 読み取ったバイト列。
	TVector<uint8> Bytes(static_cast<size_t>(Size.QuadPart));
	// 読み取り済みのバイト数。
	size_t Done = 0;
	while (Done < Bytes.Size())
	{
		// 今回読み取ったバイト数。
		DWORD Chunk = 0;
		if (!ReadFile(Guard.Handle, Bytes.Data() + Done, static_cast<DWORD>(Bytes.Size() - Done), &Chunk, nullptr) ||
		    Chunk == 0)
		{
			throw FException("Cannot read file contents");
		}
		Done += Chunk;
	}
	Out = Toolbox::Move(Bytes);
	return true;
#else
	// 所有権と共に閉じる読み取り記述子。
	FReadGuard Guard;
	Guard.Descriptor = open(Path.ToUtf8().CStr(), O_RDONLY);
	if (Guard.Descriptor < 0)
	{
		if (errno == ENOENT || errno == ENOTDIR)
		{
			return false;
		}
		throw FException("Cannot open file for reading");
	}
	// ファイル種別を含む属性情報。
	struct stat Info{};
	if (fstat(Guard.Descriptor, &Info) != 0 || !S_ISREG(Info.st_mode) || Info.st_size < 0 ||
	    static_cast<uint64>(Info.st_size) > MaxBytes)
	{
		throw FException("File is too large to read");
	}
	// 読み取ったバイト列。
	TVector<uint8> Bytes(static_cast<size_t>(Info.st_size));
	// 読み取り済みのバイト数。
	size_t Done = 0;
	while (Done < Bytes.Size())
	{
		// 今回読み取ったバイト数。
		const ssize_t Chunk = read(Guard.Descriptor, Bytes.Data() + Done, Bytes.Size() - Done);
		if (Chunk <= 0)
		{
			throw FException("Cannot read file contents");
		}
		Done += static_cast<size_t>(Chunk);
	}
	Out = Toolbox::Move(Bytes);
	return true;
#endif
}
// ファイルを新規コピーする。既存のコピー先は上書きしない。
// @param Source 読み取り元のパス。
// @param Destination 新規作成するコピー先のパス。
void CopyFile(const FPath& Source, const FPath& Destination)
{
#if defined(_WIN32)
	if (!CopyFileW(ToWide(Source.ToUtf8()).CStr(), ToWide(Destination.ToUtf8()).CStr(), TRUE))
	{
		throw FException("Cannot copy file");
	}
#else
	// コピー元のファイルハンドル。
	FILE* Input = fopen(Source.ToUtf8().CStr(), "rb");
	if (!Input)
	{
		throw FException("Cannot open source file");
	}
	// コピー先のファイルハンドル。
	FILE* Output = fopen(Destination.ToUtf8().CStr(), "wbx");
	if (!Output)
	{
		fclose(Input);
		throw FException("Cannot create destination file");
	}
	// OS呼び出しや変換に使う一時領域。
	char Buffer[8192];
	// 変換または読み取りに必要な文字・バイト数。
	size_t Count;
	// 読み書きのいずれかで失敗があったか。
	bool Failed = false;
	while ((Count = fread(Buffer, 1, sizeof(Buffer), Input)) != 0)
	{
		if (fwrite(Buffer, 1, Count, Output) != Count)
		{
			Failed = true;
			break;
		}
	}
	Failed = Failed || ferror(Input);
	fclose(Input);
	if (fclose(Output) != 0)
	{
		Failed = true;
	}
	if (Failed)
	{
		throw FException("Cannot copy file");
	}
#endif
}
// リンク先を辿らずツリーを削除する。失敗はErrorへ返す。
// @param Path 操作対象のパス。
// @param Error 失敗時のエラーコード。成功時は0。
void RemoveTree(const FPath& Path, int32& Error) noexcept
{
	Error = 0;
	try
	{
#if defined(_WIN32)
		// Windows APIに渡すUTF-16のパス。
		const FWideString Wide = ToWide(Path.ToUtf8());
		// 対象がディレクトリや再解析ポイントかを示す属性。
		const DWORD Attributes = GetFileAttributesW(Wide.CStr());
		if (Attributes == INVALID_FILE_ATTRIBUTES)
		{
			// 変換中のUnicodeコードポイント。
			const DWORD Code = GetLastError();
			if (Code != ERROR_FILE_NOT_FOUND && Code != ERROR_PATH_NOT_FOUND)
			{
				Error = static_cast<int32>(Code);
			}
			return;
		}
		if (Attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!(Attributes & FILE_ATTRIBUTE_REPARSE_POINT))
			{
				// OSから読み取ったディレクトリ内の一項目。
				WIN32_FIND_DATAW Entry{};
				// 列挙中のWindows検索ハンドル。終了時に閉じる。
				HANDLE Search = FindFirstFileW(ToWide((Path / "*").ToUtf8()).CStr(), &Entry);
				if (Search == INVALID_HANDLE_VALUE)
				{
					Error = static_cast<int32>(GetLastError());
					return;
				}
				try
				{
					do
					{
						// 列挙中の項目名。親と自己の項目は除外する。
						const FWideString Name(Entry.cFileName);
						if (Name == L"." || Name == L"..")
						{
							continue;
						}
						RemoveTree(Path / FPath(Name), Error);
						if (Error)
						{
							break;
						}
					} while (FindNextFileW(Search, &Entry));
					if (!Error && GetLastError() != ERROR_NO_MORE_FILES)
					{
						Error = static_cast<int32>(GetLastError());
					}
				}
				catch (...)
				{
					FindClose(Search);
					throw;
				}
				FindClose(Search);
				if (Error)
				{
					return;
				}
			}
			if (!RemoveDirectoryW(Wide.CStr()))
			{
				Error = static_cast<int32>(GetLastError());
			}
		}
		else if (!DeleteFileW(Wide.CStr()))
		{
			Error = static_cast<int32>(GetLastError());
		}
#else
		// ファイル種別を含むPOSIXの属性情報。
		struct stat Info{};
		if (lstat(Path.ToUtf8().CStr(), &Info) != 0)
		{
			if (errno != ENOENT)
			{
				Error = errno;
			}
			return;
		}
		if (S_ISDIR(Info.st_mode))
		{
			// POSIXのディレクトリ列挙ハンドル。終了時に閉じる。
			DIR* Directory = opendir(Path.ToUtf8().CStr());
			if (!Directory)
			{
				Error = errno;
				return;
			}
			try
			{
				errno = 0;
				// ディレクトリから次の項目を読み取る。
				while (dirent* Entry = readdir(Directory))
				{
					// 列挙中の項目名。親と自己の項目は除外する。
					const FString Name(Entry->d_name);
					if (Name == "." || Name == "..")
					{
						continue;
					}
					RemoveTree(Path / Name, Error);
					if (Error)
					{
						break;
					}
					errno = 0;
				}
				if (!Error && errno)
				{
					Error = errno;
				}
			}
			catch (...)
			{
				closedir(Directory);
				throw;
			}
			closedir(Directory);
			if (Error)
			{
				return;
			}
			if (rmdir(Path.ToUtf8().CStr()) != 0)
			{
				Error = errno;
			}
		}
		else if (unlink(Path.ToUtf8().CStr()) != 0)
		{
			Error = errno;
		}
#endif
	}
	catch (...)
	{
		Error = -1;
	}
}
} // namespace Toolbox
