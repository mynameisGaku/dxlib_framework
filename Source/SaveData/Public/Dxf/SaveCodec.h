// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVECODEC_H
#define DXF_SAVECODEC_H
#include "Dxf/SaveDocument.h"
#include "Dxf/SaveLimits.h"
#include "Dxf/SavePayloadMigration.h"
namespace Dxf
{
/**
 * 既存JSON読解器を使い、バージョン・厳密整数・封筒を検証する純粋codec。
 */
class FSaveCodec
{
public:
	/**
	 * @param Document 論理保存値。@param Limits 入力・出力の有限上限。失敗は例外。
	 */
	static Toolbox::FString Encode(const FSaveDocument& Document, FSaveLimits Limits = {});
	/**
	 * @param Bytes 長さ付きUTF-8。@param Limits 確保前の上限。未知field・将来バージョンは拒否する。
	 */
	static FSaveDocument Decode(Toolbox::FStringView Bytes, FSaveLimits Limits = {});
	/**
	 * @param Document 読込値。@param Expected 信頼するゲーム・Content・Scene・次元・payloadバージョン。
	 * 世代・checkpoint・payloadは一致条件にしない。移行後の値を再検証してから呼ぶ。
	 */
	static void RequireCompatible(const FSaveDocument& Document, const FSaveDocument& Expected);
	/**
	 * @param Document 読込値。@param Expected 信頼する識別と採用バージョン。
	 * @param Migration 明示された一段階の純粋なゲーム変換。@param Limits 変換後にも適用する上限。
	 * ゲーム・Content不一致は変換前に拒否する。世代やScene識別を変えず、元ファイルを変更しない。
	 */
	static FSaveDocument Migrate(const FSaveDocument& Document, const FSaveDocument& Expected, const FSavePayloadMigration& Migration = {}, FSaveLimits Limits = {});
};
} // namespace Dxf
#endif
