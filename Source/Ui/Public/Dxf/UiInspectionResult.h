// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_INSPECTION_RESULT_H
#define DXF_UI_INSPECTION_RESULT_H
#include "Dxf/ObjectHandle.h"
#include "Dxf/UiSurface.h"
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
	 * ルート内で一意の要素番号（作成順、再使用しない）。0は要素に対応しない問題。
	 */
	Toolbox::uint64 ElementId = 0;
	/**
	 * 要素の参照の番号（添字と世代）。要素が破棄・再利用された後はRootで解決できない。
	 */
	FObjectId Source;
	/**
	 * 検査した表示先（Hostの表示先の番号。Hostを通さない検査は0）。
	 */
	Toolbox::uint64 DisplayId = 0;
	/**
	 * ClipFitの対象の描画項目の添字（それ以外は最大値）。
	 */
	Toolbox::size_t DrawItemIndex = Toolbox::TNumericLimits<Toolbox::size_t>::Max();
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
	 * 検査した表示先（Hostを通さない検査は0）。
	 */
	Toolbox::uint64 DisplayId = 0;
	/**
	 * 検査時の表示面の画素の範囲。
	 */
	FUiPixelRect SurfacePixels;
	/**
	 * 検査時の表示面の倍率（画素/論理単位）。
	 */
	Toolbox::f32 Scale = 0;
	/**
	 * 検査時の表示面の論理寸法。
	 */
	FUiSize LogicalSize;
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
/**
 * 検査する表示先の情報（Hostが表示先ごとに渡す）。
 */
struct FUiInspectionTarget
{
	/**
	 * 表示先の番号（結果と各問題へ写す）。
	 */
	Toolbox::uint64 DisplayId = 0;
	/**
	 * 表示先が命令へ追加で掛けるクリップ（論理単位。空なら表示面全体）。
	 */
	FUiRect DisplayClip;
};
} // namespace Dxf
#endif
