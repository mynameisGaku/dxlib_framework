// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_PLATFORM_H
#define TOOLBOX_PLATFORM_H
#include "Toolbox/String.h"
namespace Toolbox
{
/**
 * 単調増加する時刻をナノ秒で返す。壁時計の変更には影響されない。
 */
uint64 MonotonicNanoseconds();
/**
 * UTF-8文字列としてパスを保持する。正規化はファイルへアクセスしない。
 */
class FPath
{
public:
	/**
	 * 空のパスを作る。
	 */
	FPath() = default;
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Text 読み取る文字列。
	 */
	FPath(const char* Text) : m_Text(Text)
	{
	}
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Text 読み取る文字列。
	 */
	FPath(const char8_t* Text) : m_Text(reinterpret_cast<const char*>(Text))
	{
	}
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Text 読み取る文字列。
	 */
	FPath(FString Text) : m_Text(Move(Text))
	{
	}
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Text 読み取る文字列。
	 */
	FPath(const wchar_t* Text);
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Text 読み取る文字列。
	 */
	FPath(const FWideString& Text);
	/**
	 * 所有しているUTF-8パスを変更せずに返す。
	 */
	FORCEINLINE const FString& ToUtf8() const noexcept
	{
		return m_Text;
	}
	/**
	 * パスの区切りや相対成分を整理する。ファイルへはアクセスしない。
	 */
	FPath Normalize() const;
	/**
	 * 最後のパス要素を除いた親ディレクトリを返す。
	 */
	FPath Parent() const;
	/**
	 * 二つのパスを結合する。右側が絶対パスなら右側を返す。
	 * @param Left 結合元のパス。
	 * @param Right 追加するパス。
	 */
	friend FPath operator/(const FPath& Left, const FPath& Right);

private:
	/**
	 * UTF-8で保持するパス文字列。
	 */
	FString m_Text;
};
/**
 * Windows APIへ渡すUTF-16文字列へ変換する。不正なUTF-8は例外で通知する。
 * @param Text 読み取る文字列。
 */
FWideString ToWide(const FString& Text);
/**
 * ワイド文字列をUTF-8へ変換する。不正な入力は例外で通知する。
 * @param Text 読み取る文字列。
 */
FString FromWide(const FWideString& Text);
/**
 * OSから現在の作業ディレクトリを取得する。
 */
FPath CurrentDirectory();
/**
 * OSの一時ディレクトリを取得する。
 */
FPath TemporaryDirectory();
/**
 * 新しいディレクトリを作る。既存ならfalse、他の失敗は例外を返す。
 * @param Path 操作対象のパス。
 */
bool CreateDirectory(const FPath& Path);
/**
 * 指定パスが通常のファイルを指しているか調べる。
 * @param Path 操作対象のパス。
 */
bool IsRegularFile(const FPath& Path);
/**
 * ファイルを新規コピーする。既存のコピー先は上書きしない。
 * @param Source 読み取り元のパス。
 * @param Destination 新規作成するコピー先のパス。
 */
void CopyFile(const FPath& Source, const FPath& Destination);
/**
 * 指定ツリーを削除し、失敗コードを返す。シンボリックリンク先は辿らない。
 * @param Path 操作対象のパス。
 * @param Error 失敗時のエラーコード。成功時は0。
 */
void RemoveTree(const FPath& Path, int32& Error) noexcept;
/**
 * テスト用の再現可能な疑似乱数。暗号用途には使用しない。
 */
class FRandom
{
public:
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Seed 疑似乱数列の初期値。
	 */
	explicit FRandom(uint32 Seed) : m_State(Seed ? Seed : 1)
	{
	}
	/**
	 * 内部状態を進め、次の32ビット疑似乱数を返す。
	 */
	uint32 operator()() noexcept
	{
		m_State ^= m_State << 13;
		m_State ^= m_State >> 17;
		m_State ^= m_State << 5;
		return m_State;
	}

private:
	/**
	 * 次の疑似乱数を生成するための内部状態。
	 */
	uint32 m_State;
};
/**
 * Cランタイムの標準出力または標準エラーへ値を書き出す。
 */
class FConsole
{
public:
	/**
	 * 書き込み先を標準出力または標準エラーから選ぶ。
	 * @param Error trueなら標準エラー、falseなら標準出力。
	 */
	explicit FConsole(bool Error) : m_bError(Error)
	{
	}
	/**
	 * 値を指定した出力先へ書き出す。
	 */
	const FConsole& operator<<(const char* Text) const
	{
		fputs(Text, m_bError ? stderr : stdout);
		return *this;
	}
	/**
	 * 値を指定した出力先へ書き出す。
	 */
	const FConsole& operator<<(const FString& Text) const
	{
		fwrite(Text.Data(), 1, Text.Size(), m_bError ? stderr : stdout);
		return *this;
	}
	/**
	 * 値を指定した出力先へ書き出す。
	 */
	const FConsole& operator<<(char Character) const
	{
		fputc(Character, m_bError ? stderr : stdout);
		return *this;
	}
	/**
	 * 値を指定した出力先へ書き出す。
	 */
	template <typename T> const FConsole& operator<<(T Value) const
	{
		return *this << ToString(Value);
	}

private:
	/**
	 * 標準エラーへ出力するかどうか。
	 */
	bool m_bError;
};
/**
 * テスト結果を書き出す標準出力。
 */
inline const FConsole Out{false};
/**
 * 失敗の診断を書き出す標準エラー。
 */
inline const FConsole Err{true};
} // namespace Toolbox
#endif
