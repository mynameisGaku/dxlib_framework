// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_INSTANCE_2D_H
#define DXF_CHECKPOINT_INSTANCE_2D_H
#include "Dxf/PrefabSpawnOptions2D.h"
#include "Dxf/ContentParameterValue.h"
namespace Dxf
{
/**
 * 再開時に配置する論理個体。実行中BodyのPoseや世代を保存する値ではない。
 */
struct FCheckpointInstance2D
{
	/**
	 * Scene内で一意なASCII個体キー。生成順は文書の配列順で保持する。
	 */
	Toolbox::FString Key;
	/**
	 * ゲームが許可したPrefabカタログのキー。ファイルパスではない。
	 */
	Toolbox::FString Prefab;
	/**
	 * この個体の準備に実際に使った定義バイトのSHA-256集約値。
	 */
	Toolbox::FString DefinitionFingerprint;
	/**
	 * falseの個体を再開時に復活させない。
	 */
	bool bPresent = true;
	/**
	 * Scene初期個体ではなく、明示Spawnした個体か。
	 */
	bool bDynamic = false;
	/**
	 * 再開レシピが選んだ安全な初期配置。剛体変換だけ。
	 */
	FPrefabSpawnOptions2D Placement;
	/**
	 * 個体だけの検証済み公開値。共有定義を変更しない。
	 */
	Toolbox::TVector<FContentParameterValue> Parameters;
};
} // namespace Dxf
#endif
