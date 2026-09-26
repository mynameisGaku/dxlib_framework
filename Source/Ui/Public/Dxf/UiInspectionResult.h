// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_INSPECTION_RESULT_H
#define DXF_UI_INSPECTION_RESULT_H
#include "Dxf/UiTypes.h"
#include "Toolbox/String.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * レイアウト検査の種類。
 */
enum class EUiInspectionKind : Toolbox::uint8
{
	TextFit,
	ClipFit,
	LayoutFit,
	Overlap,
	ImageResolution,
	UnknownStyle,
	InvalidLayout
};
/**
 * 表示不足は警告、計算できない配置はエラー。
 */
enum class EUiInspectionSeverity : Toolbox::uint8
{
	Warning,
	Error
};
/**
 * 検査時点の値。要素への所有参照は持たない。
 */
struct FUiInspectionIssue
{
	/**
	 * 検査種別。
	 */
	EUiInspectionKind Kind = EUiInspectionKind::LayoutFit;
	/**
	 * 深刻度。
	 */
	EUiInspectionSeverity Severity = EUiInspectionSeverity::Warning;
	/**
	 * ルート内で一意の要素番号。
	 */
	Toolbox::uint64 ElementId = 0;
	/**
	 * 名前と兄弟番号によるツリー上の位置。
	 */
	Toolbox::FString Path;
	/**
	 * 比較の基準矩形（論理単位）。
	 */
	FUiRect Expected;
	/**
	 * 実配置・測定矩形（論理単位）。
	 */
	FUiRect Actual;
	/**
	 * 不一致の理由。
	 */
	Toolbox::FString Reason;
};
/**
 * 省略なしの検査結果。上限超過時は成功として切り詰めず例外にする。
 */
struct FUiInspectionResult
{
	/**
	 * 見えない・Collapsedの要素を除いた検査数。
	 */
	Toolbox::size_t Elements = 0;
	/**
	 * 理由付き宣言で対象外にした検査数。
	 */
	Toolbox::size_t DeclaredExemptions = 0;
	/**
	 * 見つかった問題。
	 */
	Toolbox::TVector<FUiInspectionIssue> Issues;
};
} // namespace Dxf
#endif
