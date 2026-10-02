// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_INPUT_PROFILE_CODEC_H
#define DXF_INPUT_PROFILE_CODEC_H
#include "Dxf/InputProfile.h"
#include "Dxf/InputProfilePolicy.h"
#include "Dxf/InputMap.h"
namespace Dxf
{
/**
 * OS入力を取得せず、安定したボタン名で完全プロファイルを検証・保存・適用する。
 */
class FInputProfileCodec
{
public:
	/**
	 * @param Profile 履歴を含まない全割当。
	 * @param Policy 許可名・予約・上限。
	 * 未知名、重複、Action間競合、予約入力、不正上限は例外。ゲーム状態に触れない。
	 */
	static void Validate(const FInputProfile& Profile, const FInputProfilePolicy& Policy);
	/**
	 * @param Profile 全割当。
	 * @param Policy 検証条件。Actionと割当を正準順序で出力する。
	 */
	static Toolbox::FString Encode(const FInputProfile& Profile, const FInputProfilePolicy& Policy);
	/**
	 * @param Bytes 長さ付きUTF-8。
	 * @param Policy 全体適用前の検証条件。
	 * 未知field・将来バージョン・整数の小数表記を拒否する。現Mapを変更しない。
	 */
	static FInputProfile Decode(Toolbox::FStringView Bytes, const FInputProfilePolicy& Policy);
	/**
	 * @param Profile 完全プロファイル。
	 * @param Policy 許可条件。
	 * @param Map 採用先。
	 * @param Current UIで消費する前のSnapshot。検証・確保失敗では現Mapを保持し、採用後は解放を待つ。
	 */
	static void Apply(const FInputProfile& Profile, const FInputProfilePolicy& Policy, FInputMap& Map, const FInputSnapshot& Current);
};
} // namespace Dxf
#endif
