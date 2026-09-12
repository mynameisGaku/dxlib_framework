#pragma once
#include "Dxf/Scene.h"
#include "Dxf/AudioPlayer.h"
namespace Dxf
{
/**
 * シーン開始と終了の通知順序を管理する型。
 */
class FSceneLifecycle
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Audio 音声再生のサービス。
	 */
	explicit FSceneLifecycle(FAudioPlayer& Audio) : m_pAudio(&Audio)
	{
	}
	/**
	 * 有効化する前に初期化を完了させる。
	 * @param Scene 対象のシーン。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> Prepare_Internal(DScene& Scene, const FInitContext& Context)
	{
		return Scene.Initialize_Internal(Context);
	}
	/**
	 * 準備した対象を有効にする。
	 * @param Scene 対象のシーン。
	 * @param Context 処理に必要な実行環境。
	 */
	void Activate_Internal(DScene& Scene, const FSceneActivationContext& Context) noexcept
	{
		Scene.Enter_Internal(Context);
	}
	/**
	 * 対象の音声再生を停止する。
	 * @param Scene 対象のシーン。
	 */
	void Stop_Internal(DScene& Scene) noexcept
	{
		Scene.Exit_Internal();
		Scene.Shutdown_Internal();
		// 終了フックが開始した音声も残さないよう、利用者の後始末後にも停止する。
		if (Scene.GetAudioScope() != 0)
		{
			m_pAudio->StopScope(Scene.GetAudioScope());
		}
	}

private:
	/**
	 * 音声再生のサービス。
	 */
	FAudioPlayer* m_pAudio;
};
} // namespace Dxf
