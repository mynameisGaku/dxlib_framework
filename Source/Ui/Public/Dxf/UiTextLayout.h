// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_TEXT_LAYOUT_H
#define DXF_UI_TEXT_LAYOUT_H
#include "Dxf/SharedText.h"
#include "Dxf/UiTextService.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 行の折返し。
 */
enum class EUiTextWrap : Toolbox::uint8
{
	/**
	 * 明示した改行だけで行を分ける。
	 */
	NoWrap,
	/**
	 * 幅に収まるように折り返す（ASCIIの語は空白で、それ以外は文字の境界で分ける）。
	 */
	Wrap
};

/**
 * 収まらない文字の扱い。
 */
enum class EUiTextOverflow : Toolbox::uint8
{
	/**
	 * そのまま描く（はみ出しは要素のクリップに従う）。
	 */
	Visible,
	/**
	 * 行の末尾を省略記号（…）に置き換えて幅に収める。行数の上限を超えた最後の行にも付ける。
	 */
	Ellipsis
};

/**
 * 文字の配置の指定（画素単位。論理単位からの変換は呼出し側が行う）。
 */
struct FUiTextLayoutRequest
{
	/**
	 * 行の最大幅（画素）。負は上限なし。
	 */
	Toolbox::int32 MaxWidth = -1;
	/**
	 * 折返し。
	 */
	EUiTextWrap Wrap = EUiTextWrap::NoWrap;
	/**
	 * 収まらない文字の扱い。
	 */
	EUiTextOverflow Overflow = EUiTextOverflow::Visible;
	/**
	 * 行数の上限（0は上限なし）。
	 */
	Toolbox::uint32 MaxLines = 0;
};

/**
 * 配置した一行。
 */
struct FUiTextLine
{
	/**
	 * 描く文字列（省略記号を含む）。配置のときに一度だけ作り、描画命令は複製せずに共有する。
	 */
	FSharedText Text;
	/**
	 * 描く文字列の幅（画素、描画と同じ計測）。
	 */
	Toolbox::int32 Width = 0;
};

/**
 * 配置の結果。
 */
struct FUiTextLayoutResult
{
	/**
	 * 行（上から順）。
	 */
	Toolbox::TVector<FUiTextLine> Lines;
	/**
	 * 行の送り（画素）。
	 */
	Toolbox::int32 LineHeight = 0;
	/**
	 * 最も広い行の幅（画素）。
	 */
	Toolbox::int32 Width = 0;
	/**
	 * 全体の高さ（画素）。
	 */
	Toolbox::int32 Height = 0;
	/**
	 * 省略・行数の上限で文字を落としたか。
	 */
	bool bTruncated = false;
	/**
	 * 最大幅を超える行があるか（Visibleで収まらない、または一文字も入らない）。
	 */
	bool bOverflowed = false;
	/**
	 * 文字の計測を呼んだ回数。
	 */
	Toolbox::uint32 Measurements = 0;
};

/**
 * 文字列を行へ配置する。計測はすべて指定したフォントで行い、描画と同じ幅になる。
 * UTF-8として不正な文字列は失敗する。
 * @param Service 計測の窓口。
 * @param Font 描画と同じフォント。
 * @param Text UTF-8の文字列（'\n'で改行）。
 * @param Request 配置の指定。
 */
TResult<FUiTextLayoutResult> LayoutUiText(IUiTextService& Service, const FFont& Font, const Toolbox::FString& Text,
                                          const FUiTextLayoutRequest& Request);
} // namespace Dxf
#endif
