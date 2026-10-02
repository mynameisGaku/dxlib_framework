// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_ESAVEDOCUMENTKIND_H
#define DXF_ESAVEDOCUMENTKIND_H
namespace Dxf
{
/**
 * 進行とユーザー設定を別系列へ保存するための識別。
 */
enum class ESaveDocumentKind
{
	/**
	 * チェックポイントから再開する論理進行。
	 */
	Progress,
	/**
	 * 現在の操作環境。進行読込で巻き戻さない。
	 */
	Settings
};
} // namespace Dxf
#endif
