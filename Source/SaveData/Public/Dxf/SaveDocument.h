// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVEDOCUMENT_H
#define DXF_SAVEDOCUMENT_H
#include "Dxf/ESaveDocumentKind.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 実行時IDを持たない保存封筒。payloadの意味・移行・個体上限はゲームcodecが検証する。
 */
struct FSaveDocument
{
	/**
	 * 保存形式のバージョン。初バージョンは1のみ。
	 */
	Toolbox::uint32 FormatVersion = 1;
	/**
	 * 進行と設定の区別。
	 */
	ESaveDocumentKind Kind = ESaveDocumentKind::Progress;
	/**
	 * ゲームが明示するASCII識別子。
	 */
	Toolbox::FString GameId;
	/**
	 * ゲームpayloadのバージョン。保存形式バージョンとは独立。
	 */
	Toolbox::uint32 GameVersion = 1;
	/**
	 * 準備に実際に使ったContentを識別するゲーム側の不変revision。
	 */
	Toolbox::FString ContentRevision;
	/**
	 * 許可カタログで解決するScene論理キー。設定では空。
	 */
	Toolbox::FString SceneKey;
	/**
	 * 進行は2または3、設定は0。
	 */
	Toolbox::uint32 Dimension = 2;
	/**
	 * 安全な再開地点の論理キー。設定では空。
	 */
	Toolbox::FString CheckpointKey;
	/**
	 * 浮動小数点へ変換しない保存世代。Storeが次世代を割り当てる。
	 */
	Toolbox::uint64 Generation = 0;
	/**
	 * 型付きゲームcodecが作るJSON Object。不変のバイトとしてWorkerへ渡せる。
	 */
	Toolbox::FString Payload = "{}";
};
} // namespace Dxf
#endif
