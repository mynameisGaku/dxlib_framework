// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVE_PAYLOAD_MIGRATION_H
#define DXF_SAVE_PAYLOAD_MIGRATION_H
#include "Toolbox/Function.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * ゲームが明示する一段階のpayload移行。OS・World・Sceneへの副作用を持たせない。
 */
struct FSavePayloadMigration
{
	/**
	 * ゲームが読解・検証できる移行元のバージョン。0は移行なし。
	 */
	Toolbox::uint32 FromVersion = 0;
	/**
	 * ゲームが採用する移行先のバージョン。
	 */
	Toolbox::uint32 ToVersion = 0;
	/**
	 * 移行元を型付きで検証し、移行先の検証済みJSON Objectを返す純粋処理。
	 * Workerでも実行するため、Scene参照やNative資源を捕捉しない。
	 */
	Toolbox::TFunction<Toolbox::FString(Toolbox::FStringView)> Convert;
};
} // namespace Dxf
#endif
