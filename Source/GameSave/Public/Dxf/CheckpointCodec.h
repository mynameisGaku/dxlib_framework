// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_CODEC_H
#define DXF_CHECKPOINT_CODEC_H
#include "Dxf/CheckpointDocument2D.h"
#include "Dxf/CheckpointDocument3D.h"
#include "Dxf/CheckpointLimits.h"
namespace Dxf
{
/**
 * 両次元の論理チェックポイントと保存封筒を変換する。Worldやファイルを変更しない。
 */
class FCheckpointCodec
{
public:
	/**
	 * @param Document 安全な採取境界で完成した値。実体IDは持たない。
	 * @param Limits 出力・個体・状態・パラメーターの上限。不正は例外。
	 */
	static FSaveDocument Encode(const FCheckpointDocument2D& Document, FCheckpointLimits Limits = {});
	/**
	 * @param Document 3Dの配置と論理値。Quaternionは有限・非ゼロを検査し正規化する。
	 * @param Limits 出力・個体・状態・パラメーターの上限。
	 */
	static FSaveDocument Encode(const FCheckpointDocument3D& Document, FCheckpointLimits Limits = {});
	/**
	 * @param Document 保存コアの封筒。次元・構造・参照・値を検証する。
	 * @param Limits 確保前上限。未知のチェックポイントバージョンは拒否する。
	 */
	static FCheckpointDocument2D Decode2D(const FSaveDocument& Document, FCheckpointLimits Limits = {});
	/**
	 * @param Document 保存コアの封筒。元の文書を変更しない。
	 * @param Limits 確保前上限。実Body登録やゲーム固有進行の検証は後続の再開処理。
	 */
	static FCheckpointDocument3D Decode3D(const FSaveDocument& Document, FCheckpointLimits Limits = {});
};
} // namespace Dxf
#endif
