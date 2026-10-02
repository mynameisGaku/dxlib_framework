// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_USER_SETTINGS_CODEC_H
#define DXF_USER_SETTINGS_CODEC_H
#include "Dxf/UserSettings.h"
#include "Dxf/InputProfilePolicy.h"
#include "Dxf/SaveDocument.h"
namespace Dxf
{
/**
 * 設定だけの保存封筒を作る。進行のSceneやcheckpointを設定へ混ぜない。
 */
class FUserSettingsCodec
{
public:
	/**
	 * @param Settings 永続化する値。
	 * @param Policy 入力割当の検証条件。
	 * @param GameId 信頼するゲーム識別。
	 * @param GameVersion ゲームが指定する設定互換バージョン。
	 * 返す文書はSettings種類・次元0で、Contentと進行キーは空。Storeが世代を付ける。
	 */
	static FSaveDocument Encode(const FUserSettings& Settings, const FInputProfilePolicy& Policy, Toolbox::FString GameId, Toolbox::uint32 GameVersion = 1);
	/**
	 * @param Document 完全性確認後の文書。
	 * @param Policy 許可割当。
	 * @param GameId 信頼する識別。
	 * @param GameVersion 採用する設定互換バージョン。
	 * 全検証後に値を返す。現在の入力・音量・UIを変更せず、失敗時は旧設定を保持できる。
	 */
	static FUserSettings Decode(const FSaveDocument& Document, const FInputProfilePolicy& Policy, Toolbox::FString GameId, Toolbox::uint32 GameVersion = 1);
};
} // namespace Dxf
#endif
