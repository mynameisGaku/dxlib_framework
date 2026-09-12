#pragma once
#include "Dxf/AssetBackend.h"
namespace Dxf
{
/**
 * DxLibによる音声リソースと再生を管理する型。
 */
class FDxLibSoundBackend final : public ISoundBackend
{
public:
	/**
	 * 音声ファイルを読み込む。
	 * @param Path 読み込むファイルのパス。
	 * @param Options 処理に適用する設定。
	 */
	TResult<Toolbox::int32> LoadSound(const Toolbox::FString& Path, const FSoundLoadOptions& Options) override;
	/**
	 * 独立して再生できる音声ハンドルを複製する。
	 * @param Handle ハンドル。
	 */
	TResult<Toolbox::int32> DuplicateSound(Toolbox::int32 Handle) override;
	/**
	 * ネイティブ音声の再生を開始する。
	 * @param Handle ハンドル。
	 * @param bLoop 繰り返し再生するか。
	 */
	TResult<void> StartSound(Toolbox::int32 Handle, bool bLoop) override;
	/**
	 * ネイティブ音声の再生を停止する。
	 * @param Handle ハンドル。
	 */
	void StopSound(Toolbox::int32 Handle) noexcept override;
	/**
	 * ネイティブ音声を解放する。
	 * @param Handle ハンドル。
	 */
	void DeleteSound(Toolbox::int32 Handle) noexcept override;
	/**
	 * ネイティブ音声の音量を設定する。
	 * @param Handle ハンドル。
	 * @param Volume 再生音量。
	 */
	TResult<void> SetSoundVolume(Toolbox::int32 Handle, Toolbox::f32 Volume) override;
	/**
	 * ネイティブ音声が再生中かを調べる。
	 * @param Handle ハンドル。
	 */
	TResult<bool> IsSoundPlaying(Toolbox::int32 Handle) override;
};
} // namespace Dxf
