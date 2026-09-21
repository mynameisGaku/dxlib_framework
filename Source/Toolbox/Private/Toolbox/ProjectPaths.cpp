// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/ProjectPaths.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace Toolbox
{
namespace
{
// UTF-8本文が正しい符号化か調べる。
// @param Text 検証する文字列。
bool IsValidUtf8_Internal(const FString& Text) noexcept
{
	// 検証中のバイト位置。
	size_t Index = 0;
	while (Index < Text.Size())
	{
		// 復号中のコードポイント。
		uint32 Code = static_cast<unsigned char>(Text[Index++]);
		// 必要な後続バイト数。
		uint32 Remaining = 0;
		// この長さで許される最小コードポイント。
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
			return false;
		}
		while (Remaining-- != 0)
		{
			if (Index == Text.Size() || (static_cast<unsigned char>(Text[Index]) & 0xC0) != 0x80)
			{
				return false;
			}
			Code = (Code << 6) | (static_cast<unsigned char>(Text[Index++]) & 63);
		}
		if (Code < Minimum || Code > 0x10FFFF || (Code >= 0xD800 && Code <= 0xDFFF))
		{
			return false;
		}
	}
	return true;
}
// 埋め込みNULを含むか調べる。
// @param Text 検証する文字列。
bool HasNul_Internal(const FString& Text) noexcept
{
	// 走査中のバイト位置。
	for (size_t Index = 0; Index < Text.Size(); ++Index)
	{
		if (Text[Index] == '\0')
		{
			return true;
		}
	}
	return false;
}
// ドライブ文字から始まるか調べる。C: や C:/ の両方に一致する。
// @param Text 検証する正規化済み文字列。
bool HasDrivePrefix_Internal(const FString& Text) noexcept
{
	if (Text.Size() < 2)
	{
		return false;
	}
	// ドライブ文字の候補。
	const char Drive = Text[0];
	// ドライブ文字として許すASCII英字。
	const bool bLetter = (Drive >= 'A' && Drive <= 'Z') || (Drive >= 'a' && Drive <= 'z');
	return bLetter && Text[1] == ':';
}
// 完全修飾の絶対パスか調べる。ドライブ付きとUNCを受け付ける。
// @param Normalized 正規化済みのパス文字列。
bool IsFullyQualified_Internal(const FString& Normalized) noexcept
{
	if (Normalized.Size() >= 3 && HasDrivePrefix_Internal(Normalized) && Normalized[2] == '/')
	{
		return true;
	}
	if (Normalized.Size() >= 3 && Normalized[0] == '/' && Normalized[1] == '/' && Normalized[2] != '/')
	{
		return true;
	}
	return false;
}
// 正規化したRootの末尾区切りを除く。C:/ や / は残す。
// @param Root 正規化済みのRoot文字列。
FString TrimTrailingSeparator_Internal(const FString& Root)
{
	// 末尾区切りを除いた候補。
	FString Trimmed = Root;
	while (Trimmed.Size() > 3 && Trimmed.Back() == '/')
	{
		Trimmed.PopBack();
	}
	if (Trimmed.Size() == 3 && Trimmed.Back() == '/' && HasDrivePrefix_Internal(Trimmed))
	{
		return Trimmed;
	}
	while (Trimmed.Size() > 1 && Trimmed.Back() == '/')
	{
		Trimmed.PopBack();
	}
	return Trimmed;
}
#if defined(_WIN32)
// 開いた設定ファイルを閉じる所有権。
struct FFileCloser
{
	// 所有するファイルハンドル。
	HANDLE m_Handle;
	// 所有するハンドルを閉じる。
	~FFileCloser()
	{
		CloseHandle(m_Handle);
	}
};
#else
// 開いた設定ファイルを閉じる所有権。
struct FFileCloser
{
	// 所有するファイル記述子。
	int m_Descriptor;
	// 所有する記述子を閉じる。
	~FFileCloser()
	{
		close(m_Descriptor);
	}
};
#endif
} // namespace
// 小さなバージョン付き開発パス設定を読み取る。
// @param Text 設定ファイルのUTF-8本文。
// @param Destination 読み取った設定の格納先。
bool ParseProjectPathSettings(const FString& Text, FProjectPathSettings& Destination)
{
	if (!IsValidUtf8_Internal(Text))
	{
		return false;
	}
	// 読み取り中の設定値。
	FProjectPathSettings Parsed;
	// 必須項目の読み取り状態。
	bool bVersion = false;
	bool bMode = false;
	bool bRelative = false;
	// 現在処理している行の開始位置。
	size_t Begin = 0;
	while (Begin <= Text.Size())
	{
		// 現在処理している行の終端位置。
		size_t End = Begin;
		while (End < Text.Size() && Text[End] != '\n')
		{
			++End;
		}
		// 末尾の復帰文字を除いた行本文。
		FString Line = Text.Substr(Begin, End - Begin);
		if (!Line.IsEmpty() && Line.Back() == '\r')
		{
			Line.PopBack();
		}
		if (!Line.IsEmpty() && Line[0] != '#')
		{
			// 項目名と値の区切り位置。
			size_t Separator = 0;
			while (Separator < Line.Size() && Line[Separator] != '=')
			{
				++Separator;
			}
			if (Separator == 0 || Separator == Line.Size())
			{
				return false;
			}
			// 項目名と値。
			FString Key = Line.Substr(0, Separator);
			FString Value = Line.Substr(Separator + 1);
			if (Key == "Version")
			{
				if (bVersion || Value != "1")
				{
					return false;
				}
				Parsed.Version = 1;
				bVersion = true;
			}
			else if (Key == "Mode")
			{
				if (bMode || Value.IsEmpty())
				{
					return false;
				}
				Parsed.Mode = Value;
				bMode = true;
			}
			else if (Key == "ProjectRootRelative")
			{
				if (bRelative || Value.IsEmpty())
				{
					return false;
				}
				Parsed.ProjectRootRelative = Value;
				bRelative = true;
			}
			else
			{
				return false;
			}
		}
		Begin = End + 1;
	}
	if (!bVersion || !bMode || !bRelative)
	{
		return false;
	}
	Destination = Parsed;
	return true;
}
// exe配置先と開発パス設定からProjectRootを解決する。字句的で存在確認はしない。
// @param ExeDirectory 実行ファイルが配置されている完全修飾ディレクトリ。
// @param SettingsText 開発パス設定ファイルのUTF-8本文。
// @param Root 解決したProjectRootの格納先。
bool ResolveDevelopmentRoot(const FPath& ExeDirectory, const FString& SettingsText, FPath& Root)
{
	// 読み取った開発パス設定。
	FProjectPathSettings Settings;
	if (!ParseProjectPathSettings(SettingsText, Settings))
	{
		return false;
	}
	// 別構成の開発専用絶対パスはそのまま使う。
	FString Joined = FPath(Settings.ProjectRootRelative).Normalize().ToUtf8();
	if (!IsFullyQualified_Internal(Joined) && !(Joined.Size() != 0 && Joined[0] == '/'))
	{
		Joined = ExeDirectory.ToUtf8() + "/" + Settings.ProjectRootRelative;
	}
	FString Candidate = TrimTrailingSeparator_Internal(FPath(Move(Joined)).Normalize().ToUtf8());
	if (!IsFullyQualified_Internal(Candidate) && !(Candidate.Size() != 0 && Candidate[0] == '/'))
	{
		return false;
	}
	Root = FPath(Move(Candidate));
	return true;
}
// 開発パス設定ファイルを読み取る。存在しなければfalseを返す。
// @param Path 読み取る設定ファイルのパス。
// @param Text 読み取ったUTF-8本文の格納先。失敗時は変更しない。
bool TryReadSettingsFile(const FPath& Path, FString& Text)
{
#if defined(_WIN32)
	// 読み取る設定ファイルのハンドル。
	HANDLE File =
	    CreateFileW(ToWide(Path.ToUtf8()).CStr(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
	                FILE_ATTRIBUTE_NORMAL, nullptr);
	if (File == INVALID_HANDLE_VALUE)
	{
		// 開けなかった理由の番号。
		const DWORD Error = GetLastError();
		if (Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND)
		{
			return false;
		}
		throw FException("Cannot open the settings file");
	}
	// 所有権と共に閉じる対象。
	FFileCloser Closer{File};
	// ファイル全体のバイト数。
	LARGE_INTEGER Size{};
	if (!GetFileSizeEx(File, &Size) || Size.QuadPart < 0 ||
	    static_cast<uint64>(Size.QuadPart) > MaxSettingsFileBytes)
	{
		throw FException("Settings file is too large");
	}
	// 読み取った本文の作業領域。
	TVector<char> Bytes(static_cast<size_t>(Size.QuadPart));
	// 読み取り済みのバイト数。
	size_t Done = 0;
	while (Done < Bytes.Size())
	{
		// 今回読み取ったバイト数。
		DWORD Chunk = 0;
		if (!ReadFile(File, Bytes.Data() + Done, static_cast<DWORD>(Bytes.Size() - Done), &Chunk, nullptr) ||
		    Chunk == 0)
		{
			throw FException("Cannot read the settings file");
		}
		Done += Chunk;
	}
	Text = FString(Bytes.Data(), Done);
	return true;
#else
	// 読み取る設定ファイルの記述子。
	const int Descriptor = open(Path.ToUtf8().CStr(), O_RDONLY);
	if (Descriptor < 0)
	{
		if (errno == ENOENT || errno == ENOTDIR)
		{
			return false;
		}
		throw FException("Cannot open the settings file");
	}
	// 所有権と共に閉じる対象。
	FFileCloser Closer{Descriptor};
	// ファイル種別を含む属性情報。
	struct stat Info{};
	if (fstat(Descriptor, &Info) != 0 || !S_ISREG(Info.st_mode) || Info.st_size < 0 ||
	    static_cast<uint64>(Info.st_size) > MaxSettingsFileBytes)
	{
		throw FException("Settings file is not readable");
	}
	// 読み取った本文の作業領域。
	TVector<char> Bytes(static_cast<size_t>(Info.st_size));
	// 読み取り済みのバイト数。
	size_t Done = 0;
	while (Done < Bytes.Size())
	{
		// 今回読み取ったバイト数。
		const ssize_t Chunk = read(Descriptor, Bytes.Data() + Done, Bytes.Size() - Done);
		if (Chunk <= 0)
		{
			throw FException("Cannot read the settings file");
		}
		Done += static_cast<size_t>(Chunk);
	}
	Text = FString(Bytes.Data(), Done);
	return true;
#endif
}
// ProjectRootを一度だけ設定する。絶対パスを要求する。
// @param Root sln配置先の完全修飾ディレクトリ。
bool FAssetPathResolver::SetRoot(const FPath& Root)
{
	if (m_bSet)
	{
		return false;
	}
	// 正規化したRoot候補。
	FString Normalized = TrimTrailingSeparator_Internal(Root.Normalize().ToUtf8());
	if (!IsFullyQualified_Internal(Normalized) && !(Normalized.Size() != 0 && Normalized[0] == '/'))
	{
		return false;
	}
	m_Root = FPath(Move(Normalized));
	m_bSet = true;
	return true;
}
// 論理パスを解決する。相対パスはRootと結合し正規化する。
// @param LogicalPath 呼び出し側が指定したパス。
// @param Resolved 解決した絶対パスの格納先。
bool FAssetPathResolver::Resolve(const FString& LogicalPath, FPath& Resolved) const
{
	if (!m_bSet || LogicalPath.IsEmpty() || HasNul_Internal(LogicalPath) || !IsValidUtf8_Internal(LogicalPath))
	{
		return false;
	}
	// 区切りを揃えた入力。
	FString Normalized = FPath(LogicalPath).Normalize().ToUtf8();
	if (Normalized.IsEmpty())
	{
		return false;
	}
	if (IsFullyQualified_Internal(Normalized))
	{
		Resolved = FPath(Move(Normalized));
		return true;
	}
	if (Normalized[0] == '/' || HasDrivePrefix_Internal(Normalized))
	{
		return false;
	}
	// Rootと結合して正規化した候補。
	FString Joined = m_Root.ToUtf8() + "/" + Normalized;
	FString Candidate = FPath(Move(Joined)).Normalize().ToUtf8();
	// 比較用のRoot文字列。
	const FString& RootText = m_Root.ToUtf8();
	if (Candidate == RootText)
	{
		Resolved = FPath(Move(Candidate));
		return true;
	}
	if (Candidate.Size() > RootText.Size() && Candidate.Substr(0, RootText.Size()) == RootText &&
	    Candidate[RootText.Size()] == '/')
	{
		Resolved = FPath(Move(Candidate));
		return true;
	}
	return false;
}
} // namespace Toolbox
