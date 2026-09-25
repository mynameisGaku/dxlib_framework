// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_STYLE_PARSER_H
#define DXF_UI_STYLE_PARSER_H
#include "Dxf/UiStyleSheet.h"
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * スタイルの資源の一つ（名前は失敗の位置の表示に使う）。
 */
struct FUiStyleSource
{
	/**
	 * 資源の名前（相対パス等）。
	 */
	Toolbox::FString Name;
	/**
	 * 内容（UTF-8）。
	 */
	Toolbox::FString Text;
};

/**
 * 読込の上限。
 */
struct FUiStyleLimits
{
	/**
	 * 一つの資源のバイト数の上限。
	 */
	Toolbox::size_t MaxSourceBytes = 1024 * 1024;
	/**
	 * スタイルIDの数の上限。
	 */
	Toolbox::size_t MaxStyles = 4096;
	/**
	 * トークンの数の上限。
	 */
	Toolbox::size_t MaxTokens = 4096;
	/**
	 * トークンの参照の深さの上限。
	 */
	Toolbox::size_t MaxTokenDepth = 32;
};

/**
 * スタイルの資源を読む。書式（版1）:
 *   dxfui-style 1                 先頭の行（コメント・空行を除く）。版の宣言。
 *   source "Buttons.dxfui"        （集約済みの資源だけ）以後の行の元の資源名。行番号も1から数え直す。
 *   token 名前 = 値               共通トークン（色 #RRGGBB / #RRGGBBAA、数値、または @別のトークン）。
 *   default { ... }               スタイルIDのない要素の定義。
 *   style ID { ... }              スタイルIDの定義。{ と } はそれぞれ行末・単独の行に置く。
 *     キー = 値                   キー: background, foreground, border-color, border-width, font-family,
 *                                 font-size, padding（1・2・4個の数値）, opacity。
 *     hover.キー / pressed.キー / focus.キー / disabled.キー = 値   状態ごとの上書き。
 *   # で始まる行はコメント。
 * 重複したID・トークン、未定義のトークン、トークンの循環、型の違い、未知のキー、不正な数値、上限の超過は、
 * 資源名・行・キーを示して失敗する。複数の資源は順に読み、IDとトークンは全体で一意であること。
 * @param Sources 資源（読む順）。
 * @param Limits 上限。
 */
TResult<FUiStyleSheet> ParseUiStyleSheet(const Toolbox::TVector<FUiStyleSource>& Sources,
                                         const FUiStyleLimits& Limits = {});
/**
 * 一つの資源を読む。
 * @param Name 資源名。
 * @param Text 内容。
 * @param Limits 上限。
 */
TResult<FUiStyleSheet> ParseUiStyleSheet(const Toolbox::FString& Name, const Toolbox::FString& Text,
                                         const FUiStyleLimits& Limits = {});
} // namespace Dxf
#endif
