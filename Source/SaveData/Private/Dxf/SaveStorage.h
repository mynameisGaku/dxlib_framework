// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVESTORAGE_PRIVATE_H
#define DXF_SAVESTORAGE_PRIVATE_H
#include "Dxf/SaveStore.h"
namespace Dxf
{
// 登録前に保存領域とslotを検査する共通処理。
void ValidateSaveLocation(const Toolbox::FPath& Root, Toolbox::FStringView Slot);
// Windowsのファイル名と一致させるslotの正準表現。他の論理キーは変更しない。
Toolbox::FString CanonicalSaveSlot(Toolbox::FStringView Slot);
// 最新正常面を読み、更新先の反対面を返す。呼出し側が排他を持つ。
FSaveLoadResult ReadSaveFaces(const Toolbox::FPath& Root, Toolbox::FStringView Slot, FSaveLimits Limits, Toolbox::FPath* NextFace);
// paddingを含まない明示的な面表現を作る。
Toolbox::FString EncodeSaveFace(const FSaveDocument& Document, FSaveLimits Limits);
} // namespace Dxf
#endif
