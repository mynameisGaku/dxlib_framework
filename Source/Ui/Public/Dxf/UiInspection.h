// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_INSPECTION_H
#define DXF_UI_INSPECTION_H
#include "Dxf/UiRoot.h"
#include "Dxf/UiInspectionResult.h"
namespace Dxf
{
/** レイアウト計算済みの木を読み取り、文字・子配置・同階層の重なり・画像解像度を検査する。
 * 画素やGPUへ問い合わせず、変更・Layout・イベント配送と同時に呼ばない。
 * @param Root 計算済みのRoot。実フォントによる計測を要求する場合はそのServiceを設定する。
 * @param DrawList 任意の採取済み描画命令。指定時はClipFitも検査する：各命令のクリップを、命令を出した要素の
 * 期待するクリップ（表示面・表示先のクリップ・切り抜く祖先の矩形の共通部分を木から独立に求めたもの）と比べる。
 * 検査自体はOnDrawを呼ばない。
 * @param MaxIssues 保持する問題数の上限。超過は例外で、部分結果は返さない。
 * @param Target 表示先の番号と追加のクリップ（Hostが表示先ごとに渡す）。
 */
FUiInspectionResult InspectUiLayout(FUiRoot& Root, const FUiDrawList* DrawList = nullptr,
                                    Toolbox::size_t MaxIssues = 4096, const FUiInspectionTarget& Target = {});
} // namespace Dxf
#endif
