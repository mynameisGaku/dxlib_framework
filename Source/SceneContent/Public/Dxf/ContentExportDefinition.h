// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_EXPORT_DEFINITION_H
#define DXF_CONTENT_EXPORT_DEFINITION_H
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 公開先の型。BodyとJointと資源を混同しない。
 */
enum class EContentExportKind : Toolbox::uint8
{
	/**
	 * 剛体Component。
	 */
	RigidBody,
	/**
	 * 移動支点Component。
	 */
	KinematicMover,
	/**
	 * 距離Joint Component。
	 */
	Distance,
	/**
	 * 回転Joint Component。
	 */
	Revolute,
	/**
	 * 固定Joint Component。
	 */
	Fixed,
	/**
	 * 直動Joint Component。
	 */
	Prismatic,
	/**
	 * 読み取り専用の画像。
	 */
	Texture,
	/**
	 * 読み取り専用のモデル。
	 */
	Model,
	/**
	 * イベントから再生する音。
	 */
	Sound,
	/**
	 * 読み取り専用のフォント。
	 */
	Font,
	/**
	 * Bodyの接触／Sensor通知のComponent。
	 */
	Sensor
};
/**
 * 定義内の型付き公開先。実行時には同じindexのhandleを取得する。
 */
struct FContentExportDefinition
{
	/**
	 * 公開名。
	 */
	Toolbox::FString Id;
	/**
	 * 公開先の型。
	 */
	EContentExportKind Kind = EContentExportKind::RigidBody;
	/**
	 * 部品、Jointまたは資源表の解決済みindex。
	 */
	Toolbox::uint32 Index = 0;
};
} // namespace Dxf
#endif
